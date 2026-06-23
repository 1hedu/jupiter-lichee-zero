/*
 * Allwinner V3s High-Speed Timer (HSTimer)
 *
 * Models the 2-channel 56-bit down-counter at 0x01C60000. Register map
 * matches Jupiter's hstimer.c. AHB clock assumed to be 200 MHz (5 ns per
 * tick) — the real clock depends on CCU_AHB divider, but the guest uses
 * the timer for relative timing only, so an exact rate isn't critical.
 *
 * Per-channel registers (offsets added to 0x10 + ch*0x20):
 *   +0x00 CTRL      bit 0: enable, bit 1: reload, bit 7: single-shot
 *   +0x04 INTV_LO   low 32 bits of 56-bit interval
 *   +0x08 INTV_HI   upper 24 bits (top byte unused)
 *   +0x0C CURNT_LO  current-count readback
 *   +0x10 CURNT_HI
 *
 * Global registers:
 *   +0x00 IRQ_EN    one bit per channel
 *   +0x04 IRQ_STAS  one bit per channel, w1c
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/timer/allwinner-v3s-hstimer.h"

#define AHB_CLK_HZ      200000000ULL
#define NS_PER_TICK     (1000000000ULL / AHB_CLK_HZ)  /* = 5 */

#define REG_IRQ_EN      0x00
#define REG_IRQ_STAS    0x04

#define CH_BASE(i)      (0x10 + (i) * 0x20)
#define CH_CTRL         0x00
#define CH_INTV_LO      0x04
#define CH_INTV_HI      0x08
#define CH_CURNT_LO     0x0C
#define CH_CURNT_HI     0x10

#define CTRL_ENABLE     (1u << 0)
#define CTRL_RELOAD     (1u << 1)
#define CTRL_PRESCALE   (7u << 4)
#define CTRL_SINGLE     (1u << 7)

static void hstimer_update_irq(AwV3sHsTimerState *s, int ch)
{
    int asserted = (s->irq_en & s->irq_stas & (1u << ch)) ? 1 : 0;
    qemu_set_irq(s->irq[ch], asserted);
}

static uint64_t hstimer_ch_ticks(const AwV3sHsTimerCh *c)
{
    return (((uint64_t)c->intv_hi & 0x00FFFFFFULL) << 32)
         | (uint64_t)c->intv_lo;
}

static uint64_t hstimer_current(const AwV3sHsTimerCh *c)
{
    if (!c->running) {
        return hstimer_ch_ticks(c);
    }
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    int64_t remaining_ns = c->fire_at_ns - now;
    if (remaining_ns <= 0) {
        return 0;
    }
    return (uint64_t)remaining_ns / NS_PER_TICK;
}

static void hstimer_arm(AwV3sHsTimerState *s, int i)
{
    AwV3sHsTimerCh *c = &s->ch[i];
    uint64_t ticks = hstimer_ch_ticks(c);
    uint64_t prescale_shift = (c->ctrl >> 4) & 0x7;
    if (prescale_shift > 4) {
        prescale_shift = 4;  /* hw caps at /16 */
    }

    c->period_ns = ticks * NS_PER_TICK * (1ULL << prescale_shift);
    if (c->period_ns == 0) {
        c->running = false;
        return;
    }
    c->fire_at_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
                  + (int64_t)c->period_ns;
    c->running = true;
    timer_mod(c->ptimer, c->fire_at_ns);
}

/* Per-channel dispatchers: each timer has a fixed channel index so the
 * callback can reach the state struct without back-pointers in the
 * channel sub-struct. */
typedef struct {
    AwV3sHsTimerState *parent;
    int channel;
} HsTimerDispatch;

static HsTimerDispatch g_dispatch[AW_V3S_HSTIMER_COUNT];

static void hstimer_fire(void *opaque)
{
    HsTimerDispatch *d = opaque;
    AwV3sHsTimerState *s = d->parent;
    int i = d->channel;
    AwV3sHsTimerCh *c = &s->ch[i];

    s->irq_stas |= (1u << i);
    hstimer_update_irq(s, i);

    if (c->ctrl & CTRL_SINGLE) {
        c->running = false;
        c->ctrl &= ~CTRL_ENABLE;
    } else {
        hstimer_arm(s, i);  /* re-arm for continuous mode */
    }
}

/* ---------- MMIO --------------------------------------------------- */

