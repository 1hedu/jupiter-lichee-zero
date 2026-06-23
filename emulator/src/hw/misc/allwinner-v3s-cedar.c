/*
 * Allwinner V3s Cedar VE — H.264 baseline IDR decode via host libavcodec.
 *
 * Real silicon: Allwinner BSP cedar_ve. The guest pokes a sequence of
 * registers (clocks, bitstream pointer, frame list, output format),
 * triggers decode via VE(0x224)=8, polls VE(0x228) for completion,
 * then reads decoded NV12 from buffers it gave us in the frame list.
 *
 * Our model: track every register write. On the AVC_SLICE_DECODE
 * trigger, recover the bitstream from guest DRAM, hand it to libavcodec,
 * convert the resulting YUV420P frame into NV12 (interleaved UV plane),
 * write Y to luma and interleaved UV to chroma at the addresses the
 * guest stashed in our internal SRAM via the frame list at SRAM[0x100].
 * Set the success bit in VE(0x228) so the guest's poll loop exits.
 *
 * Out of scope: P/B-frames (Jupiter's vbins are all-IDR), reference
 * frame management, CABAC, deblocking selection, exact MB-by-MB timing.
 * libavcodec handles all that internally; the guest's "I just want a
 * decoded NV12 plane" expectation is what we satisfy.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "exec/address-spaces.h"
#include "hw/misc/allwinner-v3s-cedar.h"

#ifdef CONFIG_LIBAVCODEC
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavcodec/packet.h>
#endif

/* ------------------------------------------------------------------ */
/* VE register offsets the guest's lib/cedar.c writes/reads             */
/* ------------------------------------------------------------------ */
#define VE_CTRL              0x000
#define VE_VERSION           0x0F0
#define VE_NV12_FORMAT       0x0EC
#define VE_CHROMA_SIZE       0x0C4
#define VE_STRIDE            0x0C8
#define VE_FRAME_INDEX_HI    0x024C
#define VE_FRAME_DESC        0x0240
#define VE_FRAME_PIC_INFO    0x0250
#define VE_FRAME_NEIGHBOR    0x0254
#define VE_FRAME_INDEX       0x0050
#define VE_AVC_SHS_PARAM1    0x0200
#define VE_AVC_SHS_PARAM2    0x0204
#define VE_AVC_SHS_PARAM3    0x0208
#define VE_AVC_SHS_PARAM4    0x020C
#define VE_AVC_SHS_PARAM5    0x021C
#define VE_AVC_TRIGGER       0x0224     /* 7=INIT_SWDEC, 8=AVC_SLICE, 3=skip */
#define VE_AVC_STATUS        0x0228     /* w1c success/error/busy */
#define VE_AVC_IRQ_EN        0x0220
#define VE_AVC_VLD_OFFSET    0x0234
#define VE_AVC_VLD_LEN       0x0238     /* bits */
#define VE_AVC_VLD_END       0x023C
#define VE_AVC_VLD_ADDR      0x0230     /* VLD_ADDR_VAL encoded */
#define VE_SRAM_ADDR         0x02E0     /* SRAM auto-increment addr (word) */
#define VE_SRAM_DATA         0x02E4     /* SRAM data (write or read) */

/* Status bits in VE(0x228). */
#define VE_STAT_SUCCESS      (1u << 0)
#define VE_STAT_ERROR        (1u << 1)
#define VE_STAT_VLD_BUSY     (1u << 8)

/* Trigger codes in VE(0x224). */
#define VE_TRIG_AVC_DECODE   8
#define VE_TRIG_INIT_SWDEC   7
#define VE_TRIG_SKIP_BITS    3

/* Decode the VLD_ADDR_VAL encoding from cedar.c:
 *   ((addr) & 0x0FFFFFF0) | (addr >> 28)
 * to recover the original 32-bit DRAM byte address. */
static uint32_t vld_addr_decode(uint32_t v)
{
    return ((v & 0x0FFFFFF0u) | ((v & 0xFu) << 28));
}

/* ------------------------------------------------------------------ */
/* libavcodec scaffolding                                               */
/* ------------------------------------------------------------------ */

