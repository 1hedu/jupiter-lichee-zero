/*
 * Allwinner V3s Port I/O controller
 *
 * Generic GPIO model. Per-port layout (0x24 bytes each, indexed PB=1..PH):
 *   +0x00 CFG0   function select, pins 0-7   (4 bits/pin)
 *   +0x04 CFG1   function select, pins 8-15
 *   +0x08 CFG2   function select, pins 16-23
 *   +0x0C CFG3   function select, pins 24-31
 *   +0x10 DAT    32-bit pin data
 *   +0x14 DRV0   drive strength, pins 0-15
 *   +0x18 DRV1   drive strength, pins 16-31
 *   +0x1C PULL0  pull up/down, pins 0-15
 *   +0x20 PULL1  pull up/down, pins 16-31
 *
 * DAT reads return (guest_written | external_input) so sibling devices
 * (virtual gamepads etc.) can drive pin levels via GPIO-in lines without
 * racing guest output writes. CFG/DRV/PULL are pure register storage;
 * we don't enforce input-vs-output direction on DAT readback — the guest
 * drivers pull inputs high externally, so the captured ext_in dominates.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/gpio/allwinner-v3s-pio.h"

#define PORT_STRIDE     0x24
#define PORT_CFG_BASE   0x00      /* CFG0..CFG3 at +0x00, 0x04, 0x08, 0x0C */
#define PORT_DAT_OFFSET 0x10

/* Compute the "effective output level" mask for one port.
 * Bit i = 1 when pin i is either not host-driven (CFG != output) or is
 * actively driven high (DAT bit i = 1). Bit i = 0 only when the host is
 * pulling the pin low (CFG = output AND DAT bit = 0). */
static uint32_t port_effective_level(const AwV3sPioState *s, int port)
{
    unsigned base = port * (PORT_STRIDE / 4);
    uint32_t cfg0 = s->regs[base + 0];
    uint32_t cfg1 = s->regs[base + 1];
    uint32_t cfg2 = s->regs[base + 2];
    uint32_t cfg3 = s->regs[base + 3];
    uint32_t dat  = s->regs[base + 4];

    uint32_t is_output = 0;
    for (int i = 0; i < 8; i++) {
        if (((cfg0 >> (i * 4)) & 0xF) == 0x1) is_output |= (1u << (i +  0));
        if (((cfg1 >> (i * 4)) & 0xF) == 0x1) is_output |= (1u << (i +  8));
        if (((cfg2 >> (i * 4)) & 0xF) == 0x1) is_output |= (1u << (i + 16));
        if (((cfg3 >> (i * 4)) & 0xF) == 0x1) is_output |= (1u << (i + 24));
    }
    /* driven_low = pin is output AND dat bit = 0
     * effective  = NOT driven_low (i.e. high whenever released). */
    uint32_t driven_low = is_output & ~dat;
    return ~driven_low;
}

static int offset_to_port(hwaddr off, unsigned *port_reg)
{
    int port = off / PORT_STRIDE;
    if (port >= AW_V3S_PIO_NUM_PORTS) {
        return -1;
    }
    *port_reg = off - (unsigned)(port * PORT_STRIDE);
    return port;
}

static uint64_t aw_v3s_pio_read(void *opaque, hwaddr off, unsigned size)
{
    AwV3sPioState *s = opaque;
    unsigned port_reg;
    int port;

    if (off + size > AW_V3S_PIO_IOSIZE) {
        return 0;
    }

    port = offset_to_port(off, &port_reg);
    if (port >= 0 && port_reg == PORT_DAT_OFFSET) {
        /* DAT reads include both guest-latched output level and any bits
         * currently asserted by an attached virtual peripheral. */
        return s->regs[off / 4] | s->ext_in[port];
    }
    return s->regs[off / 4];
}

