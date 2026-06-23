# `allwinner-v3s-pio` — Port I/O (GPIO)

Source: [`src/hw/gpio/allwinner-v3s-pio.c`](../../src/hw/gpio/allwinner-v3s-pio.c)
Header: [`src/include/hw/gpio/allwinner-v3s-pio.h`](../../src/include/hw/gpio/allwinner-v3s-pio.h)

Generic GPIO controller at `0x01C20800`. Models 7 ports (PB..PH), each
with 32 pins. The interesting part isn't the register file — it's the
**effective-level fan-out** that lets sibling devices treat the PIO as
a real GPIO bus.

## Register layout (per port, 36-byte stride)

| Offset | Name   | Description                                  |
|--------|--------|----------------------------------------------|
| +0x00  | CFG0   | Function select, pins 0–7 (4 bits/pin)       |
| +0x04  | CFG1   | Function select, pins 8–15                   |
| +0x08  | CFG2   | Function select, pins 16–23                  |
| +0x0C  | CFG3   | Function select, pins 24–31                  |
| +0x10  | DAT    | 32-bit pin data                              |
| +0x14  | DRV0   | Drive strength, pins 0–15                    |
| +0x18  | DRV1   | Drive strength, pins 16–31                   |
| +0x1C  | PULL0  | Pull up/down, pins 0–15                      |
| +0x20  | PULL1  | Pull up/down, pins 16–31                     |

Port indexing in our flat register array: `regs[port * 9 + reg/4]`
with port 0 = PB, port 1 = PC, …, port 6 = PH.

CFG nibble values are stored verbatim. `0x1` = output, anything else =
not-output (inputs / alternate functions).

DRV and PULL are pure register storage.

## DAT semantics

- **Writes** to DAT update `regs[]` directly.
- **Reads** from DAT return `regs[port_dat] | ext_in[port]`. The
  `ext_in[]` array tracks bits asserted by sibling peripherals via
  gpio-in lines (see fan-out below). This way a virtual gamepad can
  drive a pin high without racing guest output writes.

## Fan-out: `port_effective_level()`

When the guest writes any of CFG0..CFG3 or DAT, the device recomputes
the **effective output level** for the whole port:

```
is_output      = bits where CFG nibble == 0x1
driven_low     = is_output & ~DAT
effective_level = ~driven_low
```

In other words: a pin is host-driving-low if and only if it's both
configured as output *and* its DAT bit is 0. Otherwise the host has
released the line and we report level 1 (the pull-up wins, or whatever
external peripheral has asserted ext_in).

Bits that changed since the last write fire `qemu_set_irq(pin_out[…])`.
**Every write that could move a level fires; identical writes don't.**
This is what allows the Joybus protocol decoder in `virt-n64-pad` to
see "the host pulled the line low" as a single edge — even though on
real hardware "pull low" means flipping CFG to output-enable while DAT
already sat at 0.

## Reset behavior

`device_class_set_legacy_reset` clears the register array and seeds
`eff_level[port] = 0xFFFFFFFF` (all pins released). It deliberately
does **not** clear `ext_in[]`: those bits are owned by sibling devices,
and wiping them races those devices' own reset handlers. In practice,
clearing ext_in here was breaking the N64 idle-high state on warm
reboots.

## How siblings wire in

Two patterns:

**Driving a pin from outside** (virt-n64-pad pulling PE20 low):
```c
qdev_connect_gpio_out(pad, 0,
    qdev_get_gpio_in(DEVICE(pio), port_E * 32 + 20));
```
The pad's `qemu_set_irq(line, 0)` clears `ext_in[E]` bit 20; reads of
PE_DAT see the bit clear.

**Observing host writes** (virt-ym3438 watching PG2 = /WR):
```c
qdev_connect_gpio_out(DEVICE(pio), port_G * 32 + 2,
    qdev_get_gpio_in(ym3438, VIRT_YM3438_PIN_nWR));
```
Every effective-level transition on PG2 fires the YM3438 device's
gpio-in handler.

A device that needs both attaches both lines.

## Known limitations

- We don't enforce direction on DAT reads beyond what's described
  above. A pin configured as alt-function still has its `ext_in` ORed
  into DAT reads. No payload has cared.
- DRV and PULL register state is not consulted anywhere — it's stored
  for future use (e.g. simulating open-drain + pull-up handshake more
  literally).
- `AW_V3S_PIO_NUM_PORTS = 7` — covers PB through PH. PA exists in some
  V3s revs but Jupiter doesn't use it.
