# mt32_poc — MT-32 emulator proof-of-concept

Brings up Munt (the MT-32 emulator) on the V3s and plays a test
SMF. Requires the Roland MT-32 firmware ROM, which is **Roland's
copyrighted property and is not distributed with this SDK**.

## What you need

| File | Size | Source |
|---|---|---|
| `MT32_PCM.ROM` (also called `MT-32_PCM.ROM`) | 524 288 bytes | Roland MT-32 v1.07 legacy PCM ROM. Dump from your own MT-32 unit, or obtain from your Roland-cleared backup of a legally owned MT-32. |

## How to convert your ROM into the header this example needs

```bash
python scripts/rom_to_header.py /path/to/MT32_PCM.ROM \
    examples/mt32_poc/mt32_pcm_rom.h \
    mt32_pcm_rom
```

Then build and run as usual:

```bash
make GAME=examples/mt32_poc/main.c
```

The build will fail with `mt32_pcm_rom.h: No such file or directory`
until you do the conversion above.

## Legal note

Roland's MT-32 firmware is not free software. We don't redistribute
it. Only use a ROM dump you legally obtained from a unit you own.
