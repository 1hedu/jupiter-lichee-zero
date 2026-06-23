/*
 * Allwinner V3s DMA engine (sun8i family, linux-sunxi "sun6i-dma" compat).
 *
 * 8 channels, each a descriptor walker. Descriptors live in DRAM and
 * point to a src buffer, a dst buffer, a byte count, and an optional
 * "next" descriptor address for chaining / ring topology.
 *
 * For Jupiter's audio path: a pair of descriptors ping-pong between a
 * DRAM buffer and the codec's DAC_TXDATA register at 48 kHz. On each
 * descriptor end the engine fires a PKG interrupt so the guest can
 * refill the just-freed half.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_DMA_ALLWINNER_V3S_DMA_H
#define HW_DMA_ALLWINNER_V3S_DMA_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "exec/memory.h"

#define TYPE_AW_V3S_DMA "allwinner-v3s-dma"
OBJECT_DECLARE_SIMPLE_TYPE(AwV3sDmaState, AW_V3S_DMA)

#define AW_V3S_DMA_IOSIZE   0x400
#define AW_V3S_DMA_CHANNELS 8

typedef struct AwV3sDmaChan {
    uint32_t enable;      /* +0x00 */
    uint32_t pause;       /* +0x04 */
    uint32_t desc;        /* +0x08 — descriptor table address */
    uint32_t cfg;         /* +0x0C — mirrored from current descriptor */
    uint32_t cur_src;     /* +0x10 — current read address */
    uint32_t cur_dst;     /* +0x14 */
    uint32_t cur_bcnt;    /* +0x18 — remaining bytes */
    uint32_t para;        /* +0x1C */

    QEMUTimer *tmr;
    bool running;
} AwV3sDmaChan;

struct AwV3sDmaState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;

    /* Shared registers */
    uint32_t irq_en[2];       /* +0x00, +0x04 — 4 bits per channel */
    uint32_t irq_stat[2];     /* +0x10, +0x14 — w1c */
    uint32_t gate;            /* +0x20 */
    uint32_t status;          /* +0x30 */

    AwV3sDmaChan ch[AW_V3S_DMA_CHANNELS];
    qemu_irq irq;             /* shared SPI 50 */
    uint32_t sample_rate_hz;  /* current DAC sample rate (for rate-limit) */
};

/* Set by the codec device when the guest programs DAC_FIFOC with a new
 * sample rate. DMA reads this to pace its per-descriptor timer. */
void aw_v3s_dma_set_sample_rate(AwV3sDmaState *s, uint32_t rate_hz);

#endif /* HW_DMA_ALLWINNER_V3S_DMA_H */
