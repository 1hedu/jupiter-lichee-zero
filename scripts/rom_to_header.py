#!/usr/bin/env python3
"""
rom_to_header.py — Convert a binary ROM file into a C header for embedding.

Usage:
    python scripts/rom_to_header.py <input.bin> <output.h> <symbol_name>
    python scripts/rom_to_header.py MT32_PCM.ROM mt32_pcm_rom.h mt32_pcm_rom

Output format:
    /* <input filename> (<size> bytes) */
    const unsigned char <symbol_name>[<size>] = {
      0x00, 0x01, ...
    };

The resulting header is what examples/mt32_poc/, examples/sc55_warcraft/,
and similar firmware-dependent examples expect to #include.
"""
import sys, os, pathlib


def main():
    if len(sys.argv) != 4:
        print(__doc__.strip(), file=sys.stderr)
        sys.exit(1)
    in_path  = pathlib.Path(sys.argv[1])
    out_path = pathlib.Path(sys.argv[2])
    symbol   = sys.argv[3]

    if not in_path.exists():
        print(f"ERROR: input file not found: {in_path}", file=sys.stderr)
        sys.exit(2)

    data = in_path.read_bytes()
    size = len(data)
    print(f"Reading {in_path} ({size} bytes) → writing {out_path} as symbol `{symbol}`")

    with open(out_path, "w") as f:
        f.write(f"/* {in_path.name} ({size} bytes) */\n")
        f.write(f"const unsigned char {symbol}[{size}] = {{\n")
        for i in range(0, size, 12):
            chunk = data[i:i+12]
            line = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"  {line},\n")
        f.write("};\n")
    print(f"Done. {out_path} = {out_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
