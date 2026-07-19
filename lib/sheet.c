/*
 * Jupiter SDK — Spriters-Resource sprite-sheet toolkit
 *
 * ARGB sheet → console tiles: background detection, palette
 * extraction, nearest-palette quantization, integer up-scaling, and
 * tile cutting into NES / GB / SNES / Genesis formats. The bit packing
 * is the contract with lib/nes.c, lib/gb.c, lib/snes.c, lib/genesis.c
 * — see sheet.h for the per-format byte layouts.
 */
#include "sheet.h"

/* Squared-RGB proximity test (alpha ignored) */
static int color_near(uint32_t a, uint32_t b, int thresh)
{
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF)  - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF)          - (int)(b & 0xFF);
    return (dr * dr + dg * dg + db * db) < thresh * thresh;
}

int sheet_tile_bytes(sheet_fmt_t fmt)
{
    return (fmt == SHEET_FMT_NES_2BPP || fmt == SHEET_FMT_GB_2BPP) ? 16 : 32;
}

void sheet_init(sheet_t *s, const uint32_t *argb, uint32_t w, uint32_t h)
{
    s->argb = argb;
    s->w = w;
    s->h = h;
    s->bg = argb[0] | 0xFF000000;
    s->bg_thresh = 40;
}

int sheet_extract_palette(const sheet_t *s, int x, int y, int w, int h,
                          uint32_t *pal, int pal_off, int max_colors)
{
    int count = 0;
    pal[pal_off] = 0x00000000;

    /* Resume accumulation over previously extracted entries (extracted
     * colors are opaque, so non-zero; caller zeroes the palette once) */
    while (count < max_colors && pal[pal_off + 1 + count])
        count++;

    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            uint32_t c = s->argb[py * (int)s->w + px] | 0xFF000000;
            if (color_near(c, s->bg, s->bg_thresh)) continue;
            if ((c & 0x00FFFFFF) < 0x080808) continue;
            int found = 0;
            for (int j = 0; j <= count; j++)
                if (pal[pal_off + j] == c) { found = 1; break; }
            if (!found && count < max_colors) pal[pal_off + (++count)] = c;
        }
    }
    return count + 1;
}

