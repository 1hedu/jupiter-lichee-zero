# Warcraft 1 Port — Build Guide

How to take the original Blizzard WC1 DOS data and end up with
`jupiter.bin` running the game on the Lichee Pi Zero. The Jupiter SDK
ships the engine port (Stratagus + War1gus + Lua + guisan), the asset-
conversion scripts, and the example entry point at `examples/war1/`.
You bring the original game data.

## 1. What you need

| Item | Why |
|---|---|
| Original WC1 DOS data | The actual game files (`.IMG`, `.WAR`, etc.) — you must legally own a copy. Disk, CD, or the GOG installer all work. |
| **War1gus** (desktop, any OS) | Extracts the DOS data into the directory tree we consume. Get it from <https://wargus.github.io/war1gus.html>. |
| Python 3 | Asset-conversion scripts (`scripts/war1_*.py`, `scripts/sd_pack_music.py`). |
| FFmpeg | Used by `wc1_video_pack.py` to transcode cinematics. |
| `arm-none-eabi-gcc` | The SDK compiler. |
| `u-boot-tools` | `mkimage` for building the boot script. |
| SD card | At minimum 1 GB. Two partitions: boot (FAT, jupiter.bin + boot.scr) + data (FAT16/FAT32, ~1 GB for assets). Music pack writes raw blocks past the data partition. |

## 2. Extract the source data

Run War1gus on a desktop. Point it at your WC1 install (GOG default
is `C:\GOG Games\Warcraft Orcs & Humans\DATA`). It'll extract
into the standard War1gus layout — typically
`Documents/Stratagus/data.War1gus/`:

```
data.War1gus/
├── graphics/           PNG sprites + tilesets per race
├── sounds/             *.wav.gz unit voices, SFX, building sounds
├── music/              00.mid … 44.mid (Adlib renderings of the WC1 OST)
├── videos/             OGV cinematics (intro, mission briefings, etc.)
└── scripts/            Lua / SMS scenario scripts
```

If War1gus runs and you can play the campaign on desktop, the data
extraction succeeded.

## 3. Convert assets for Jupiter

Three scripts process the extracted data into Jupiter-embeddable
formats. Run them from the SDK root, pointing each at the matching
subdirectory.

### 3a. Graphics (PNG → 8bpp paletted .pc8 + catalog)

```bash
python scripts/war1_assets.py /path/to/data.War1gus/graphics/
```

Output: `examples/war1/assets/**/*.pc8` and
`examples/war1/war1_asset_catalog.h` (generated). Each .pc8 becomes
an objcopy'd binary blob the engine resolves by its original
Stratagus path string (e.g. `tilesets/forest/terrain.png` →
`war1_asset_tilesets_forest_terrain_pc8_start`).

### 3b. Sound (gzipped WAV → raw PCM + catalog)

```bash
python scripts/war1_sounds.py /path/to/data.War1gus/sounds/
```

Output:
- `examples/war1/assets/sounds/*.pcm` — embedded SFX, unit voices,
  building sounds (the small ones, ~2.7 MB total).
- `examples/war1/fatfs_payload/intros/*.pcm` — long campaign-intro
  voiced narrations (~17 MB), too big to embed. Stream from the SD's
  FAT partition at runtime.
- `examples/war1/war1_sound_catalog.h` (generated).

### 3c. Cinematics (OGV → CedarVE-ready vbin)

```bash
python scripts/wc1_videos_convert.py \
    /path/to/data.War1gus/videos/ \
    build/wc1_vbins/
```

Output: one `.vbin` file per `.ogv` in `build/wc1_vbins/`. These get
copied to the SD's FAT partition for runtime playback via CedarVE.

### 3d. Music (MIDI → SC-55 rendered PCM → SD music pack)

WC1's MIDIs were composed for AdLib. We render them through
Nuked-SC55 (Roland SC-55 emulator) to mono 11025 Hz PCM, then pack
them as a raw-block image written past the FAT partition.

Two-step:

```bash
# 1. Render every MIDI through Nuked-SC55 to WAV.
#    Note: the sc55_render binary itself is NOT in this repo (the
#    Nuked-SC55 driver source needs to land here in a future pass).
#    See tools/sc55_render/README.md for the workarounds — either
#    build Nuked-SC55 yourself, or render through any SC-55
#    compatible synth and drop 00.wav…44.wav into tools/sc55_render/out/.
#    Once the WAVs exist:
bash tools/sc55_render/render_all.sh    # if you have a working sc55_render

# 2. Pack the rendered WAVs into a single SD-block image.
python scripts/sd_pack_music.py tools/sc55_render/out/ build/wc1_music.img
```

