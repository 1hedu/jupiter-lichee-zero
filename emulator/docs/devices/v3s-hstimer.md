# `allwinner-v3s-hstimer` — High-Speed Timer

Source: [`src/hw/timer/allwinner-v3s-hstimer.c`](../../src/hw/timer/allwinner-v3s-hstimer.c)
Header: [`src/include/hw/timer/allwinner-v3s-hstimer.h`](../../src/include/hw/timer/allwinner-v3s-hstimer.h)

Two-channel 56-bit down-counter at `0x01C60000`. Used by Jupiter's
`hstimer.c` for raster-scanline interrupts and other sub-microsecond
timing.

## Register layout

### Global (at base)

| Offset | Name     | Description                                  |
|--------|----------|----------------------------------------------|
| 0x00   | `IRQ_EN` | One bit per channel; gates IRQ assertion     |
| 0x04   | `IRQ_STAS` | One bit per channel; w1c                   |

### Per-channel (at `0x10 + ch * 0x20`)

| Offset | Name       | Description                                         |
|--------|------------|-----------------------------------------------------|
| +0x00  | `CTRL`     | Bit 0 = enable; bit 1 = reload; bits 6:4 = prescale; bit 7 = single-shot |
| +0x04  | `INTV_LO`  | Low 32 bits of 56-bit interval                      |
| +0x08  | `INTV_HI`  | Upper 24 bits (top byte unused)                     |
| +0x0C  | `CURNT_LO` | Current-count readback (live)                       |
| +0x10  | `CURNT_HI` | "                                                   |

## Clock assumption

The AHB clock is hardcoded to **200 MHz** (5 ns/tick):

```c
#define AHB_CLK_HZ      200000000ULL
#define NS_PER_TICK     (1000000000ULL / AHB_CLK_HZ)  /* = 5 */
```

The real AHB rate depends on the CCU AHB divider, which Jupiter
programs but doesn't poll. Since the guest uses HSTimer for relative
timing only — set an interval, wait for IRQ — an exact rate match
isn't necessary.

The prescale field (bits 6:4 of CTRL) is honored: dividers 1, 2, 4, 8,
16, 32, 64, 128.

## Modes

- **Reload** (bit 1 set): on expiry, reload from `INTV_*` and restart.
  Fires IRQ once per period.
- **Single-shot** (bit 7 set): on expiry, fire IRQ once and stop.
- **Free-run** (neither bit): not used by Jupiter; we treat it as
  reload.

## IRQs

- Channel 0 → GIC SPI 51
- Channel 1 → GIC SPI 52

The `IRQ_EN` bit is the gate. `IRQ_STAS` is set when the timer expires
regardless of `IRQ_EN`; clearing it is w1c (write 1 to clear).

## Implementation

Each channel owns a `QEMUTimer` (virtual clock). Enable transitions
from 0→1 schedule the timer; disable cancels it. The `CURNT_*`
readbacks are computed live from `(fire_at_ns - now_ns) / NS_PER_TICK`
so reading mid-period shows a sane countdown.

## Known limitations

- **No frequency calibration.** The 200 MHz assumption is fixed; if a
  payload reprograms the AHB divider and times against wall-clock,
  it'll be off by the same factor.
- **Top byte of `INTV_HI` ignored.** Real HW uses 24 bits there; we
  mask to 24 bits explicitly. Periods up to ~28 minutes are
  representable; longer values are truncated.