static uint64_t hstimer_read(void *opaque, hwaddr off, unsigned size)
{
    AwV3sHsTimerState *s = opaque;

    if (off == REG_IRQ_EN)   return s->irq_en;
    if (off == REG_IRQ_STAS) return s->irq_stas;

    for (int i = 0; i < AW_V3S_HSTIMER_COUNT; i++) {
        hwaddr base = CH_BASE(i);
        if (off >= base && off < base + 0x20) {
            hwaddr r = off - base;
            AwV3sHsTimerCh *c = &s->ch[i];
            switch (r) {
            case CH_CTRL:     return c->ctrl;
            case CH_INTV_LO:  return c->intv_lo;
            case CH_INTV_HI:  return c->intv_hi;
            case CH_CURNT_LO: return (uint32_t)hstimer_current(c);
            case CH_CURNT_HI: return (uint32_t)(hstimer_current(c) >> 32);
            }
        }
    }
    return 0;
}

static void hstimer_write(void *opaque, hwaddr off, uint64_t val,
                          unsigned size)
{
    AwV3sHsTimerState *s = opaque;

    if (off == REG_IRQ_EN) {
        s->irq_en = (uint32_t)val & 0x3;
        for (int i = 0; i < AW_V3S_HSTIMER_COUNT; i++) {
            hstimer_update_irq(s, i);
        }
        return;
    }
    if (off == REG_IRQ_STAS) {
        /* Write-1-to-clear */
        s->irq_stas &= ~((uint32_t)val & 0x3);
        for (int i = 0; i < AW_V3S_HSTIMER_COUNT; i++) {
            hstimer_update_irq(s, i);
        }
        return;
    }

    for (int i = 0; i < AW_V3S_HSTIMER_COUNT; i++) {
        hwaddr base = CH_BASE(i);
        if (off >= base && off < base + 0x20) {
            hwaddr r = off - base;
            AwV3sHsTimerCh *c = &s->ch[i];
            switch (r) {
            case CH_CTRL: {
                bool was_on = (c->ctrl & CTRL_ENABLE);
                c->ctrl = (uint32_t)val;
                bool now_on = (c->ctrl & CTRL_ENABLE);
                if (now_on && (c->ctrl & CTRL_RELOAD)) {
                    hstimer_arm(s, i);
                } else if (!now_on && was_on) {
                    timer_del(c->ptimer);
                    c->running = false;
                }
                break;
            }
            case CH_INTV_LO:  c->intv_lo = (uint32_t)val; break;
            case CH_INTV_HI:  c->intv_hi = (uint32_t)val; break;
            /* Current-count writes are ignored on real hardware. */
            }
            return;
        }
    }
}

static const MemoryRegionOps hstimer_ops = {
    .read = hstimer_read,
    .write = hstimer_write,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
    .impl  = { .min_access_size = 4, .max_access_size = 4 },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* ---------- QOM ---------------------------------------------------- */

static void aw_v3s_hstimer_reset(DeviceState *dev)
{
    AwV3sHsTimerState *s = AW_V3S_HSTIMER(dev);

    s->irq_en = 0;
    s->irq_stas = 0;
    for (int i = 0; i < AW_V3S_HSTIMER_COUNT; i++) {
        AwV3sHsTimerCh *c = &s->ch[i];
        if (c->ptimer) {
            timer_del(c->ptimer);
        }
        c->ctrl = c->intv_lo = c->intv_hi = 0;
        c->fire_at_ns = 0;
        c->period_ns = 0;
        c->running = false;
        qemu_set_irq(s->irq[i], 0);
    }
}

static void aw_v3s_hstimer_realize(DeviceState *dev, Error **errp)
{
    AwV3sHsTimerState *s = AW_V3S_HSTIMER(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &hstimer_ops, s,
                          "allwinner-v3s-hstimer", AW_V3S_HSTIMER_IOSIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    for (int i = 0; i < AW_V3S_HSTIMER_COUNT; i++) {
        g_dispatch[i].parent  = s;
        g_dispatch[i].channel = i;
        s->ch[i].ptimer = timer_new_ns(QEMU_CLOCK_VIRTUAL, hstimer_fire,
                                       &g_dispatch[i]);
        sysbus_init_irq(sbd, &s->irq[i]);
    }
}

static void aw_v3s_hstimer_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = aw_v3s_hstimer_realize;
    device_class_set_legacy_reset(dc, aw_v3s_hstimer_reset);
    dc->desc = "Allwinner V3s High-Speed Timer";
}

static const TypeInfo aw_v3s_hstimer_type_info = {
    .name          = TYPE_AW_V3S_HSTIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AwV3sHsTimerState),
    .class_init    = aw_v3s_hstimer_class_init,
};

static void aw_v3s_hstimer_register_types(void)
{
    type_register_static(&aw_v3s_hstimer_type_info);
}

type_init(aw_v3s_hstimer_register_types)
