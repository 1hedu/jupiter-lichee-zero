# Changelog

All notable feature additions to licheeEmu, organized by the development
phase that introduced them. Phases roughly correspond to the question
"what guest code broke next, and which device did we add to fix it?"

The project doesn't tag releases yet; each phase below maps to a contiguous
run of commits on `main`.

## Unreleased

- **Cedar SPS/PPS synthesis** — `cedar_codec_open` now builds baseline
  H.264 SPS+PPS bitstreams from the guest-supplied SHS_PARAM registers
  (mb_w / mb_h / qp) and feeds them to libavcodec via `ctx->extradata`.
  Vbin streams in the wild contain slice NALs only; without this,
  libavcodec rejects every frame with "non-existing PPS 0 referenced"
  (commit `7d8b53d`).
- **virt-ym3438** — virtual YM3438 (OPN2C) FM synth backed by Nuked-OPN2.
  The device snoops PB0..PB7 + PG0..PG4 transitions from the PIO,
  reconstructs the GPIO-bit-banged bus protocol that the Jupiter
  `lib/ym3438_hw.c` driver issues, and pumps stereo samples to the host
  audio backend. Tolerates missing `-audio` backend (silent in that case
  rather than failing realize).

## Phase 8 — Cedar VE / H.264 decode (`f783880`)

The `allwinner-v3s-cedar` device at `0x01C0E000` models the V3s Video
Engine's H.264 decode path. Real silicon is a fixed-function
macroblock-by-macroblock decoder driven by register pokes; we recover the
bitstream from guest DRAM via VLD_ADDR / VLD_END, hand it to a host
libavcodec H.264 decoder, convert the result from YUV420P to NV12, and
write the planes back into the guest-pointed luma/chroma buffers (frame
list at internal SRAM `0x100`, words 3 and 4).

If `pkg-config` can't find libavcodec/libavutil, the device still compiles
and reports `OK` to the guest but writes nothing — the guest sees stale
frames. `-DCONFIG_LIBAVCODEC=1` is set automatically when the libs are
available.

## Phase 7 — Real-HW SD partition layout (`cbc99e9`)

`scripts/make-sd-image.sh` rewritten to match what the Jupiter FatFs glue
expects: `third_party/fatfs/diskio.c`'s `VolToPart` maps drive `0:/` to
**partition #2** (1-indexed). The new layout is:

| LBA range          | Type | Purpose                                |
|--------------------|------|----------------------------------------|
| 0                  | —    | MBR                                    |
| 2048..4095         | 0x83 | 1 MiB Linux dummy (SPL placeholder)    |
| 4096..524287       | 0x06 | ~254 MiB FAT16 — what `0:/` mounts to  |
| 524288+            | —    | free space (WC1 music pack splice)     |

`scripts/populate-fat.sh` was added to mount partition #2 via loop device
and copy `/videos/*.vbin` and `/intros/*` from the Jupiter build tree onto
it.

## Phase 6 — Joybus protocol & Controller Pak (`4721f36`)

`virt-n64-pad` gained a full Nintendo 64 Joybus 1-wire protocol decoder.
RX path: timestamp falling edges, decode pulse widths (cell ≈ 4 µs;
<2 µs = bit `1`, ≥2 µs = bit `0`). TX path: schedule per-bit timer
expiries to clock the response back onto the same line.

Command dispatch:
- `0x00 INFO` — 3-byte response (`0x05 0x00 0x02`, "Cpak inserted")
- `0x01 BTN_STATE` — 4-byte controller state
- `0x02 READ32` — 33-byte response (32 B pak data + CRC-8)
- `0x03 WRITE32` — 1-byte CRC-8 acknowledgment

The 32 KB Controller Pak is in-memory only (no persistence yet).

## Phase 5 — Storage path (`c5a244b`, `65c1039`, `f23b97a`)

- **SDMMC0 controller** at `0x01C0F000`: re-uses the QEMU sun5i SDHOST
  model, IRQ on GIC SPI 23. Lichee Zero's machine init plugs an
  `IF_SD,0,0` drive into the resulting `sd-bus` so `-drive
  if=sd,file=...,format=raw` works.
