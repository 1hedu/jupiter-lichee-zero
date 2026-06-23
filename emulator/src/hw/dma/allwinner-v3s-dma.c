/*
 * Allwinner V3s DMA engine (8 channels, sun8i family).
 *
 * A descriptor is a 24-byte struct in DRAM:
 *   +0x00 uint32_t cfg         -- src_drq, src_width, src_burst, dst_* bits
 *   +0x04 uint32_t src_addr
 *   +0x08 uint32_t dst_addr
 *   +0x0C uint32_t byte_cnt
 *   +0x10 uint32_t param
 *   +0x14 uint32_t next        -- 0 for chain end
 *
 * Per-channel CUR_SRC/CUR_DST/CUR_BCNT registers update as the engine
 * moves through a descriptor; guest ISRs read CUR_SRC to detect which
 * half of a ring-buffer just drained.
 *
 * Rate-limiting: we'd transfer bytes instantly in virtual time, which
 * starves guest audio refill loops and corrupts sequencing. Instead we
 * hold the descriptor for
 *   samples / sample_rate   seconds
 * then fire the PKG interrupt and advance to the next descriptor.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "exec/address-spaces.h"
#include "hw/dma/allwinner-v3s-dma.h"

#define REG_IRQ_EN0     0x00
#define REG_IRQ_EN1     0x04
#define REG_IRQ_STAT0   0x10
#define REG_IRQ_STAT1   0x14
#define REG_GATE        0x20
#define REG_STATUS      0x30

#define CH_BASE(n)      (0x100 + (n) * 0x40)
#define CH_EN_OFF       0x00
#define CH_PAUSE_OFF    0x04
#define CH_DESC_OFF     0x08
#define CH_CFG_OFF      0x0C
#define CH_CUR_SRC_OFF  0x10
#define CH_CUR_DST_OFF  0x14
#define CH_BCNT_OFF     0x18
#define CH_PARA_OFF     0x1C

/* IRQ types, 4 bits per channel in IRQ_EN/IRQ_STAT registers. */
#define IRQ_HALF        (1u << 0)
#define IRQ_PKG         (1u << 1)
#define IRQ_QUEUE       (1u << 2)

/* Descriptor in guest DRAM. */
typedef struct QEMU_PACKED {
    uint32_t cfg;
    uint32_t src;
    uint32_t dst;
    uint32_t byte_cnt;
    uint32_t param;
    uint32_t next;
} AwV3sDmaDesc;

/* Per-channel back-pointer to state + channel index. Needed because the
 * timer callback only gets a single opaque, and pointer arithmetic from
 * &state->ch[0] breaks for any channel other than 0. */
typedef struct {
    AwV3sDmaState *parent;
    int            channel;
} DmaChanCtx;

static DmaChanCtx g_ctx[AW_V3S_DMA_CHANNELS];

/* ---------- helpers ------------------------------------------------- */

static void update_irq(AwV3sDmaState *s)
{
    bool asserted = (s->irq_en[0] & s->irq_stat[0]) ||
                    (s->irq_en[1] & s->irq_stat[1]);
    qemu_set_irq(s->irq, asserted ? 1 : 0);
}

static int ch_irq_bit_index(int ch, int type_shift)
{
    /* IRQ_EN/IRQ_STAT layout: 4 bits per channel, channels 0..3 in reg 0,
     * channels 4..7 in reg 1. type_shift is 0..3 for half/pkg/queue. */
    return (ch % 4) * 4 + type_shift;
}

static void ch_fire_irq(AwV3sDmaState *s, int ch, int type_bit)
{
    int reg   = ch / 4;
    int shift = 0;
    if      (type_bit == IRQ_HALF)  shift = 0;
    else if (type_bit == IRQ_PKG)   shift = 1;
    else if (type_bit == IRQ_QUEUE) shift = 2;
    int bit = ch_irq_bit_index(ch, shift);
    s->irq_stat[reg] |= (1u << bit);
    update_irq(s);
}

static int cfg_width_bytes(uint32_t cfg, int is_dst)
{
    /* Width is in the "9/25" bits: src width at bits 10:9, dst at 26:25.
     * Encoding: 0=8bit, 1=16bit, 2=32bit. */
    int shift = is_dst ? 25 : 9;
    uint32_t w = (cfg >> shift) & 0x3;
    return 1 << w;
}

/* ---------- descriptor walker --------------------------------------- */

static void ch_load_desc(AwV3sDmaState *s, int ch_i, hwaddr addr);
static void ch_descriptor_complete(AwV3sDmaState *s, int ch_i);

