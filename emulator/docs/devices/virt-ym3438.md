# `virt-ym3438` — virtual YM3438 (Nuked-OPN2)

Source: [`src/hw/audio/virt-ym3438.c`](../../src/hw/audio/virt-ym3438.c)
Header: [`src/include/hw/audio/virt-ym3438.h`](../../src/include/hw/audio/virt-ym3438.h)
Backend: [`src/third_party/nuked_opn2/`](../../src/third_party/nuked_opn2/)

A virtual Yamaha YM3438 (OPN2C) FM synth, backed by **Nuked-OPN2**
(Alexey Khokholov's cycle-accurate emulator). Real Lichee Pi Zero
boards drive a physical YM3438 chip over GPIO bit-banging via a 74HCT245
buffer; this device snoops those GPIO transitions and feeds the same
register writes into Nuked-OPN2 instead.

This is not an MMIO device — it has no memory region.

## Wiring

The device exposes 13 gpio-in lines, all driven from the PIO's
`pin_out[]`:

| Pin index | Source pin | Function          |
|-----------|------------|-------------------|
| 0..7      | PB0..PB7   | Data bus D0..D7   |
| 8         | PG0        | A0                |
| 9         | PG1        | A1                |
| 10        | PG2        | /WR (active low)  |
| 11        | PG3        | /CS (active low)  |
| 12        | PG4        | /IC (reset, active low) |

Connections are made by the SoC realize:

```c
for (int p = 0; p < 8; p++)
    qdev_connect_gpio_out(pio_dev, 1 * 32 + p,           /* PB0..PB7 */
        qdev_get_gpio_in(ym_dev, VIRT_YM3438_PIN_D0 + p));
qdev_connect_gpio_out(pio_dev, 6 * 32 + 0, /* PG0 */ ...);  /* etc. */
```

## Bus-cycle reconstruction

The device keeps a 13-bit shadow of all input pins (`pin_state`).
Every `gpio_in` callback updates one bit in the shadow.

A YM3438 register write happens on a **/WR rising edge with /CS
asserted** (low). When that combination occurs:

```c
port = (A1 << 1) | A0;
data = pin_state & 0xFF;
nukedopn2_write(s->chip, port, data);
```

The four valid port values map to the YM3438's two address+data
register pairs:

| Port | A1 | A0 | Function                              |
|------|----|----|---------------------------------------|
| 0    | 0  | 0  | Address register, port 0 (channels 0–2) |
| 1    | 0  | 1  | Data register, port 0                   |
| 2    | 1  | 0  | Address register, port 1 (channels 3–5) |
| 3    | 1  | 1  | Data register, port 1                   |

A **/IC falling edge** (PG4 going low) calls `nukedopn2_reset_chip`.

## Audio output

- `AUD_register_card("virt-ym3438", ...)`
- `AUD_open_out` at 44.1 kHz stereo S16. Independent from the
  `allwinner-v3s-codec` voice — two host audio cards coexist.
- The pull callback chunks `len / 4` frames into 441-sample buffers,
  calls `nukedopn2_update(chip, n, channels)` to fill `bufL/bufR`,
  clamps to S16, interleaves, and `AUD_write`s.
- If `AUD_open_out` returns NULL (no audio backend configured), realize
  silently continues with `voice = NULL`. The pull callback no-ops on
  NULL voice; chip writes are still snooped and logged.

## Why GPIO snoop instead of MMIO

The real Lichee Pi Zero hardware drives a physical YM3438 chip via
GPIO bit-banging — there is no MMIO interface to substitute. To make
the same Jupiter binary run on the emulator, we hook the same GPIO
pins the bit-banger drives. This means the guest binary doesn't change.

The snoop is passive: if no payload is using the YM3438 (e.g. the menu),
the device never sees a /WR rising edge with /CS asserted, never
issues a chip write, and stays silent.

## Known limitations

- **Default clock = 7.670454 MHz** (NTSC OPN2 canonical). The Jupiter
  driver supports an 8 MHz clock variant from CSI_MCLK with F-Number
  scaling compensation, but we don't model the clock source — Nuked-
  OPN2 always runs at the canonical rate. F-Numbers from the guest
  reach the chip un-rescaled.
- **No mode flags.** `nukedopn2_set_options` isn't called; we get
  default Nuked behavior. Future tuning may want
  `ym3438_mode_ym2612 | ym3438_mode_readmode` for closer Sega Genesis
  fidelity.
- **No filter.** Nuked-OPN2's superctr MD1 filter (`FILTER_CUTOFF =
  0.512`) is part of the core but enabled per-instance. We don't
  toggle it; default-off output is brighter than a real Genesis.
- **Reset behavior:** the in-band /IC pulse triggers a `reset_chip`
  call; QEMU's `device_class_set_legacy_reset` also resets pin shadow
  to idle (control lines high, data bus 0).