#ifdef CONFIG_LIBAVCODEC
/* ---- minimal H.264 baseline SPS + PPS bitstream synth -------------
 *
 * Jupiter's vbin format stores per-frame slice NALs only — no SPS/PPS.
 * Real Cedar VE doesn't need them in the stream because the guest pokes
 * those parameters through VE(0x200..0x21C) registers. libavcodec on
 * the host expects standard H.264 with SPS+PPS, so we synthesize them
 * from mb_w / mb_h / pic_init_qp once and feed them as extradata at
 * decoder open time.
 *
 * Baseline-only: profile_idc=66, no CABAC, no B-frames, frame_mbs_only=1,
 * no cropping, no VUI. Matches what wc1_video_pack.py asks x264 for.
 */

typedef struct {
    uint8_t buf[64];
    int     bit_off;
} bit_writer_t;

static void bw_init(bit_writer_t *w) {
    memset(w, 0, sizeof *w);
}
static void bw_put(bit_writer_t *w, uint32_t value, int nbits) {
    for (int i = nbits - 1; i >= 0; i--) {
        int bit = (value >> i) & 1;
        if (bit) w->buf[w->bit_off >> 3] |= (1u << (7 - (w->bit_off & 7)));
        w->bit_off++;
    }
}
static void bw_ue(bit_writer_t *w, uint32_t v) {
    /* Exp-Golomb unsigned: code(v) = (k zeros)(1)(k bits of v+1).
     * Equivalently: write v+1 in binary, prepend (bit_count - 1) zeros. */
    uint32_t x = v + 1;
    int n = 0;
    for (uint32_t t = x; t; t >>= 1) n++;
    for (int i = 0; i < n - 1; i++) bw_put(w, 0, 1);
    bw_put(w, x, n);
}
static int  bw_finish(bit_writer_t *w) {
    /* RBSP trailing: 1 stop bit, then zero-pad to byte. */
    bw_put(w, 1, 1);
    while (w->bit_off & 7) bw_put(w, 0, 1);
    return w->bit_off >> 3;
}

static int build_sps(uint8_t *out, uint32_t mb_w, uint32_t mb_h)
{
    bit_writer_t w; bw_init(&w);
    bw_put(&w, 0x67, 8);          /* NAL: ref_idc=3, type=7 (SPS) */
    bw_put(&w, 66, 8);            /* profile_idc = baseline */
    bw_put(&w, 0xC0, 8);          /* constraint_set0+1=1, others=0; reserved=0 */
    bw_put(&w, 30, 8);            /* level_idc = 3.0 */
    bw_ue(&w, 0);                 /* seq_parameter_set_id */
    bw_ue(&w, 0);                 /* log2_max_frame_num_minus4 */
    bw_ue(&w, 2);                 /* pic_order_cnt_type = 2 (no POC) */
    bw_ue(&w, 1);                 /* num_ref_frames */
    bw_put(&w, 0, 1);             /* gaps_in_frame_num_value_allowed */
    bw_ue(&w, mb_w - 1);          /* pic_width_in_mbs_minus1 */
    bw_ue(&w, mb_h - 1);          /* pic_height_in_map_units_minus1 */
    bw_put(&w, 1, 1);             /* frame_mbs_only_flag */
    bw_put(&w, 1, 1);             /* direct_8x8_inference_flag */
    bw_put(&w, 0, 1);             /* frame_cropping_flag */
    bw_put(&w, 0, 1);             /* vui_parameters_present_flag */
    int n = bw_finish(&w);
    memcpy(out, w.buf, n);
    return n;
}
static int build_pps(uint8_t *out, int pic_init_qp)
{
    bit_writer_t w; bw_init(&w);
    bw_put(&w, 0x68, 8);          /* NAL: ref_idc=3, type=8 (PPS) */
    bw_ue(&w, 0);                 /* pic_parameter_set_id */
    bw_ue(&w, 0);                 /* seq_parameter_set_id */
    bw_put(&w, 0, 1);             /* entropy_coding_mode_flag = 0 (CAVLC) */
    bw_put(&w, 0, 1);             /* bottom_field_pic_order_in_frame_present_flag */
    bw_ue(&w, 0);                 /* num_slice_groups_minus1 */
    bw_ue(&w, 0);                 /* num_ref_idx_l0_default_active_minus1 */
    bw_ue(&w, 0);                 /* num_ref_idx_l1_default_active_minus1 */
    bw_put(&w, 0, 1);             /* weighted_pred_flag */
    bw_put(&w, 0, 2);             /* weighted_bipred_idc */
    int qp_minus26 = pic_init_qp - 26;
    /* signed exp-Golomb: 2|x|, sign-shifted. */
    int abs_q = qp_minus26 < 0 ? -qp_minus26 : qp_minus26;
    int code = qp_minus26 <= 0 ? (-2 * qp_minus26) : (2 * qp_minus26 - 1);
    (void)abs_q;
    bw_ue(&w, (uint32_t)code);    /* pic_init_qp_minus26 */
    bw_ue(&w, 0);                 /* pic_init_qs_minus26 */
    bw_ue(&w, 0);                 /* chroma_qp_index_offset (signed=0 → ue=0) */
    bw_put(&w, 1, 1);             /* deblocking_filter_control_present_flag */
    bw_put(&w, 0, 1);             /* constrained_intra_pred_flag */
    bw_put(&w, 0, 1);             /* redundant_pic_cnt_present_flag */
    int n = bw_finish(&w);
    memcpy(out, w.buf, n);
    return n;
}