static void ch_start_transfer(AwV3sDmaState *s, int ch_i)
{
    AwV3sDmaChan *c = &s->ch[ch_i];

    if (c->cur_bcnt == 0) {
        ch_descriptor_complete(s, ch_i);
        return;
    }

    /*
     * Transfer is instant in the byte-movement sense: copy the whole
     * range from src to dst now, feeding the codec its samples so host
     * playback can start draining immediately. Rate-limiting then holds
     * CUR_SRC at desc.src for `samples / sample_rate` seconds before
     * the timer fires ch_descriptor_complete.
     *
     * Holding CUR_SRC at desc.src (instead of past desc-end) matters:
     * the guest's polling refill in audio.c keys off CUR_SRC to pick
     * which half to refill, and toggling between A.src / B.src is what
     * makes that toggle work. Pre-advancing breaks it.
     */
    uint32_t bytes = c->cur_bcnt;
    uint32_t width = cfg_width_bytes(c->cfg, 0);

    for (uint32_t off = 0; off < bytes; off += width) {
        uint32_t word = 0;
        address_space_read(&address_space_memory, c->cur_src + off,
                           MEMTXATTRS_UNSPECIFIED, &word, width);
        bool dst_io = (c->cfg >> 21) & 1;
        hwaddr dst = dst_io ? c->cur_dst : c->cur_dst + off;
        address_space_write(&address_space_memory, dst,
                            MEMTXATTRS_UNSPECIFIED, &word, width);
    }

    /* Track remaining-bytes for readback; don't move cur_src/cur_dst. */
    c->cur_bcnt = 0;

    /* Rate-limit completion to the configured sample rate. */
    uint32_t rate = s->sample_rate_hz ? s->sample_rate_hz : 48000;
    uint64_t samples = bytes / width;
    uint64_t dur_ns = (samples * 1000000000ULL) / rate;

    timer_mod(c->tmr,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + (int64_t)dur_ns);
}

static void ch_load_desc(AwV3sDmaState *s, int ch_i, hwaddr addr)
{
    AwV3sDmaChan *c = &s->ch[ch_i];
    AwV3sDmaDesc d;

    if (addr == 0xFFFFFFFF || addr == 0) {
        c->running = false;
        return;
    }

    address_space_read(&address_space_memory, addr,
                       MEMTXATTRS_UNSPECIFIED, &d, sizeof d);

    c->cfg       = d.cfg;
    c->cur_src   = d.src;
    c->cur_dst   = d.dst;
    c->cur_bcnt  = d.byte_cnt;
    c->para      = d.param;
    c->desc      = (uint32_t)addr;  /* remember so "next" relative loops work */

    /* Stash the *next* descriptor pointer in the channel's desc slot so
     * ch_descriptor_complete knows where to go. Reusing c->desc for this
     * because on real HW the desc register advances to the next desc as
     * chaining proceeds. */
    c->enable = 1;  /* mark active */

    /* Kick off the actual data movement. */
    /* Overwrite desc with the NEXT pointer so completion can chain. */
    /* (c->desc is public via CH_DESC register; match that expectation.) */
    c->desc = d.next ? d.next : 0xFFFFFFFF;

    ch_start_transfer(s, ch_i);
}

static void ch_descriptor_complete(AwV3sDmaState *s, int ch_i)
{
    AwV3sDmaChan *c = &s->ch[ch_i];

    if (!c->running) {
        return;
    }

    /* Load next descriptor BEFORE firing IRQ so CUR_SRC reflects the
     * position the guest's ISR and polling helpers expect — "already in
     * the next buffer, so the one that just finished is free". Without
     * this reordering, guests see CUR_SRC pointing into the same half
     * that just completed and refill the wrong one. */
    uint32_t nxt = c->desc;
    if (nxt && nxt != 0xFFFFFFFF) {
        ch_load_desc(s, ch_i, nxt);
        ch_fire_irq(s, ch_i, IRQ_PKG);
    } else {
        ch_fire_irq(s, ch_i, IRQ_PKG);
        c->running = false;
        c->enable = 0;
    }
}

/* Completion timer dispatcher — opaque points to DmaChanCtx in g_ctx[]. */
static void ch_timer_fire(void *opaque)
{
    DmaChanCtx *ctx = opaque;
    ch_descriptor_complete(ctx->parent, ctx->channel);
}

/* ---------- MMIO --------------------------------------------------- */

static uint64_t dma_read(void *opaque, hwaddr off, unsigned size)
{
    AwV3sDmaState *s = opaque;

    switch (off) {
    case REG_IRQ_EN0:   return s->irq_en[0];
    case REG_IRQ_EN1:   return s->irq_en[1];
    case REG_IRQ_STAT0: return s->irq_stat[0];
    case REG_IRQ_STAT1: return s->irq_stat[1];
    case REG_GATE:      return s->gate;
    case REG_STATUS:    return s->status;
    }

    for (int i = 0; i < AW_V3S_DMA_CHANNELS; i++) {
        hwaddr base = CH_BASE(i);
        if (off >= base && off < base + 0x40) {
            hwaddr r = off - base;
            AwV3sDmaChan *c = &s->ch[i];
            switch (r) {
            case CH_EN_OFF:      return c->enable;
            case CH_PAUSE_OFF:   return c->pause;
            case CH_DESC_OFF:    return c->desc;
            case CH_CFG_OFF:     return c->cfg;
            case CH_CUR_SRC_OFF: return c->cur_src;
            case CH_CUR_DST_OFF: return c->cur_dst;
            case CH_BCNT_OFF:    return c->cur_bcnt;
            case CH_PARA_OFF:    return c->para;
            }
        }
    }
    return 0;
}

