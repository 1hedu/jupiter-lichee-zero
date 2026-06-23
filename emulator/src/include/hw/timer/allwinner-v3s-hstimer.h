/*
 * Allwinner V3s High-Speed Timer (HSTimer)
 *
 * Two 56-bit down-counters clocked from AHB (~200 MHz). Used by Jupiter's
 * hstimer.c for scanline-accurate mid-frame interrupts.
 *
 * MMIO base 0x01C60000. Datasheet §4.6.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_TIMER_ALLWINNER_V3S_HSTIMER_H
#define HW_TIMER_ALLWINNER_V3S_HSTIMER_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"

#define TYPE_AW_V3S_HSTIMER "allwinner-v3s-hstimer"
OBJECT_DECLARE_SIMPLE_TYPE(AwV3sHsTimerState, AW_V3S_HSTIMER)

#define AW_V3S_HSTIMER_IOSIZE   0x100
#define AW_V3S_HSTIMER_COUNT    2

typedef struct AwV3sHsTimerCh {
    QEMUTimer *ptimer;
    uint32_t ctrl;           /* reg at +0x00 (in-channel) */
    uint32_t intv_lo;        /* +0x04 */
    uint32_t intv_hi;        /* +0x08 (upper 24 bits of 56-bit value) */
    int64_t  fire_at_ns;     /* vm_clock deadline for current expiry */
    uint64_t period_ns;      /* interval translated to ns */
    bool     running;
} AwV3sHsTimerCh;

struct AwV3sHsTimerState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    AwV3sHsTimerCh ch[AW_V3S_HSTIMER_COUNT];
    uint32_t irq_en;         /* +0x00 */
    uint32_t irq_stas;       /* +0x04 */
    qemu_irq irq[AW_V3S_HSTIMER_COUNT];
};

#endif /* HW_TIMER_ALLWINNER_V3S_HSTIMER_H */
