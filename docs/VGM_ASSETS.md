# VGM Music Assets

Several SDK examples play back Sega Genesis VGM music files through
Nuked-OPN2 (and the real YM3438 hardware where wired). The VGM
binary files themselves are **copyrighted compositions from
commercial Sega Genesis games** and are not distributed with this
SDK.

Each example expects a specific VGM embedded as a C header
(`<name>.h` declaring `const unsigned char vgm_<name>[]` plus
`const unsigned int vgm_<name>_len`). The `scripts/rom_to_header.py`
script produces exactly this format from any binary input.

## What you need

| VGM | Source game | Size | Used by |
|---|---|---|---|
| `fighting_back.vgm` | Thunder Force IV — Stage 1A "Fighting Back" (Genesis, Technosoft / Sega 1992) | 514 423 bytes | `examples/genesis_vdp/`, `examples/opn2_jupiter/`, `examples/opn2_megademo/`, `examples/opn2_rt/`, `examples/ym3438/` |
| `step_on_beat.vgm` | Sonic the Hedgehog 3 — "Step On the Beat" (Genesis, Sega 1994) | 1 604 603 bytes | `examples/av_demo/` |
| `sortie.vgm` | Comix Zone — Episode 1 "Sortie" (Genesis, Sega 1995) | 215 503 bytes | `examples/opn2_hw_live/`, `examples/opn2_hw_xtal/` |

These specific VGM dumps came from community archives (e.g.
[vgmrips.net](https://vgmrips.net/)). They are dumps of the actual
YM2612/SN76489 register writes from real Genesis games — they
contain the composers' / publishers' copyrighted music. Don't
redistribute them; either rip your own from a cartridge you own
(using `vgm_rip` against a real Genesis or accurate emulator), or
download from a community archive at your own risk.

## How to convert each VGM into the header the example expects

```bash
python scripts/rom_to_header.py /path/to/fighting_back.vgm \
    examples/genesis_vdp/fighting_back.h vgm_fighting_back
# Repeat for each example that needs the same VGM:
python scripts/rom_to_header.py /path/to/fighting_back.vgm \
    examples/opn2_jupiter/fighting_back.h vgm_fighting_back
# ... etc for opn2_megademo, opn2_rt, ym3438

python scripts/rom_to_header.py /path/to/step_on_beat.vgm \
    examples/av_demo/step_on_beat.h vgm_step_on_beat

python scripts/rom_to_header.py /path/to/sortie.vgm \
    examples/opn2_hw_live/sortie.h vgm_sortie
python scripts/rom_to_header.py /path/to/sortie.vgm \
    examples/opn2_hw_xtal/sortie.h vgm_sortie
```

Each command produces the expected `vgm_<name>[]` array + matching
`vgm_<name>_len` constant the example's `vgm_load(...)` /
`vgm_hw_init(...)` calls reference.

## Build

Once the headers exist in the right places:

```bash
make GAME=examples/<name>/main.c
```

Build fails with `<name>.h: No such file or directory` until you
run the conversion for that example.

## Legal note

These compositions remain the property of their original
copyright holders (Sega, Technosoft, the original composers).
The SDK ships only the code that plays them; the music itself is
yours to source legally.
