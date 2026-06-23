#!/usr/bin/env python3
"""
war1_convert.py — convert an 8-bit indexed PNG to a .pc8 blob.

Format (v2, stores the raw image; frame slicing happens at blit time):

    offset  type   field
    0       u32    magic = 0x20384350 ("PC8 ")
    4       u16    image_w (pixels)
    6       u16    image_h (pixels)
    8       u16    version = 2
    10      u16    reserved (0)
    12      img_w * img_h bytes   indexed pixels, row-major
    N       256 × u32 ARGB8888    palette (color 0 = transparent by convention)

Usage:
    scripts/war1_convert.py INPUT.png OUTPUT.pc8

Notes:
- The PNG must be PNG8 (8-bit colormap). War1gus's extracted PNGs already are.
- Color 0 is treated as transparent in the palette (alpha=0). Other entries
  get alpha=0xFF.
"""
import struct, sys, zlib, pathlib

PC8_MAGIC = 0x20384350  # "PC8 "

def _read_truecolor_via_pil(path):
    """RGBA-PNG fallback. Quantizes by building a per-image unique-color
    palette (cap 256). Idx 0 is reserved for fully-transparent. Anything
    above 256 unique colors collapses to transparent. Used for the
    hand-prescaled HUD assets that ship as RGBA, not indexed."""
    from PIL import Image
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    px = list(img.getdata())
    seen = {(0, 0, 0, 0): 0}
    palette = [0]
    out = bytearray(w * h)
    for i, p in enumerate(px):
        r, g, b, a = p
        key = (r, g, b, a) if a > 0 else (0, 0, 0, 0)
        if key not in seen:
            if len(palette) >= 256:
                key = (0, 0, 0, 0)
            else:
                seen[key] = len(palette)
                palette.append((a << 24) | (r << 16) | (g << 8) | b)
        out[i] = seen[key]
    while len(palette) < 256:
        palette.append(0)
    return w, h, bytes(out), palette

# ---- minimal 8-bit-indexed PNG reader -----------------------------------
def read_png8(path):
    data = pathlib.Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    ihdr = None
    plte = None
    trns = b""
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos+4])[0]
        ctype = data[pos+4:pos+8]
        body = data[pos+8:pos+8+length]
        pos += 8 + length + 4  # skip crc
        if ctype == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", body)
        elif ctype == b"PLTE":
            plte = body
        elif ctype == b"tRNS":
            trns = body
        elif ctype == b"IDAT":
            idat += body
        elif ctype == b"IEND":
            break
    if ihdr is None:
        raise ValueError(f"{path}: missing IHDR")
    if plte is None:
        # No palette = truecolor PNG. Fall back to PIL to quantize:
        # build a 256-entry palette from unique RGBA tuples (transparent
        # → idx 0). HUD assets we hand-prescaled are RGBA truecolor and
        # use few enough distinct colors to fit cleanly.
        return _read_truecolor_via_pil(path)
    w, h, bit_depth, color_type, comp, filt, interlace = ihdr
    if color_type != 3 or interlace != 0 or bit_depth not in (1, 2, 4, 8):
        raise ValueError(f"{path}: need indexed (1/2/4/8 bit) non-interlaced PNG "
                         f"(got depth={bit_depth} ctype={color_type} interlace={interlace})")
    raw = zlib.decompress(bytes(idat))
    # Strip filter byte per row, apply filter. For sub-byte depths the
    # raw filtered stride is ceil(w * bit_depth / 8); we filter at byte
    # level then expand to one-byte-per-pixel after.
    pixels = bytearray()
    stride = (w * bit_depth + 7) // 8
    prev_row = bytes(stride)
    rp = 0
    for _y in range(h):
        ftype = raw[rp]; rp += 1
        row = raw[rp:rp+stride]; rp += stride
        out = bytearray(stride)
        if ftype == 0:        # None
            out[:] = row
        elif ftype == 1:      # Sub
            for i in range(stride):
                out[i] = (row[i] + (out[i-1] if i >= 1 else 0)) & 0xFF
        elif ftype == 2:      # Up
            for i in range(stride):
                out[i] = (row[i] + prev_row[i]) & 0xFF
        elif ftype == 3:      # Average
            for i in range(stride):
                a = out[i-1] if i >= 1 else 0
                b = prev_row[i]
                out[i] = (row[i] + ((a + b) >> 1)) & 0xFF
        elif ftype == 4:      # Paeth
            for i in range(stride):
                a = out[i-1] if i >= 1 else 0
                b = prev_row[i]
                c = prev_row[i-1] if i >= 1 else 0
                p = a + b - c
                pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                out[i] = (row[i] + pr) & 0xFF
        else:
            raise ValueError(f"{path}: bad filter type {ftype}")
        # Expand sub-byte depths to one byte per pixel.
        if bit_depth == 8:
            pixels += out
        else:
            mask = (1 << bit_depth) - 1
            ppb = 8 // bit_depth  # pixels per byte
            row_pixels = bytearray(w)
            for px in range(w):
                byte = out[px // ppb]
                shift = 8 - bit_depth - (px % ppb) * bit_depth
                row_pixels[px] = (byte >> shift) & mask
            pixels += row_pixels
        prev_row = bytes(out)

    # Build ARGB palette from PLTE + tRNS. Color 0 always transparent.
    palette = []
    entries = len(plte) // 3
    for i in range(256):
        if i < entries:
            r, g, b = plte[i*3], plte[i*3+1], plte[i*3+2]
        else:
            r = g = b = 0
        if i < len(trns):
            a = trns[i]
        else:
            a = 0xFF
        if i == 0:
            a = 0  # force color-0 fully transparent (War1gus convention)
        palette.append((a << 24) | (r << 16) | (g << 8) | b)
    return w, h, bytes(pixels), palette


# ---- pc8 writer ----------------------------------------------------------
def write_pc8(path, w, h, pixels, palette):
    hdr = struct.pack("<IHHHH", PC8_MAGIC, w, h, 2, 0)
    pal = struct.pack("<256I", *palette)
    if len(pixels) != w * h:
        raise ValueError(f"pixels size {len(pixels)} != w*h {w*h}")
    pathlib.Path(path).write_bytes(hdr + pixels + pal)


def main():
    if len(sys.argv) != 3:
        print("usage: war1_convert.py INPUT.png OUTPUT.pc8", file=sys.stderr)
        sys.exit(1)
    src, dst = sys.argv[1], sys.argv[2]
    w, h, pixels, palette = read_png8(src)
    write_pc8(dst, w, h, pixels, palette)
    print(f"{src} ({w}x{h}) -> {dst} ({len(pixels)} px + 1024 pal = {12 + len(pixels) + 1024} bytes)")


if __name__ == "__main__":
    main()