uint8_t sheet_pal_index(const sheet_t *s, uint32_t c,
                        const uint32_t *pal, int pal_off, int pal_size)
{
    if (s->bg_thresh > 0) {
        if (color_near(c | 0xFF000000, s->bg, s->bg_thresh)) return 0;
        if ((c & 0x00FFFFFF) < 0x080808) return 0;
    }
    int cr = (int)((c >> 16) & 0xFF);
    int cg = (int)((c >> 8) & 0xFF);
    int cb = (int)(c & 0xFF);
    int best = 0, bd = 999999;
    for (int i = 0; i < pal_size; i++) {
        uint32_t p = pal[pal_off + i];
        int dr = cr - (int)((p >> 16) & 0xFF);
        int dg = cg - (int)((p >> 8) & 0xFF);
        int db = cb - (int)(p & 0xFF);
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return (uint8_t)best;
}

/* Quantize one destination pixel of the scaled rect; 0 outside it */
static uint8_t cut_px(const sheet_t *s, int x, int y, int scale,
                      int dw, int dh, int px, int py,
                      const uint32_t *pal, int pal_off, int pal_size)
{
    if (px >= dw || py >= dh) return 0;
    uint32_t c = s->argb[(y + py / scale) * (int)s->w + (x + px / scale)];
    return sheet_pal_index(s, c, pal, pal_off, pal_size);
}

/* Encode one 8x8 tile at tile coords (tx,ty) of the scaled rect */
static void cut_tile(const sheet_t *s, int x, int y, int scale,
                     int dw, int dh, int tx, int ty,
                     sheet_fmt_t fmt,
                     const uint32_t *pal, int pal_off, int pal_size,
                     uint8_t *d)
{
    for (int r = 0; r < 8; r++) {
        int py = ty * 8 + r;
        switch (fmt) {
        case SHEET_FMT_NES_2BPP:
        case SHEET_FMT_GB_2BPP: {
            uint8_t bp0 = 0, bp1 = 0;
            for (int c = 0; c < 8; c++) {
                uint8_t v = cut_px(s, x, y, scale, dw, dh, tx * 8 + c, py,
                                   pal, pal_off, pal_size);
                bp0 |= (uint8_t)((v & 1) << (7 - c));
                bp1 |= (uint8_t)(((v >> 1) & 1) << (7 - c));
            }
            if (fmt == SHEET_FMT_NES_2BPP) {
                d[r]     = bp0;         /* plane 0 block */
                d[r + 8] = bp1;         /* plane 1 block */
            } else {
                d[r * 2]     = bp0;     /* per-row low plane  */
                d[r * 2 + 1] = bp1;     /* per-row high plane */
            }
            break;
        }
        case SHEET_FMT_SNES_4BPP: {
            uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
            for (int c = 0; c < 8; c++) {
                uint8_t v = cut_px(s, x, y, scale, dw, dh, tx * 8 + c, py,
                                   pal, pal_off, pal_size);
                b0 |= (uint8_t)((v & 1) << (7 - c));
                b1 |= (uint8_t)(((v >> 1) & 1) << (7 - c));
                b2 |= (uint8_t)(((v >> 2) & 1) << (7 - c));
                b3 |= (uint8_t)(((v >> 3) & 1) << (7 - c));
            }
            d[r * 2]      = b0;
            d[r * 2 + 1]  = b1;
            d[r * 2 + 16] = b2;
            d[r * 2 + 17] = b3;
            break;
        }
        case SHEET_FMT_SNES_4BPP_PACKED:
        case SHEET_FMT_GEN_4BPP:
            for (int c = 0; c < 4; c++) {
                uint8_t hi = cut_px(s, x, y, scale, dw, dh,
                                    tx * 8 + c * 2, py,
                                    pal, pal_off, pal_size);
                uint8_t lo = cut_px(s, x, y, scale, dw, dh,
                                    tx * 8 + c * 2 + 1, py,
                                    pal, pal_off, pal_size);
                d[r * 4 + c] = (uint8_t)((hi << 4) | lo);
            }
            break;
        }
    }
}

int sheet_cut(const sheet_t *s, int x, int y, int w, int h, int scale,
              sheet_fmt_t fmt, int col_major,
              const uint32_t *pal, int pal_off, int pal_size,
              uint8_t *out_tiles, int max_tiles)
{
    int dw = w * scale, dh = h * scale;
    int tw = (dw + 7) / 8, th = (dh + 7) / 8;
    int tb = sheet_tile_bytes(fmt);
    int n = 0;

    for (int i = 0; i < tw * th && n < max_tiles; i++, n++) {
        int tx = col_major ? i / th : i % tw;
        int ty = col_major ? i % th : i / tw;
        cut_tile(s, x, y, scale, dw, dh, tx, ty, fmt,
                 pal, pal_off, pal_size, out_tiles + n * tb);
    }
    return n;
}

int sheet_cut_chunks(const sheet_t *s, int x, int y, int w, int h, int scale,
                     sheet_fmt_t fmt,
                     const uint32_t *pal, int pal_off, int pal_size,
                     uint8_t *tiles, int tile_base, int max_tiles,
                     sheet_chunk_t *chunks, int max_chunks,
                     int *tiles_used)
{
    int dw = w * scale, dh = h * scale;
    int tw = (dw + 7) / 8, th = (dh + 7) / 8;
    int tb = sheet_tile_bytes(fmt);
    int nchunks = 0, tile = tile_base;

    /* Split into <=4x4-tile sprite chunks; each chunk stores its tiles
     * as a contiguous column-major block (stride = ch) so a renderer
     * addressing tile_base + tx * h + ty matches the storage. */
    for (int cx = 0; cx < tw; cx += 4) {
        for (int cy = 0; cy < th; cy += 4) {
            int cw = (tw - cx) > 4 ? 4 : (tw - cx);
            int ch = (th - cy) > 4 ? 4 : (th - cy);

            if (nchunks >= max_chunks) goto done;
            chunks[nchunks].tile_base = tile;
            chunks[nchunks].cx = cx;
            chunks[nchunks].cy = cy;
            chunks[nchunks].cw = cw;
            chunks[nchunks].ch = ch;
            nchunks++;

            for (int ltx = 0; ltx < cw; ltx++) {
                for (int lty = 0; lty < ch; lty++) {
                    if (tile >= max_tiles) goto done;
                    cut_tile(s, x, y, scale, dw, dh,
                             cx + ltx, cy + lty, fmt,
                             pal, pal_off, pal_size, tiles + tile * tb);
                    tile++;
                }
            }
        }
    }
done:
    if (tiles_used) *tiles_used = tile - tile_base;
    return nchunks;
}
