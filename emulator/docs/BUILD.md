# Build guide

This walks through getting `qemu-system-arm` with the `lichee-zero`
machine model built from a stock QEMU 9.2 tree plus this repo's sources.

## Prerequisites

### Required

- **WSL2 / Ubuntu 24.04** (or any modern Linux). Build is tested on
  Ubuntu under WSL2 on Windows 11.
- **C toolchain** — `gcc` 11+ (`build-essential`) or clang.
- **Build glue** — `meson` ≥ 0.63, `ninja-build`, `pkg-config`,
  `python3`.
- **glib2** — `libglib2.0-dev`.
- **Headers QEMU itself needs** — `libpixman-1-dev`, `libslirp-dev`,
  `libfdt-dev` if you want device-tree support (we don't use it).

```bash
sudo apt update && sudo apt install -y \
    build-essential ninja-build meson pkg-config python3 git \
    libglib2.0-dev libpixman-1-dev libslirp-dev
```

### Optional but recommended

- **SDL2** — `libsdl2-dev` for the `-display sdl` graphical window.
  Without this, build configure-passes but `-display sdl` falls back to
  `none`.
- **PulseAudio** — `libpulse-dev` for `-audio pa`. WSL2 with WSLg
  already has the runtime; on a headless box you'll want PipeWire-pulse
  or fall back to `-audio none`.
- **libavcodec / libavutil** — `libavcodec-dev libavutil-dev`. Required
  for the Cedar VE device's H.264 decode. Without these, the device
  compiles in stub mode (the war1 cinematic plays audio but the SDL
  window stays on the previous frame).

```bash
sudo apt install -y libsdl2-dev libpulse-dev libavcodec-dev libavutil-dev
```

## Cloning

```bash
# This repo
git clone https://github.com/<your-fork>/licheeEmu.git
cd licheeEmu

# QEMU upstream — this is what gets patched
git clone --branch v9.2.0 --depth 1 \
    https://gitlab.com/qemu-project/qemu.git ~/licheeEmu-build/qemu
```

The `~/licheeEmu-build/qemu` path is hardcoded in
`scripts/patch-and-build.sh`. If you want it elsewhere, edit `QEMU_DIR`
in that script.

## Building

```bash
./scripts/patch-and-build.sh
```

What this does, in order:

1. **Copy** every `src/...` source and header into the matching path
   under `~/licheeEmu-build/qemu/`. This is `install -Dm644` (regular
   copy, not symlink) — you edit files in this repo and re-run the
   script to propagate.
2. **Wire** the new sources into QEMU's build:
   - `hw/arm/meson.build` — adds an `arm_ss.add(when:
     'CONFIG_LICHEE_ZERO', if_true: files(...))` line.
   - `hw/{audio,display,dma,gpio,input,timer}/meson.build` — same
     pattern, one line each.
   - `hw/misc/meson.build` — Cedar VE block; auto-detects libavcodec
     via `dependency('libavcodec', required: false)` and conditionally
     defines `CONFIG_LIBAVCODEC=1`.
   - `hw/audio/meson.build` — virt-ym3438 + the vendored Nuked-OPN2
     sources, with an `include_directories('nuked_opn2/emu/cores')`
     dep so the wrapper finds the headers.
   - `hw/arm/Kconfig` — defines `config LICHEE_ZERO`, selects
     `ALLWINNER_H3` (for CCU/sysctrl/cpucfg/SID reuse), `ARM_GIC`,
     `SERIAL_MM`, `UNIMP`.
   - `configs/devices/arm-softmmu/default.mak` — opts in by default.
3. **Configure** (first run only) — `../configure --target-list=arm-softmmu
   --enable-sdl --disable-docs --disable-werror --disable-capstone`.
4. **Build** — `ninja -C build qemu-system-arm`.

The script is idempotent: re-runs detect existing wiring with `grep -q`
and skip the `meson.build` patches; the source copies always re-run.

Output binary: `~/licheeEmu-build/qemu/build/qemu-system-arm`.

## Verifying

```bash
~/licheeEmu-build/qemu/build/qemu-system-arm -M help | grep lichee
# expect: lichee-zero          Lichee Pi Zero (Allwinner V3s, Cortex-A7)

./scripts/run.sh -g docs/menu.bin
# SDL window should pop up showing the Jupiter SDK example menu.
```

If you see the menu and can D-pad through it with the arrow keys, the
build is good.

## Common errors

### `configure: error: glib-2.0 >= 2.66.0 is required`

Install `libglib2.0-dev`. On Ubuntu 22.04 the version is borderline;
upgrade to 24.04 if you hit this.

### `ERROR: meson >= 0.63.3 is required`

```bash
pip3 install --user 'meson>=0.63'
export PATH=~/.local/bin:$PATH
```

### `ERROR: SDL2 not found`

```bash
sudo apt install libsdl2-dev
```

If you genuinely don't want SDL, drop `--enable-sdl` from the
`configure` line in `scripts/patch-and-build.sh` and use
`-display none -serial stdio`.

### Cedar VE: `Could not find: libavcodec`

You'll see `[cedar] libavcodec not built in (stub mode)` when running.
The build still completes; install `libavcodec-dev libavutil-dev` and
re-run `scripts/patch-and-build.sh`. The auto-detect uses
`pkg-config --exists libavcodec`.

### `nuked_opn2/emu/cores/ym3438.c: error: no previous prototype for ...`

Means the vendored `ym3438_int.h` got out of sync with `ym3438.c` —
should be fixed in the current tree (the patch adds prototypes for
`NOPN2_WriteBuffered` and `NOPN2_GenerateResampled`). Re-pull and
re-run.

### `BIT redefined`

QEMU's `qemu/bitops.h` defines `BIT(nr)` as `(1UL << (nr))`. New device
code can't use a local `BIT(...)` macro. Use `(1u << ...)` inline or
pick a different name (the YM3438 device uses `YM_PIN_BIT`).

### `error: declaration of 'i' shadows a previous local`

Renamed loop counters fix this. QEMU is built with `-Wshadow=local
-Werror`.

### Build succeeded but `-M lichee-zero` is missing from `-M help`

The Kconfig wiring didn't take. Check that `hw/arm/Kconfig` has the
`config LICHEE_ZERO` block and `configs/devices/arm-softmmu/default.mak`
has `CONFIG_LICHEE_ZERO=y` (or no `# CONFIG_LICHEE_ZERO=n` line). On
re-runs the script preserves the existing setting; if you're starting
fresh, delete the line in `default.mak` and re-run.

## Cleaning

```bash
ninja -C ~/licheeEmu-build/qemu/build clean
```

Or nuke the build dir:

```bash
rm -rf ~/licheeEmu-build/qemu/build
```

Then re-run `scripts/patch-and-build.sh` to reconfigure + rebuild.

## Development loop

The fast loop while iterating on a single device:

```bash
# edit src/hw/<device>/foo.c or .h
./scripts/patch-and-build.sh   # ~30 s incremental rebuild
./scripts/run.sh -g docs/menu.bin
```

Full rebuild from `make clean` is ~5 minutes on a modern laptop with
SDL/audio/codec support.
