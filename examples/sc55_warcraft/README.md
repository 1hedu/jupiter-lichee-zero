# sc55_warcraft — Nuked-SC55 + WC1 music demo

Plays a Warcraft 1 MIDI through Roland SC-55 mkII emulation
(Nuked-SC55) running on the V3s. Requires both the SC-55 firmware
ROMs and a WC1 MIDI source, neither of which are distributed with
this SDK.

## What you need (Roland SC-55 mkII ROMs)

Five binary ROM files dumped from a real SC-55 mkII unit. Filenames
must match the source filename annotation in the existing example
code:

| Source file | Size | What it is | Header to generate |
|---|---|---|---|
| `r15199858_main_mcu.bin` | 32 768 bytes | Main CPU firmware | `sc55_rom1.h` (symbol `sc55_rom1`) |
| `r00233567_control.bin`  | 524 288 bytes | Control ROM | `sc55_rom2.h` (symbol `sc55_rom2`) |
| `r15199880_secondary_mcu.bin` | 4 096 bytes | Secondary MCU firmware | `sc55_rom_sm.h` (symbol `sc55_rom_sm`) |
| `r15209359_pcm_1.bin` | 2 097 152 bytes | PCM ROM (sample bank 1) | `sc55_waverom1.h` (symbol `sc55_waverom1`) |
| `r15279813_pcm_2.bin` | 1 048 576 bytes | PCM ROM (sample bank 2) | `sc55_waverom2.h` (symbol `sc55_waverom2`) |

These are Roland's copyrighted firmware. We don't distribute them.
You must dump them from your own SC-55 mkII unit (or obtain from
your legal backup of one).

## How to convert each ROM into the header this example needs

Run `scripts/rom_to_header.py` once per ROM:

```bash
python scripts/rom_to_header.py /path/to/r15199858_main_mcu.bin       examples/sc55_warcraft/sc55_rom1.h     sc55_rom1
python scripts/rom_to_header.py /path/to/r00233567_control.bin        examples/sc55_warcraft/sc55_rom2.h     sc55_rom2
python scripts/rom_to_header.py /path/to/r15199880_secondary_mcu.bin  examples/sc55_warcraft/sc55_rom_sm.h   sc55_rom_sm
python scripts/rom_to_header.py /path/to/r15209359_pcm_1.bin          examples/sc55_warcraft/sc55_waverom1.h sc55_waverom1
python scripts/rom_to_header.py /path/to/r15279813_pcm_2.bin          examples/sc55_warcraft/sc55_waverom2.h sc55_waverom2
```

Build:

```bash
make GAME=examples/sc55_warcraft/main.c
```

Build will fail with missing header errors until all five are present.

## Legal note

Roland's SC-55 firmware is not free software. We don't redistribute
it. Only use ROM dumps you legally obtained from a unit you own.
