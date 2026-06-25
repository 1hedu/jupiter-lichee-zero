/*
 * Allwinner V3s Cedar VE (Video Engine) — H.264 decode + encode shim.
 *
 * Real silicon is a fixed-function MB-by-MB H.264 baseline codec
 * driven by register pokes; we substitute libavcodec on the host —
 * decode writes NV12 back into guest DRAM, encode reads an NV12 frame
 * from guest DRAM and writes the H.264 bitstream back — both where the
 * guest expects.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_ALLWINNER_V3S_CEDAR_H
#define HW_MISC_ALLWINNER_V3S_CEDAR_H

#include "qom/object.h"
#include "hw/sysbus.h"

#define TYPE_AW_V3S_CEDAR "allwinner-v3s-cedar"
OBJECT_DECLARE_SIMPLE_TYPE(AwV3sCedarState, AW_V3S_CEDAR)

#define AW_V3S_CEDAR_IOSIZE   0x1000   /* 4 KiB MMIO window */
#define AW_V3S_CEDAR_SRAM_BYTES 0x1000 /* internal SRAM (frame list etc.) */

struct AwV3sCedarState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[AW_V3S_CEDAR_IOSIZE / 4];

    /* Internal SRAM accessed via VE(0x2E0) addr / VE(0x2E4) data. The
     * guest stashes a frame list at SRAM offset 0x100; frame[3]/[4] are
     * the output luma/chroma DRAM addresses we write decoded NV12 to. */
    uint8_t  sram[AW_V3S_CEDAR_SRAM_BYTES];
    uint32_t sram_addr;

    /* libavcodec decoder context. Opaque void* so the header doesn't
     * have to pull <libavcodec/avcodec.h> into every translation unit
     * that includes us. */
    void    *avctx;     /* AVCodecContext* */
    void    *avframe;   /* AVFrame* */
    void    *avpkt;     /* AVPacket* */
    bool     have_codec;

    /* libavcodec H.264 *encoder* context. The guest's lib/cedar_enc.c
     * stages an NV12 frame in DRAM, pokes the AVC encoder register block
     * (0xA00..0xBC4), and triggers ENC_START; we run libx264 on the host
     * and write the resulting Annex-B bitstream back into the guest's
     * stream buffer. Reused across frames; rebuilt if w/h/qp change. */
    void    *enc_ctx;   /* AVCodecContext* */
    void    *enc_frame; /* AVFrame* */
    void    *enc_pkt;   /* AVPacket* */
    bool     have_enc;
    uint32_t enc_w;
    uint32_t enc_h;
    int      enc_qp;
    int64_t  enc_pts;   /* monotonic PTS counter for reused encoder */
};

#endif /* HW_MISC_ALLWINNER_V3S_CEDAR_H */