static void dma_write(void *opaque, hwaddr off, uint64_t val, unsigned size)
{
    AwV3sDmaState *s = opaque;

    switch (off) {
    case REG_IRQ_EN0:
        s->irq_en[0] = (uint32_t)val;
        update_irq(s);
        return;
    case REG_IRQ_EN1:
        s->irq_en[1] = (uint32_t)val;
        update_irq(s);
        return;
    case REG_IRQ_STAT0:
        s->irq_stat[0] &= ~(uint32_t)val;    /* write-1-to-clear */
        update_irq(s);
        return;
    case REG_IRQ_STAT1:
        s->irq_stat[1] &= ~(uint32_t)val;
        update_irq(s);
        return;
    case REG_GATE:
        s->gate = (uint32_t)val;
        return;
    }

    for (int i = 0; i < AW_V3S_DMA_CHANNELS; i++) {
        hwaddr base = CH_BASE(i);
        if (off >= base && off < base + 0x40) {
            hwaddr r = off - base;
            AwV3sDmaChan *c = &s->ch[i];
            switch (r) {
            case CH_EN_OFF: {
                bool on  = (val & 1);
                bool was = c->running;
                if (on && !was && c->desc) {
                    c->running = true;
                    ch_load_desc(s, i, c->desc);
                } else if (!on && was) {
                    c->running = false;
                    c->enable = 0;
                    timer_del(c->tmr);
                }
                return;
            }
            case CH_PAUSE_OFF: c->pause = (uint32_t)val; return;
            case CH_DESC_OFF:  c->desc  = (uint32_t)val; return;
            case CH_CFG_OFF:   c->cfg   = (uint32_t)val; return;
            case CH_PARA_OFF:  c->para  = (uint32_t)val; return;
            /* CUR_* and BCNT are read-only on real hardware. */
            }
            return;
        }
    }
}

static const MemoryRegionOps dma_ops = {
    .read  = dma_read,
    .write = dma_write,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
    .impl  = { .min_access_size = 4, .max_access_size = 4 },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* ---------- public API --------------------------------------------- */

void aw_v3s_dma_set_sample_rate(AwV3sDmaState *s, uint32_t rate_hz)
{
    s->sample_rate_hz = rate_hz ? rate_hz : 48000;
}

/* ---------- QOM ---------------------------------------------------- */

static void aw_v3s_dma_reset(DeviceState *dev)
{
    AwV3sDmaState *s = AW_V3S_DMA(dev);
    for (int i = 0; i < AW_V3S_DMA_CHANNELS; i++) {
        AwV3sDmaChan *c = &s->ch[i];
        if (c->tmr) {
            timer_del(c->tmr);
        }
        /* Clear register/state fields explicitly — can't memset the whole
         * struct because that'd zero c->tmr (not the last field). */
        c->enable   = c->pause = c->desc = c->cfg = 0;
        c->cur_src  = c->cur_dst = c->cur_bcnt = c->para = 0;
        c->running  = false;
    }
    s->irq_en[0] = s->irq_en[1] = 0;
    s->irq_stat[0] = s->irq_stat[1] = 0;
    s->gate = 0;
    s->status = 0;
    s->sample_rate_hz = 48000;
    qemu_set_irq(s->irq, 0);
}

static void aw_v3s_dma_realize(DeviceState *dev, Error **errp)
{
    AwV3sDmaState *s = AW_V3S_DMA(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &dma_ops, s,
                          "allwinner-v3s-dma", AW_V3S_DMA_IOSIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    for (int i = 0; i < AW_V3S_DMA_CHANNELS; i++) {
        g_ctx[i].parent  = s;
        g_ctx[i].channel = i;
        s->ch[i].tmr = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                    ch_timer_fire, &g_ctx[i]);
    }
}

static void aw_v3s_dma_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = aw_v3s_dma_realize;
    device_class_set_legacy_reset(dc, aw_v3s_dma_reset);
    dc->desc = "Allwinner V3s DMA engine (8-channel)";
}

static const TypeInfo aw_v3s_dma_type_info = {
    .name          = TYPE_AW_V3S_DMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AwV3sDmaState),
    .class_init    = aw_v3s_dma_class_init,
};

static void aw_v3s_dma_register_types(void)
{
    type_register_static(&aw_v3s_dma_type_info);
}

type_init(aw_v3s_dma_register_types)