Output: `build/wc1_music.img`. Layout is documented at the top of
`scripts/sd_pack_music.py` (4-sector header + sector-aligned PCM
tracks).

## 4. Build the game

```bash
make GAME=examples/war1/main.c
```

The Makefile detects "war1" in the path and pulls in the engine
sources (Stratagus + Lua + guisan + tolua++), every `.pc8` and
`.pcm` from `examples/war1/assets/`, and every Lua/SMS script from
`examples/war1/scripts/`. Output: `build/jupiter.bin` (typically
several MB).

```bash
make boot              # build/boot.scr — the U-Boot launch script
```

## 5. Write to the SD card

The SD layout for a WC1 boot card:

```
sector 0 .. 524287    : Two FAT partitions (boot + data)
sector 524288 onward  : Raw music pack — no filesystem here
```

### Partition 1 (boot, FAT, ~16 MB)
```
boot.scr               # from `make boot`
jupiter.bin            # from `make GAME=examples/war1/main.c`
```

Either `make sdcard MNT=/mnt/sd-boot` if you've mounted the partition
already, or copy by hand.

### Partition 2 (data, FAT16/FAT32, ~256 MB)
```
intros/*.pcm           # entire contents of examples/war1/fatfs_payload/intros/
vbins/*.vbin           # entire contents of build/wc1_vbins/
```

Plain drag-drop.

### Music pack (raw blocks past the partitions)

```bash
# Identify your SD device first — wrong dev path = wrecked drive.
lsblk
sudo dd if=build/wc1_music.img of=/dev/sdX seek=524288 bs=512 conv=fsync
```

On Windows, use `scripts/sd_write_music.ps1` instead.

## 6. Boot

Power up with the SD card inserted. U-Boot loads `jupiter.bin` from
the boot partition. Stratagus runs through its canonical boot
sequence (`InitLua → InitSound → InitVideo → ShowTitleScreens →
PreMenuSetup → MenuLoop`). Title menu appears; pick a campaign;
play.

## 7. Smoke tests if something doesn't work

Before chasing engine bugs, verify the data layer with the dedicated
diagnostic examples — each tests one slice of the asset pipeline in
isolation:

| Build target | Verifies |
|---|---|
| `examples/sdmmc/main.c` | SD card mounts, blocks read |
| `examples/sdmmc_music/main.c` | The music pack at LBA 524288 is well-formed; prints track table |
| `examples/fs_test/main.c` | FatFs sees your data partition, lists files |
| `examples/cpak_browser/main.c` | (Unrelated to WC1 but tests Controller Pak save backend) |
| `examples/cedar_video_av/main.c` | CedarVE plays a single .vbin with audio sync |
| `examples/wc1_save/main.c` | Round-trips a 16 KB save game through SD blocks and Controller Pak |

If `sdmmc_music` can't find the pack, the `dd` step didn't take. If
`fs_test` can't see your assets, the FAT partition isn't
recognized. If `cedar_video_av` plays but `war1` doesn't, the engine
side is the problem.

## 8. Limits and caveats

- **No networking.** Multiplayer (LAN, internet) is out — the V3s
  has no Wi-Fi or Ethernet wired up. Single-player campaigns + skirmish
  vs. AI only.
- **Original game data isn't distributed.** You must legally own a
  WC1 copy. The asset scripts will refuse to run on a directory
  that doesn't look like a War1gus extraction.
- **Music rendering is one-time.** The MIDI → SC-55 PCM step happens
  on the desktop, not on the V3s — the Cortex-A7 can't run Nuked-SC55
  in real time (~3× too slow). The pre-rendered PCM streams from SD
  at playback.
- **First-boot timing.** Initial Lua script parse + asset table
  construction takes ~2-3 seconds before the title screen appears.
  That's the engine, not a hang.

## 9. Where to look in the code

- `examples/war1/main.c` — bare-metal entry point, hands off to
  `stratagusMain()`
- `examples/war1/war1_bridge.cpp` — port-side glue: SD music
  streaming, FatFs payload mounts, audio channel allocation
- `third_party/stratagus_port/` — the Jupiter-side adapter shims for
  the upstream engine
- `docs/ports/WC1_PORT_AUDIT.md` — subsystem-by-subsystem status of
  what's verbatim upstream vs. modified / stubbed
- `docs/ports/WC1_PORTING_OTHER_STRATAGUS_GAMES.md` — what would
  change to port Wargus / other Stratagus games using the same
  Jupiter scaffolding