- **`scripts/make-sd-image.sh`** — original 1-partition FAT16 image
  (superseded by Phase 7 layout).
- **`scripts/make-music-sd.sh`** — one-shot wrapper: build a fresh
  `sd.img` and splice `wc1_music.img` at LBA `524288`.
- **Codec sample-rate fix** — `allwinner-v3s-codec` was previously
  reading rate from a hard-coded `Hz` table that ignored guest PLL
  reprogramming. Replaced with `f_pll = 24 MHz × (N+1) / ((M+1) × (P+1))`
  decoded from `PLL_AUDIO_CTRL`, divided by the FIFOC FS divisor table
  `{512, 768, 1024, 1536, 2048, 3072, 128, 256}`.

## Phase 4 — Audio codec + DMA (`639d175`)

- **`allwinner-v3s-codec`** at `0x01C22C00` — DAC FIFO model. Writes to
  `DAC_TXDATA` get pushed to a host SWVoiceOut. `DAC_FIFOS` returns
  "always room available."
- **`allwinner-v3s-dma`** at `0x01C02000` — 8-channel descriptor-driven
  engine. Per-channel descriptor lives in DRAM (24 bytes: cfg / src / dst
  / byte-count / param / next-pointer). Audio paths terminate with the
  DMA writing into the codec; we trap `dst == DAC_TXDATA` and forward the
  payload directly so the guest's IRQ-driven double-buffer keeps cycling.

## Phase 3, part 2 — Virtual N64 pad + PIO edge fan-out (`8f4da2c`)

- **`virt-n64-pad`** plug device wired to PE20 (Lichee Pi Zero pin
  convention from Jupiter's `lib/input.c`).
- **PIO edge fan-out** — `aw_v3s_pio_write` recomputes
  `port_effective_level` after every CFG/DAT touch and fires
  `qemu_set_irq(s->pin_out[…])` only on actual level changes. This is
  what makes the pad model work: the Joybus driver pulls the line low by
  switching the pin from input (Hi-Z + external pullup) to output-low,
  not by writing the DAT bit.
- **icount requirement** — without `-icount shift=auto`, virtual timer
  expiries bunch up at ~20 µs resolution and Joybus bit detection fails.
  This is documented in `scripts/run.sh`.

## Phase 3, part 1 — PIO + HSTimer (`c15f81f`)

- **`allwinner-v3s-pio`** — generic GPIO controller covering PB through
  PH (1 KiB MMIO window). `DAT` reads composite guest-driven level and
  external `ext_in[]` levels so sibling peripherals can assert input
  states without racing guest writes.
- **`allwinner-v3s-hstimer`** at `0x01C60000` — 2-channel 56-bit
  down-counter. Assumes 200 MHz AHB (5 ns/tick). IRQs to GIC SPI 51/52.

## Phase 1 + 2 — Boot + display (`7c2b16b`)

- **`allwinner-v3s` SoC**: Cortex-A7, 64 MiB DDR2 at `0x40000000`, GICv2,
  UART0 (16550, 4-byte stride), reuses H3 CCU / SysCtrl / CPUcfg / SID
  models, A10 PIT for periodic interrupts.
- **`lichee-zero` machine**: bare-metal entry — drops `-kernel` payload
  verbatim at `0x41000000` (where U-Boot's `go 0x41000000` jumps after
  SPL on real hardware) and points the CPU PC there on reset. No DTB,
  no PSCI, no firmware.
- **`allwinner-v3s-display`** — DE2 + Mixer0 + TCON0 stitched into one
  device. Renders VI0 (game world, XRGB8888, 480×272) plus UI0 (ARGB8888
  overlay) into a QemuConsole. Not register-accurate; only the bits
  Jupiter actually flips are honored.
- **`scripts/patch-and-build.sh`** — idempotent installer that copies
  every source/header into a stock QEMU tree and patches
  `meson.build`/`Kconfig`/`default.mak` to wire `CONFIG_LICHEE_ZERO`.
- **`scripts/run.sh`** — convenience launcher with `-icount`, audio
  fallback, optional SD plug.