static void aw_v3s_pio_write(void *opaque, hwaddr off, uint64_t val,
                             unsigned size)
{
    AwV3sPioState *s = opaque;
    unsigned port_reg;
    int port;

    if (off + size > AW_V3S_PIO_IOSIZE) {
        return;
    }

    s->regs[off / 4] = (uint32_t)val;

    /* Any write to CFG0..CFG3 or DAT of a port can change the effective
     * output level of one or more pins in that port. Recompute and fire
     * pin_out for each changed pin. */
    port = offset_to_port(off, &port_reg);
    if (port < 0 || port_reg > PORT_DAT_OFFSET) {
        return;
    }
    uint32_t new_level = port_effective_level(s, port);
    uint32_t changed   = new_level ^ s->eff_level[port];
    s->eff_level[port] = new_level;
    while (changed) {
        int b = ctz32(changed);
        changed &= changed - 1;
        int line = port * AW_V3S_PIO_PINS_PER_PORT + b;
        qemu_set_irq(s->pin_out[line], (new_level >> b) & 1);
    }
}

static const MemoryRegionOps aw_v3s_pio_ops = {
    .read  = aw_v3s_pio_read,
    .write = aw_v3s_pio_write,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
    .impl  = { .min_access_size = 4, .max_access_size = 4 },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/*
 * GPIO-in handler: attached peripherals call qemu_set_irq on line
 * (port * 32 + pin) to set/clear the external input level for that pin.
 */
static void aw_v3s_pio_gpio_in(void *opaque, int line, int level)
{
    AwV3sPioState *s = opaque;
    int port = line / AW_V3S_PIO_PINS_PER_PORT;
    int pin  = line % AW_V3S_PIO_PINS_PER_PORT;

    if (port >= AW_V3S_PIO_NUM_PORTS) {
        return;
    }
    if (level) {
        s->ext_in[port] |= (1u << pin);
    } else {
        s->ext_in[port] &= ~(1u << pin);
    }
}

static void aw_v3s_pio_reset(DeviceState *dev)
{
    AwV3sPioState *s = AW_V3S_PIO(dev);
    memset(s->regs, 0, sizeof s->regs);
    /* Effective level defaults to all-high (nothing driving low). */
    for (int i = 0; i < AW_V3S_PIO_NUM_PORTS; i++) {
        s->eff_level[i] = 0xFFFFFFFF;
    }
    /* Deliberately do NOT clear ext_in[]: those bits are owned by sibling
     * peripherals (virtual pads etc.) via gpio-in lines. Wiping them here
     * races the peripheral's own reset handler — in practice it leaves
     * "pull-up asserted" lines stuck low, breaking open-drain protocols
     * that depend on a high idle level (e.g. N64 data line). */
}

static void aw_v3s_pio_realize(DeviceState *dev, Error **errp)
{
    AwV3sPioState *s = AW_V3S_PIO(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &aw_v3s_pio_ops, s,
                          "allwinner-v3s-pio", AW_V3S_PIO_IOSIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    /* One input line per pin (ext_in drivers) plus a matching output per
     * pin (guest-written level changes). Both use line = port*32 + bit. */
    qdev_init_gpio_in(dev, aw_v3s_pio_gpio_in, AW_V3S_PIO_TOTAL_PINS);
    qdev_init_gpio_out(dev, s->pin_out, AW_V3S_PIO_TOTAL_PINS);
}

static void aw_v3s_pio_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = aw_v3s_pio_realize;
    device_class_set_legacy_reset(dc, aw_v3s_pio_reset);
    dc->desc = "Allwinner V3s PIO (GPIO controller)";
}

static const TypeInfo aw_v3s_pio_type_info = {
    .name          = TYPE_AW_V3S_PIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AwV3sPioState),
    .class_init    = aw_v3s_pio_class_init,
};

static void aw_v3s_pio_register_types(void)
{
    type_register_static(&aw_v3s_pio_type_info);
}

type_init(aw_v3s_pio_register_types)
