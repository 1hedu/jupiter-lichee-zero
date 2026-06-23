# licheeEmu

A QEMU-based emulator for the **Lichee Pi Zero** (Allwinner V3s, Cortex-A7),
purpose-built to run bare-metal payloads from the Jupiter SDK — homebrew demos,
ports, and games — on a host PC without the physical board.

This is not an upstream QEMU patchset. It is a small, self-contained set of
device models that you drop into a stock QEMU 9.2.x source tree and build. The
payload binary that runs on the emulator is byte-identical to the one you'd
flash to a real board.

## Status

| Subsystem            | Device                | Notes                                    |
|----------------------|-----------------------|------------------------------------------|
| CPU / RAM / GIC      | `allwinner-v3s`       | 1× Cortex-A7, 64 MiB DDR2, GICv2         |
| Clocks / SysCtrl     | reused H3 models      | PLL_LOCK fakery, enough for boot         |
| Display              | `allwinner-v3s-display` | DE2 + Mixer0 + TCON0 → SDL window      |
| GPIO                 | `allwinner-v3s-pio`   | Full register model + edge fan-out       |
| Hi-speed timer       | `allwinner-v3s-hstimer` | 56-bit down counter, 2 channels        |
| Periodic timer       | reused A10 PIT        |                                          |
| DMA                  | `allwinner-v3s-dma`   | 8 channels, descriptor-driven            |
| Audio DAC            | `allwinner-v3s-codec` | TXDATA FIFO → host audio                 |
| Audio FM             | `virt-ym3438`         | GPIO-snooped Nuked-OPN2 backend          |
| Storage              | `allwinner-sdhost`    | reused sun5i model + MBR/FAT16 image     |
| Video decode         | `allwinner-v3s-cedar` | H.264 baseline IDR via host libavcodec   |
| Input                | `virt-n64-pad`        | Joybus 1-wire over PE20 + 32 KB Cpak     |

## Demo

These bare-metal binaries live under [`docs/`](docs/) and boot directly on the
emulator:

- `menu.bin` — Jupiter SDK example launcher (D-pad + A button)
- `war1.bin` — Warcraft 1 port (Stratagus engine, FAT16 SD assets, H.264 cinematics)
- `cpak_browser.bin` — N64 Controller Pak hex viewer
- `colorbars.bin`, `hstimer_raster.bin`, `n64_diag.bin`, `opn2_rt.bin`, `sdmmc.bin`, …

See [`docs/TEST_BINARIES.md`](docs/TEST_BINARIES.md) for the full list.

## Quickstart (WSL or Linux)

Prerequisites: bash, ninja, meson ≥ 0.63, pkg-config, a C toolchain, SDL2-dev,
libpulse-dev (optional, for audio), libavcodec-dev + libavutil-dev (optional,
enables H.264 video decode).

```bash
# 1. Clone QEMU 9.2 next to this repo
git clone --branch v9.2.0 --depth 1 https://gitlab.com/qemu-project/qemu.git \
    ~/licheeEmu-build/qemu

# 2. Build (idempotent; copies our sources into the QEMU tree, runs ninja)
./scripts/patch-and-build.sh

# 3. Run the menu demo
./scripts/run.sh -g docs/menu.bin
```

Full setup is in [`docs/BUILD.md`](docs/BUILD.md).

## Repo layout

```
src/
  hw/                  device models (one .c per device)
    arm/               SoC + board (allwinner-v3s, lichee-zero)
    audio/             v3s codec (DAC), virt-ym3438 (Nuked-OPN2)
    display/           DE2 + Mixer0 + TCON0
    dma/               V3s DMA engine
    gpio/              V3s PIO
    input/             virt-n64-pad
    misc/              v3s-cedar (H.264 decoder)
    timer/             V3s HSTimer
  include/hw/...       paired headers
  third_party/
    nuked_opn2/        vendored Nuked-OPN2 (LGPL 2.1+) for virt-ym3438
scripts/
  patch-and-build.sh   wire sources into QEMU tree + build
  run.sh               launch a payload on lichee-zero
  make-sd-image.sh     1 GiB sparse SD image with FAT16 partition #2
  make-music-sd.sh     splice WC1 music pack onto sd.img
  populate-fat.sh      copy /videos and /intros into the FAT
docs/
  ARCHITECTURE.md      device tree, MMIO map, host data flow
  BUILD.md             dependencies, build, troubleshooting
  RUN.md               running payloads, expected output
  PHASES.md            chronological design walkthrough
  TEST_BINARIES.md     index of every .bin shipped under docs/
  devices/             one short page per device
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — how the SoC model is wired and what
  each device assumes about its caller.
- [Build guide](docs/BUILD.md) — host setup, dependencies, common errors.
- [Runtime guide](docs/RUN.md) — how to launch payloads, audio backends,
  SD images, headless mode.
- [Phase walkthrough](docs/PHASES.md) — narrative of how the emulator was
  built, phase by phase, with the design tradeoffs that mattered.
- [Per-device pages](docs/devices/) — register layout, IRQ wiring, host
  backing, and known limitations for each device.

## License

Original code is **GPL-2.0-or-later** (matching QEMU). See [`LICENSE`](LICENSE).

Vendored **Nuked-OPN2** under `src/third_party/nuked_opn2/` is **LGPL-2.1-or-later**
by Alexey Khokholov (Nuke.YKT).

The PDF datasheets at the repo root are vendor documents (Allwinner / Sipeed)
and are included for reference only; their copyright is held by their respective
vendors.
