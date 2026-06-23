/*
 * Allwinner V3s audio codec (DAC side only).
 *
 * MMIO at 0x01C22C00. The V3s codec has independent DAC and ADC paths;
 * we model the DAC because that's the path Jupiter's bare-metal code
 * uses to push mixed audio to the internal speaker. ADC can be added
 * later.
 *
 * The guest (or DMA acting on its behalf) writes 16- or 32-bit samples
 * into DAC_TXDATA. We buffer them into a ring and drain to QEMU's audio
 * subsystem at the configured sample rate.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_AUDIO_ALLWINNER_V3S_CODEC_H
#define HW_AUDIO_ALLWINNER_V3S_CODEC_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "audio/audio.h"

#define TYPE_AW_V3S_CODEC "allwinner-v3s-codec"
OBJECT_DECLARE_SIMPLE_TYPE(AwV3sCodecState, AW_V3S_CODEC)

#define AW_V3S_CODEC_IOSIZE   0x400  /* covers digital + analog control */

/* Ring big enough for ~500 ms at 48 kHz stereo = 96000 samples = 384 KB
 * — keep it modest, 32 KB lets ~340 ms at 48 kHz mono 16-bit. */
#define AW_V3S_CODEC_RING_BYTES   (32 * 1024)

struct AwV3sCodecState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[AW_V3S_CODEC_IOSIZE / 4];

    /* DAC state decoded from DAC_FIFOC. */
    uint32_t sample_rate_hz;
    bool     stereo;
    bool     bits24;          /* 32-bit word holds 24-bit sample */
    bool     fifo_mode_01;    /* 16-bit compact in TXDATA[15:0] */

    /* Ring buffer of decoded 16-bit samples ready for QEMU audio. Stored
     * in host (little) endian int16_t. */
    int16_t  ring[AW_V3S_CODEC_RING_BYTES / 2];
    uint32_t wr_idx;
    uint32_t rd_idx;
    uint32_t count;           /* bytes valid, in int16 units */

    /* QEMU audio subsystem handles. */
    QEMUSoundCard card;
    SWVoiceOut   *voice;
    struct audsettings as;

    /* Peer DMA engine — notified on sample-rate reprograms so it can
     * pace descriptor completions. */
    void *dma_peer;           /* AwV3sDmaState* but avoid header cycle */
};

/* Wire the codec to its DMA peer (lichee-zero board calls this). */
void aw_v3s_codec_attach_dma(AwV3sCodecState *s, void *dma);

#endif /* HW_AUDIO_ALLWINNER_V3S_CODEC_H */
