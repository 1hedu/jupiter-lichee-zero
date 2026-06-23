# Architecture

This document describes the licheeEmu device tree, MMIO map, IRQ
routing, and host backings — the contract every guest payload sees.

## What we model

The Lichee Pi Zero is a single-board computer built around the Allwinner
V3s, a single-core Cortex-A7 SoC originally aimed at IP cameras. The
relevant hardware is:

- 1× Cortex-A7 @ ~1.2 GHz (we model 1× A7 with TCG; speed is not real-time).
- 64 MiB DDR2 at physical `0x40000000`.
- ARM GICv2.
- 16550-compatible UARTs at `0x01C28000` / `0x01C28400` / `0x01C28800`.
- 4.3" parallel-RGB LCD driven by Display Engine 2 (DE2) → Mixer0 → TCON0.
- SDIO/SDMMC0, audio codec, DMA, several timers, GPIO, H.264 decoder
  (Cedar VE), assorted unused-by-Jupiter blocks.

Everything Jupiter-bare-metal payloads touch has a real device model
here; everything else is `create_unimplemented_device(...)` so stray
pokes log a warning instead of aborting.

## SoC top level

The `allwinner-v3s` device (`src/hw/arm/allwinner-v3s.c`) instantiates
all of the above and wires them to the GIC. The board layer
(`lichee-zero` in `src/hw/arm/lichee-zero.c`) is thin — its sole jobs
are:

- Creating one `allwinner-v3s` instance with `clk0=32.768 kHz`,
  `clk1=24 MHz`.
- Loading `-kernel <bin>` verbatim at `0x41000000` and pointing the CPU
  PC there on reset (matching U-Boot's `go 0x41000000` handoff on real
  silicon — no DTB, no PSCI, no firmware).
- Plugging an `IF_SD,0,0` drive into the SoC's `sd-bus` so
  `-drive if=sd,file=...` works.
- Realizing one `virt-n64-pad` and wiring its data line to the PIO's
  PE20 pin (Lichee Pi Zero pin convention from Jupiter's
  `lib/input.c`).

## MMIO map

| Base       | Size   | Device                | Source                                         |
|------------|--------|-----------------------|------------------------------------------------|
| 0x00000000 | 48 KiB | SRAM A1               | `memory_region_init_ram` in v3s realize        |
| 0x00044000 | 16 KiB | SRAM A2               | "                                              |
| 0x01000000 | —      | DE2 (display engine)  | `allwinner-v3s-display`                        |
| 0x01100000 | 48 KiB | Mixer0                | `allwinner-v3s-display`                        |
| 0x01C00000 | —      | SysCtrl               | reused `allwinner-h3-sysctrl`                  |
| 0x01C02000 | 4 KiB  | DMA                   | `allwinner-v3s-dma`                            |
| 0x01C0C000 | —      | TCON0                 | `allwinner-v3s-display`                        |
| 0x01C0E000 | 4 KiB  | Cedar VE              | `allwinner-v3s-cedar`                          |
| 0x01C0F000 | —      | SDMMC0                | reused `allwinner-sdhost-sun5i`                |
| 0x01C14000 | —      | SID (chip-id fuses)   | reused `allwinner-sid`                         |
| 0x01C20000 | —      | CCU (clocks)          | reused `allwinner-h3-ccu`                      |
| 0x01C20800 | 1 KiB  | PIO (GPIO)            | `allwinner-v3s-pio`                            |
| 0x01C20C00 | —      | A10 PIT (periodic)    | reused `allwinner-a10-pit`                     |
| 0x01C22C00 | —      | Audio codec (DAC)     | `allwinner-v3s-codec`                          |
| 0x01C25C00 | —      | CPUCFG                | reused `allwinner-cpucfg`                      |
| 0x01C28000 | —      | UART0 (16550)         | `serial_mm_init`                               |
| 0x01C28400 | —      | UART1                 | "                                              |
| 0x01C28800 | —      | UART2                 | "                                              |
| 0x01C60000 | —      | HSTimer               | `allwinner-v3s-hstimer`                        |
| 0x01C81000 | —      | GIC distributor       | `arm_gic`                                      |
| 0x01C82000 | —      | GIC CPU iface         | "                                              |
| 0x01D00000 | 64 KiB | SRAM C                | `memory_region_init_ram`                       |
| 0x40000000 | 64 MiB | DDR2 SDRAM            | `machine->ram` from `lichee_zero_init`         |

Stubs (registered as `create_unimplemented_device`): SPI0/1, OWA (SPDIF),
PWM, KEYADC, codec-analog, TWI0/1, USB OTG/PHY, CSI0.

## IRQ routing

GIC SPI numbers used by guest drivers (matches `jupiter v3s.h`):

| SPI | Source                |
|-----|-----------------------|
| 0   | UART0                 |
| 1   | UART1                 |
| 2   | UART2                 |
| 18  | A10 PIT timer 0       |
| 19  | A10 PIT timer 1       |
| 23  | SDMMC0                |
| 50  | DMA (shared, 8 chan)  |
| 51  | HSTimer ch0           |
| 52  | HSTimer ch1           |