static void cedar_codec_open(AwV3sCedarState *s)
{
    if (s->have_codec) return;
    /* Need dimensions to build SPS. They come from VE(0x200) which the
     * guest must have written before triggering decode. */
    uint32_t param1 = s->regs[VE_AVC_SHS_PARAM1 / 4];
    uint32_t mb_w = ((param1 >> 8) & 0xFF) + 1;
    uint32_t mb_h = ((param1 >> 0) & 0xFF) + 1;
    if (mb_w < 2 || mb_h < 2 || mb_w > 64 || mb_h > 64) {
        return;  /* bogus dims; postpone codec open */
    }
    /* Pull pic_init_qp out of VE(0x21C) — low 6 bits of slice QP. */
    uint32_t param5 = s->regs[VE_AVC_SHS_PARAM5 / 4];
    int pic_init_qp = (int)(param5 & 0x3F);
    if (pic_init_qp < 1) pic_init_qp = 25;

    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) return;
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) return;

    /* Build SPS + PPS, slap them together with annex-B start codes,
     * hand to the codec as extradata so avcodec_open2 parses them. */
    static const uint8_t startcode[4] = { 0x00, 0x00, 0x00, 0x01 };
    uint8_t sps[64], pps[32];
    int sps_n = build_sps(sps, mb_w, mb_h);
    int pps_n = build_pps(pps, pic_init_qp);
    int total = (4 + sps_n) + (4 + pps_n);
    ctx->extradata = av_mallocz(total + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!ctx->extradata) { avcodec_free_context(&ctx); return; }
    ctx->extradata_size = total;
    uint8_t *p = ctx->extradata;
    memcpy(p, startcode, 4); p += 4; memcpy(p, sps, sps_n); p += sps_n;
    memcpy(p, startcode, 4); p += 4; memcpy(p, pps, pps_n);

    if (avcodec_open2(ctx, codec, NULL) < 0) {
        avcodec_free_context(&ctx);
        return;
    }
    s->avctx   = ctx;
    s->avframe = av_frame_alloc();
    s->avpkt   = av_packet_alloc();
    s->have_codec = (s->avframe && s->avpkt);
}

/* Run libavcodec on the bytes in `nal` (with annex-B start code already
 * present), copy the resulting NV12 plane pair into guest DRAM at
 * luma_addr / chroma_addr. width/height in pixels. Returns 0 on success. */
