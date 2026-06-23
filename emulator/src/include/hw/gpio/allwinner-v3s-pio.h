/*
 * Allwinner V3s Port I/O controller
 *
 * Pure GPIO register file at 0x01C20800. Seven ports (PB..PH in various
 * V3s revs; we expose the full 1KB window). No attached peripherals —
 * virtual gamepads / sensors / whatever plug in as separate devices and
 * drive pin inputs via qemu_irq lines.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_GPIO_ALLWINNER_V3S_PIO_H
#define HW_GPIO_ALLWINNER_V3S_PIO_H

#include "qom/object.h"
#include "hw/sysbus.h"

#define TYPE_AW_V3S_PIO "allwinner-v3s-pio"
OBJECT_DECLARE_SIMPLE_TYPE(AwV3sPioState, AW_V3S_PIO)

#define AW_V3S_PIO_IOSIZE       0x400
#define AW_V3S_PIO_NUM_PORTS    7      /* PB..PH */
#define AW_V3S_PIO_PINS_PER_PORT 32
#define AW_V3S_PIO_TOTAL_PINS   (AW_V3S_PIO_NUM_PORTS * AW_V3S_PIO_PINS_PER_PORT)

struct AwV3sPioState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[AW_V3S_PIO_IOSIZE / 4];

    /* Per-pin input state driven by external virtual peripherals.
     * Bits here OR into the pin's DAT register on read. Kept separate
     * from `regs` so guest writes don't clobber externally-driven bits. */
    uint32_t ext_in[AW_V3S_PIO_NUM_PORTS];

    /* Per-pin output notifications: fired when the effective "host
     * driving level" changes for a pin. A pin is host-driven low iff
     * CFG nibble == 0x1 (output mode) AND the DAT bit is 0. Otherwise
     * the line is released and we report level=1 (i.e. the pull-up, or
     * whatever external peripheral is attached). Peripherals watching a
     * pin subscribe to the matching gpio_out line. */
    qemu_irq pin_out[AW_V3S_PIO_TOTAL_PINS];

    /* Cached effective level mask per port so we can detect edges on
     * any write that could have moved them. */
    uint32_t eff_level[AW_V3S_PIO_NUM_PORTS];
};

#endif /* HW_GPIO_ALLWINNER_V3S_PIO_H */