The Cedar VE, codec, and display devices currently don't fire IRQs (the
guest polls them); virt-ym3438 doesn't use IRQs by design (it's
GPIO-driven only).

## How the guest reaches each device

### Memory-mapped (the easy case)

CPU stores/loads to MMIO ranges hit `MemoryRegionOps` callbacks the
device installs in its realize. Examples: every register-modeled device
above. The wrappers each device exports look like:

```c
static const MemoryRegionOps foo_ops = {
    .read  = foo_read,
    .write = foo_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_NATIVE_ENDIAN,
};
```

### GPIO pin fan-out

The PIO (`src/hw/gpio/allwinner-v3s-pio.c`) exposes both an input array
(`qdev_init_gpio_in`, one line per pin = port × 32 + pin) and an output
array (`qdev_init_gpio_out`, same indexing). Sibling devices wire to it
two ways:

- **Driving a pin from outside** (e.g. virt-n64-pad pulling PE20 low):
  `qdev_connect_gpio_out(pad, 0, qdev_get_gpio_in(pio, line))`. The PIO
  ORs `ext_in[port]` into `DAT` reads.
- **Observing host writes to a pin** (e.g. virt-ym3438 watching PG2 /WR
  edges): `qdev_connect_gpio_out(pio, line, qdev_get_gpio_in(observer,
  obs_line))`. The PIO recomputes the "effective output level" on every
  CFG/DAT write and fires `qemu_set_irq` only on actual transitions —
  not on every guest write to DAT. This is what makes Joybus and the
  YM3438 bus snooper work: a "pull line low" can be a pin direction
  change (output enable + DAT=0), not a DAT write.

### DMA

The audio path is the only DMA consumer in current payloads. The DMA
engine traps writes whose destination is `DAC_TXDATA` and forwards the
payload directly to the codec, rate-limited to
`samples / sample_rate` virtual seconds per descriptor so the guest's
double-buffer ISR keeps cycling instead of being starved or flooded.

### H.264 video

The Cedar VE device transforms the guest-issued register sequence (set
bitstream pointers, set output frame buffer pointers, trigger
slice-decode) into a `avcodec_send_packet` / `avcodec_receive_frame` call
on a host libavcodec H.264 decoder. The decoder is opened lazily once
the slice-header parameters are known so we can synthesize matching
SPS/PPS as `extradata`. NV12 output is written back to the guest-pointed
luma + chroma DRAM addresses recovered from the frame list at internal
SRAM `0x100`.

If `pkg-config` couldn't find libavcodec/libavutil at build time the
device compiles in stub mode — registers respond, slice-decode reports
success, but no pixels are written.

### Audio (codec + virt-ym3438)

Two independent audio paths:

1. **Codec** (`allwinner-v3s-codec`) — guest writes 16-bit samples to
   `DAC_TXDATA`, we forward to a `SWVoiceOut` opened against the host
   audio backend (PulseAudio / OSS / `none`). Guest's PLL_AUDIO setting
   is decoded to derive the actual sample rate per FIFOC `FS` field.
2. **virt-ym3438** (`src/hw/audio/virt-ym3438.c`) — wraps Nuked-OPN2
   behind the GPIO bus protocol the Jupiter `lib/ym3438_hw.c` driver
   bit-bangs. PB0..PB7 = data, PG0/PG1 = A0/A1, PG2 = /WR, PG3 = /CS,
   PG4 = /IC. Each /WR rising edge with /CS asserted latches one chip
   write; the audio cb pulls samples from Nuked-OPN2 via
   `nukedopn2_update`. Both paths get their own `QEMUSoundCard` and
   coexist.

## Why icount

The N64 Joybus protocol uses 4 µs bit cells — a "1" is 1 µs low + 3 µs
high, a "0" is 3 µs low + 1 µs high. The host distinguishes bits by
measuring the *low pulse width*, threshold 2 µs.

Without `-icount shift=auto`, QEMU's virtual clock advances in coarse
chunks (~20 µs at typical TCG rates), so every bit's low pulse rounds
to the same value and decoding fails. icount locks virtual time to
instruction count, which gives us sub-µs resolution at the cost of
running slower than wall clock. `scripts/run.sh` always passes it.

## What's *not* modeled (and why it's fine)

- **CCU bring-up sequencing** — the H3 CCU model just sets PLL_LOCK on
  read after write, which is all the guest polls for. Real PLL ramp
  timing isn't observable from software here.
- **Real interrupt vectors / GICv2 priorities** — guest drivers use the
  vanilla GIC pattern; no priority abuse.
- **TCON timing** — we don't simulate VBLANK or HSYNC. The guest queries
  a timer to time itself instead of waiting on TCON.
- **DRAM controller** — we hand `64 MiB at 0x40000000` and skip every
  byte of DRAM init the guest writes.
- **Allwinner crypto / CSI / usb / spi / i2c** — guest doesn't touch
  these.

## Source layout

Refer to [`README.md`](../README.md#repo-layout) for the directory tree.
Per-device internals are in [`docs/devices/`](devices/), one short page
per device.