static int cedar_decode_frame(AwV3sCedarState *s,
                              const uint8_t *nal, uint32_t nal_len,
                              uint32_t width, uint32_t height,
                              uint32_t luma_addr, uint32_t chroma_addr)
{
    if (!s->have_codec) cedar_codec_open(s);
    if (!s->have_codec) return -1;

    AVCodecContext *ctx = (AVCodecContext *)s->avctx;
    AVFrame  *frame     = (AVFrame *)s->avframe;
    AVPacket *pkt       = (AVPacket *)s->avpkt;

    /* libavcodec's H.264 parser, in its default annex-B mode, requires
     * a 00 00 00 01 start code so it can find NAL boundaries. The two
     * paths in lib/cedar.c stage their bitstream differently:
     *   - cedar_video_av (NAL_STAGE) prepends start code itself; we
     *     read it back here unchanged.
     *   - cedar_h264_decode (BUF_INPUT) strips the start code via
     *     find_nal() and writes only the NAL payload.
     * Always prepend a fresh start code so both paths feed libavcodec
     * something it can split. Idempotent — extra start codes are fine,
     * the parser just notices an empty NAL between them. */
    static const uint8_t startcode[4] = { 0x00, 0x00, 0x00, 0x01 };
    uint8_t *buf = av_malloc(sizeof startcode + nal_len +
                             AV_INPUT_BUFFER_PADDING_SIZE);
    if (!buf) return -1;
    memcpy(buf, startcode, sizeof startcode);
    memcpy(buf + sizeof startcode, nal, nal_len);
    memset(buf + sizeof startcode + nal_len, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    pkt->data = buf;
    pkt->size = (int)(sizeof startcode + nal_len);

    int rc = avcodec_send_packet(ctx, pkt);
    if (rc < 0) {
        av_free(buf);
        return -1;
    }
    rc = avcodec_receive_frame(ctx, frame);
    av_free(buf);
    if (rc < 0) {
        return -1;
    }

    /* Output stride in MBs (16-byte aligned), matching the guest's
     * cedar_nv12_to_argb stride formula. */
    uint32_t stride = (width + 15) & ~15u;
    uint32_t mb_h   = ((height + 15) / 16) * 16;

    /* Luma: copy Y plane row-by-row at stride. */
    for (uint32_t r = 0; r < height && r < (uint32_t)frame->height; r++) {
        const uint8_t *src = frame->data[0] + r * frame->linesize[0];
        address_space_write(&address_space_memory, luma_addr + r * stride,
                            MEMTXATTRS_UNSPECIFIED, src, width);
    }
    /* Pad tail rows with zero up to mb_h. */
    for (uint32_t r = height; r < mb_h; r++) {
        static const uint8_t zero_row[2048] = { 0 };
        uint32_t n = width <= sizeof zero_row ? width : sizeof zero_row;
        address_space_write(&address_space_memory, luma_addr + r * stride,
                            MEMTXATTRS_UNSPECIFIED, zero_row, n);
    }

    /* Chroma: NV12 = interleaved UV at half height. ffmpeg gives us
     * separate U (data[1]) and V (data[2]) planes; weave one row at a
     * time into a small scratch buffer. */
    uint32_t ch_h = mb_h / 2;
    uint8_t row[4096];
    for (uint32_t r = 0; r < height / 2 && r < (uint32_t)frame->height / 2; r++) {
        const uint8_t *u = frame->data[1] + r * frame->linesize[1];
        const uint8_t *v = frame->data[2] + r * frame->linesize[2];
        uint32_t cw = width / 2;
        if (cw > sizeof row / 2) cw = sizeof row / 2;
        for (uint32_t c = 0; c < cw; c++) {
            row[c * 2 + 0] = u[c];
            row[c * 2 + 1] = v[c];
        }
        address_space_write(&address_space_memory, chroma_addr + r * stride,
                            MEMTXATTRS_UNSPECIFIED, row, cw * 2);
    }
    /* Pad chroma tail with neutral (0x80 = 128, "no color"). */
    static uint8_t neutral_row[4096];
    static int neutral_init;
    if (!neutral_init) { memset(neutral_row, 0x80, sizeof neutral_row); neutral_init = 1; }
    for (uint32_t r = height / 2; r < ch_h; r++) {
        uint32_t n = width <= sizeof neutral_row ? width : sizeof neutral_row;
        address_space_write(&address_space_memory, chroma_addr + r * stride,
                            MEMTXATTRS_UNSPECIFIED, neutral_row, n);
    }
    return 0;
}
#else
static int cedar_decode_frame(AwV3sCedarState *s,
                              const uint8_t *nal, uint32_t nal_len,
                              uint32_t width, uint32_t height,
                              uint32_t luma_addr, uint32_t chroma_addr)
{
    (void)s; (void)nal; (void)nal_len;
    (void)width; (void)height; (void)luma_addr; (void)chroma_addr;
    return -1;  /* libavcodec not compiled in — guest sees decode-failure pixels */
}
#endif

/* Recover the luma / chroma output addresses from the frame list the
 * guest wrote into our SRAM at offset 0x100. frame[3] = luma, frame[4]
 * = chroma. */
static uint32_t cedar_get_frame_addr(AwV3sCedarState *s, int idx)
{
    uint32_t off = 0x100 + idx * 4;
    if (off + 4 > sizeof s->sram) return 0;
    return ldl_le_p(s->sram + off);
}

/* Pull the H.264 NAL bytes from guest DRAM and run a decode. */
static void cedar_handle_avc_decode(AwV3sCedarState *s)
{
    uint32_t vld_addr_enc = s->regs[VE_AVC_VLD_ADDR / 4];
    uint32_t vld_bits     = s->regs[VE_AVC_VLD_LEN  / 4];
    uint32_t param1       = s->regs[VE_AVC_SHS_PARAM1 / 4];

    uint32_t bs_dram = vld_addr_decode(vld_addr_enc);
    uint32_t bs_len  = (vld_bits + 7) / 8;
    uint32_t mb_w    = ((param1 >> 8)  & 0xFF) + 1;
    uint32_t mb_h    = ((param1 >> 0)  & 0xFF) + 1;
    uint32_t width   = mb_w * 16;
    uint32_t height  = mb_h * 16;

    uint32_t luma   = cedar_get_frame_addr(s, 3);
    uint32_t chroma = cedar_get_frame_addr(s, 4);

    if (bs_len == 0 || bs_len > 16 * 1024 * 1024 || luma == 0) {
        s->regs[VE_AVC_STATUS / 4] |= VE_STAT_ERROR;
        return;
    }

    /* Read NAL bytes from guest DRAM. The guest's lib/cedar.c stages
     * "00 00 00 01 <NAL>" at NAL_STAGE before pointing VE_AVC_VLD_ADDR
     * at it, so what we read is annex-B-prefixed and ready to feed
     * libavcodec directly. */
    g_autofree uint8_t *nal = g_malloc(bs_len);
    address_space_read(&address_space_memory, bs_dram,
                       MEMTXATTRS_UNSPECIFIED, nal, bs_len);

    int rc = cedar_decode_frame(s, nal, bs_len, width, height, luma, chroma);
    if (rc == 0) {
        s->regs[VE_AVC_STATUS / 4] |= VE_STAT_SUCCESS;
    } else {
        /* Decode failed but mark "success" so the guest poll loop exits
         * cleanly; the frame just shows whatever was already in the NV12
         * buffers. Without this the guest's 50M-iteration poll burns
         * minutes of wall time per frame. */
        s->regs[VE_AVC_STATUS / 4] |= VE_STAT_SUCCESS;
    }
}

/* ------------------------------------------------------------------ */
/* MMIO                                                                 */
/* ------------------------------------------------------------------ */

static uint64_t cedar_read(void *opaque, hwaddr off, unsigned size)
{
    AwV3sCedarState *s = opaque;
    uint32_t idx = off / 4;
    if (idx >= AW_V3S_CEDAR_IOSIZE / 4) return 0;

    switch (off) {
    case VE_VERSION:
        /* Cedar VE on V3s reports 0x1680 (per Allwinner docs). */
        return 0x1680;
    case VE_AVC_STATUS:
        return s->regs[idx];
    case VE_SRAM_DATA: {
        uint32_t a = s->sram_addr;
        if (a + 4 > sizeof s->sram) return 0;
        uint32_t v = ldl_le_p(s->sram + a);
        s->sram_addr = a + 4;
        return v;
    }
    }
    return s->regs[idx];
}

static void cedar_write(void *opaque, hwaddr off, uint64_t val, unsigned size)
{
    AwV3sCedarState *s = opaque;
    uint32_t idx = off / 4;
    if (idx >= AW_V3S_CEDAR_IOSIZE / 4) return;
    uint32_t v = (uint32_t)val;

    switch (off) {
    case VE_AVC_STATUS:
        /* Write-1-to-clear. */
        s->regs[idx] &= ~v;
        return;
    case VE_SRAM_ADDR:
        /* Address comes in word-shifted (cedar.c does `off << 2`). */
        s->sram_addr = (v >> 2) & (AW_V3S_CEDAR_SRAM_BYTES - 1);
        return;
    case VE_SRAM_DATA: {
        uint32_t a = s->sram_addr & (AW_V3S_CEDAR_SRAM_BYTES - 1);
        if (a + 4 > sizeof s->sram) return;
        stl_le_p(s->sram + a, v);
        s->sram_addr = a + 4;
        return;
    }
    case VE_AVC_TRIGGER: {
        uint32_t op = v & 0xFF;
        if (op == VE_TRIG_AVC_DECODE) {
            cedar_handle_avc_decode(s);
        } else if (op == VE_TRIG_INIT_SWDEC || op == VE_TRIG_SKIP_BITS) {
            /* Nothing to do — the bitstream parser is libavcodec-internal.
             * Just clear busy and let the guest's wait_vld() exit. */
            s->regs[VE_AVC_STATUS / 4] &= ~VE_STAT_VLD_BUSY;
        }
        s->regs[idx] = v;
        return;
    }
    }
    s->regs[idx] = v;
}

static const MemoryRegionOps cedar_ops = {
    .read  = cedar_read,
    .write = cedar_write,
    .valid = { .min_access_size = 1, .max_access_size = 4 },
    .impl  = { .min_access_size = 4, .max_access_size = 4 },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* ------------------------------------------------------------------ */
/* QOM                                                                  */
/* ------------------------------------------------------------------ */

static void aw_v3s_cedar_reset(DeviceState *dev)
{
    AwV3sCedarState *s = AW_V3S_CEDAR(dev);
    memset(s->regs, 0, sizeof s->regs);
    memset(s->sram, 0, sizeof s->sram);
    s->sram_addr = 0;
    /* Don't free the libavcodec context; reuse across resets. */
}

static void aw_v3s_cedar_realize(DeviceState *dev, Error **errp)
{
    AwV3sCedarState *s = AW_V3S_CEDAR(dev);
    memory_region_init_io(&s->iomem, OBJECT(dev), &cedar_ops, s,
                          "allwinner-v3s-cedar", AW_V3S_CEDAR_IOSIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void aw_v3s_cedar_unrealize(DeviceState *dev)
{
#ifdef CONFIG_LIBAVCODEC
    AwV3sCedarState *s = AW_V3S_CEDAR(dev);
    if (s->avframe) av_frame_free((AVFrame **)&s->avframe);
    if (s->avpkt)   av_packet_free((AVPacket **)&s->avpkt);
    if (s->avctx)   avcodec_free_context((AVCodecContext **)&s->avctx);
#endif
}

static void aw_v3s_cedar_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize   = aw_v3s_cedar_realize;
    dc->unrealize = aw_v3s_cedar_unrealize;
    device_class_set_legacy_reset(dc, aw_v3s_cedar_reset);
    dc->desc = "Allwinner V3s Cedar VE (H.264 via libavcodec)";
}

static const TypeInfo aw_v3s_cedar_type_info = {
    .name          = TYPE_AW_V3S_CEDAR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AwV3sCedarState),
    .class_init    = aw_v3s_cedar_class_init,
};

static void aw_v3s_cedar_register_types(void)
{
    type_register_static(&aw_v3s_cedar_type_info);
}

type_init(aw_v3s_cedar_register_types)
