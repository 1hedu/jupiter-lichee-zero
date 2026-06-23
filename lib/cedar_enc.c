/*
 * Jupiter SDK — cedar_enc.c
 * CedarVE H.264 encoder for V3s, ported from Bootlin cedrus_enc_h264.c.
 *
 * Complete register sequence for I-frame encoding:
 *   1. Encoder reset + ISP enable
 *   2. Stream buffer init (STM registers)
 *   3. SPS/PPS/slice header via put_bits (exp-golomb, with polling)
 *   4. Buffer addresses (mb_info, subpix, rec, ref)
 *   5. Encoding parameters (PARA0/PARA1)
 *   6. Trigger + wait
 */
#include "jupiter.h"
#include <string.h>

#define VE_BASE  0x01C0E000
#define VE(off)  REG32(VE_BASE + (off))

/* Encoder buffers — all DRAM addresses used by VE are DMA (physical) */
#define ENC_STREAM_ADDR   0x43700000  /* must NOT overlap decoder's BUF_INPUT (0x43000000) */
#define ENC_STREAM_SIZE   0x00100000  /* 1MB */
#define ENC_NV12_Y        0x43100000
#define ENC_NV12_C        0x43200000
#define ENC_MB_INFO       0x43500000  /* 4KB */
#define ENC_SUBPIX        0x43510000  /* 256KB */
#define ENC_REC           0x43560000  /* 768KB */

/* Register offsets (from Bootlin cedrus_regs.h) */
#define AVC_PARA0         0xB04
#define AVC_PARA1         0xB08
#define AVC_PARA2         0xB0C
#define AVC_ME_PARA       0xB10
#define AVC_INT_EN        0xB14
#define AVC_TRIGGER       0xB18
#define AVC_STATUS        0xB1C
#define AVC_PUTBITS       0xB20
#define AVC_OVERTIME_MB   0xB24
#define AVC_CIR           0xB28
#define AVC_RESIDUAL_BITS 0xB54
#define AVC_HEADER_BITS   0xB58
#define AVC_MV_BUF        0xB60
#define AVC_STM_ADDR      0xB80
#define AVC_STM_END       0xB84
#define AVC_STM_OFF       0xB88
#define AVC_STM_MAX       0xB8C
#define AVC_STM_LEN       0xB90
#define AVC_REF0_Y        0xBA0
#define AVC_REF0_C        0xBA4
#define AVC_REC_Y         0xBB0
#define AVC_REC_C         0xBB4
#define AVC_SUBPIX_LAST   0xBB8
#define AVC_SUBPIX_NEW    0xBBC
#define AVC_MB_INFO       0xBC0
#define AVC_DEBLK         0xBC4

/* ISP registers */
#define ISP_PIC_INFO      0xA00
#define ISP_PIC_STRIDE    0xA04
#define ISP_CTRL          0xA08
#define ISP_SCALER_SIZE   0xA2C
#define ISP_INPUT_Y       0xA78
#define ISP_INPUT_C       0xA7C

/* Status bits */
#define STS_PUT_BITS_RDY  (1u << 9)
#define STS_FINISH        (1u << 0)
#define STS_STALL         (1u << 1)
#define STS_MASK          0xF

/* ================================================================== */
/* ARGB → NV12 conversion (writes to ENC_NV12_Y/C)                      */
/* ================================================================== */
void cedar_argb_to_nv12(const uint32_t *src, uint32_t src_pitch,
                        uint32_t w, uint32_t h)
{
    uint32_t stride = (w + 15) & ~15;
    uint8_t *Y  = (uint8_t *)ENC_NV12_Y;
    uint8_t *UV = (uint8_t *)ENC_NV12_C;

    for (uint32_t r = 0; r < h; r++) {
        for (uint32_t c = 0; c < w; c++) {
            uint32_t px = src[r * src_pitch + c];
            int R = (px >> 16) & 0xFF;
            int G = (px >> 8)  & 0xFF;
            int B =  px        & 0xFF;
            int y = ((66*R + 129*G + 25*B + 128) >> 8) + 16;
            if (y < 0) y = 0; if (y > 255) y = 255;
            Y[r * stride + c] = (uint8_t)y;

            if ((r & 1) == 0 && (c & 1) == 0) {
                int u = ((-38*R - 74*G + 112*B + 128) >> 8) + 128;
                int v = ((112*R - 94*G - 18*B + 128) >> 8) + 128;
                if (u < 0) u = 0; if (u > 255) u = 255;
                if (v < 0) v = 0; if (v > 255) v = 255;
                UV[(r/2) * stride + (c & ~1)]     = (uint8_t)u;
                UV[(r/2) * stride + (c & ~1) + 1] = (uint8_t)v;
            }
        }
        for (uint32_t c = w; c < stride; c++) Y[r * stride + c] = 16;
    }

    uint32_t luma_sz = stride * ((h + 15) & ~15);
    dcache_clean_range(ENC_NV12_Y, luma_sz);
    dcache_clean_range(ENC_NV12_C, luma_sz / 2);
}

/* ================================================================== */
/* put_bits with polling (Bootlin: cedrus_enc_h264_coded_append)        */
/* ================================================================== */
static void put_bits(uint32_t value, int nbits)
{
    /* Poll for PUT_BITS_READY */
    for (int i = 0; i < 100000; i++)
        if (VE(AVC_STATUS) & STS_PUT_BITS_RDY) break;

    VE(AVC_PUTBITS) = value;
    VE(AVC_TRIGGER) = ((nbits & 0x3F) << 8) | 1;  /* TYPE_PUT_BITS */
}

/* Exp-golomb unsigned */
static void put_ue(uint32_t v)
{
    /* ue(v) = prefix zeros + (v+1) in binary */
    int bits = 0;
    uint32_t tmp = v + 1;
    while (tmp > 1) { tmp >>= 1; bits++; }
    put_bits(v + 1, 2 * bits + 1);
}

/* Exp-golomb signed */
static void put_se(int v)
{
    uint32_t ue_v = (v > 0) ? (2 * v - 1) : (-2 * v);
    put_ue(ue_v);
}

static void put_byte(uint8_t v) { put_bits(v, 8); }
static void put_u32(uint32_t v) {
    put_byte((v >> 24) & 0xFF);
    put_byte((v >> 16) & 0xFF);
    put_byte((v >> 8) & 0xFF);
    put_byte(v & 0xFF);
}
static void put_bit(int v) { put_bits(v & 1, 1); }

static void put_align(void)
{
    uint32_t len = VE(AVC_STM_LEN);
    int rem = len % 8;
    if (rem) put_bits(0, 8 - rem);
}

/* ================================================================== */
/* Disable/enable emulation prevention three-byte                       */
/* ================================================================== */
static void eptb_set(int enable)
{
    uint32_t v = VE(AVC_PARA0);
    if (enable)
        v &= ~(1u << 31);  /* clear EPTB_DIS */
    else
        v |= (1u << 31);   /* set EPTB_DIS */
    VE(AVC_PARA0) = v;
}

/* ================================================================== */
/* Write SPS NAL                                                        */
/* ================================================================== */
static void write_sps(uint32_t mb_w, uint32_t mb_h)
{
    put_u32(0x00000001);            /* start code */
    put_byte(0x67);                 /* NAL: ref_idc=3, type=7 (SPS) */
    put_byte(66);                   /* profile_idc = baseline */
    put_byte(0xC0);                 /* constraint_set0+1 */
    put_byte(31);                   /* level_idc = 3.1 */
    put_ue(0);                      /* seq_parameter_set_id */
    put_ue(4);                      /* log2_max_frame_num_minus4 = 4 → log2=8 */
    put_ue(0);                      /* pic_order_cnt_type */
    put_ue(4);                      /* log2_max_pic_order_cnt_lsb_minus4 = 4 → log2=8 */
    put_ue(1);                      /* max_num_ref_frames */
    put_bit(0);                     /* gaps_in_frame_num_allowed */
    put_ue(mb_w - 1);              /* pic_width_in_mbs_minus1 */
    put_ue(mb_h - 1);              /* pic_height_in_map_units_minus1 */
    put_bit(1);                     /* frame_mbs_only */
    put_bit(0);                     /* direct_8x8_inference (baseline) */
    put_bit(0);                     /* frame_cropping */
    put_bit(0);                     /* vui_parameters_present */
    put_bit(1);                     /* rbsp_stop_one_bit */
    put_align();
}

/* ================================================================== */
/* Write PPS NAL                                                        */
/* ================================================================== */
static void write_pps(int qp)
{
    put_u32(0x00000001);            /* start code */
    put_byte(0x68);                 /* NAL: ref_idc=3, type=8 (PPS) */
    put_ue(0);                      /* pic_parameter_set_id */
    put_ue(0);                      /* seq_parameter_set_id */
    put_bit(0);                     /* entropy_coding_mode = CAVLC */
    put_bit(0);                     /* bottom_field_pic_order_in_frame_present */
    put_ue(0);                      /* num_slice_groups_minus1 */
    put_ue(0);                      /* num_ref_idx_l0_default_active_minus1 */
    put_ue(0);                      /* num_ref_idx_l1_default_active_minus1 */
    put_bit(0);                     /* weighted_pred */
    put_bits(0, 2);                 /* weighted_bipred_idc */
    put_se(qp - 26);               /* pic_init_qp_minus26 */
    put_se(qp - 26);               /* pic_init_qs_minus26 */
    put_se(0);                      /* chroma_qp_index_offset */
    put_bit(1);                     /* deblocking_filter_control_present */
    put_bit(0);                     /* constrained_intra_pred */
    put_bit(0);                     /* redundant_pic_cnt_present */
    put_bit(1);                     /* rbsp_stop_one_bit */
    put_align();
}

/* ================================================================== */
/* Write IDR slice header NAL                                           */
/* ================================================================== */
static void write_slice_header(int qp)
{
    put_u32(0x00000001);            /* start code */
    put_byte(0x65);                 /* NAL: ref_idc=3, type=5 (IDR) */
    put_ue(0);                      /* first_mb_in_slice */
    put_ue(2);                      /* slice_type = I */
    put_ue(0);                      /* pic_parameter_set_id */
    put_bits(0, 8);                 /* frame_num (8 bits, log2_max=8) */
    put_ue(0);                      /* idr_pic_id */
    put_bits(0, 8);                 /* pic_order_cnt_lsb (8 bits) */
    put_bit(0);                     /* no_output_of_prior_pics */
    put_bit(0);                     /* long_term_reference */
    put_se(0);                      /* slice_qp_delta = 0 (qp = pic_init_qp) */
    put_ue(1);                      /* disable_deblocking_filter_idc = 1 (disabled) */
    /* No alpha/beta since deblocking disabled */
}

/* ================================================================== */
/* H.264 I-frame encode — complete Bootlin-derived sequence             */
/* ================================================================== */
int cedar_h264_encode(uint32_t w, uint32_t h, int qp,
                      const uint8_t *unused_hdr, uint32_t unused_hdr_len)
{
    (void)unused_hdr; (void)unused_hdr_len;

    uint32_t mb_w = (w + 15) / 16;
    uint32_t mb_h = (h + 15) / 16;
    uint32_t stride = mb_w * 16;

    /* Rec buffer sizes (from Bootlin) */
    uint32_t rec_luma_sz = ((mb_w + 1) & ~1) * 16 * (((mb_h + 1) + 3) & ~3) * 16;
    uint32_t rec_chroma_sz = ((mb_w + 1) & ~1) * 16 * ((((mb_h + 1) / 2) + 3) & ~3) * 16;

    /* Clear buffers */
    memset((void *)ENC_STREAM_ADDR, 0xAA, 4096);  /* canary */
    memset((void *)ENC_MB_INFO, 0, 4096);
    memset((void *)ENC_SUBPIX, 0, 0x40000);
    memset((void *)ENC_REC, 0, rec_luma_sz + rec_chroma_sz);
    dcache_clean_range(ENC_STREAM_ADDR, ENC_STREAM_SIZE);
    dcache_clean_range(ENC_MB_INFO, 4096);
    dcache_clean_range(ENC_SUBPIX, 0x40000);
    dcache_clean_range(ENC_REC, rec_luma_sz + rec_chroma_sz);

    /* ---- Encoder init (from cedrus_enc.c lines 62-88) ---- */
    {
        uint32_t mode = VE(0x000);
        mode &= ~((1u << 7) | (1u << 6));
        mode |= 0x07;
        VE(0x000) = mode;

        VE(0x004) = VE(0x004) | (1u << 24);
        for (volatile int i = 0; i < 1000; i++);
        VE(0x004) = VE(0x004) & ~(1u << 24);
        for (volatile int i = 0; i < 1000; i++);

        mode = VE(0x000);
        mode |= (1u << 7) | (1u << 6) | 0x07;
        VE(0x000) = mode;
    }

    /* ---- ISP input (from cedrus_enc.c lines 131-177) ---- */
    VE(ISP_PIC_INFO) = ((mb_w & 0xFFFF) << 16) | (mb_h & 0xFFFF);
    VE(ISP_SCALER_SIZE) = ((mb_h & 0x7FF) << 16) | (mb_w & 0x7FF);
    VE(ISP_PIC_STRIDE) = ((stride / 16) & 0x7FF) << 16;
    VE(ISP_CTRL) = (0u << 27) | (0u << 20) | (0u << 1);  /* NV12, no rot, BT601 */
    VE(ISP_INPUT_Y) = ENC_NV12_Y;
    VE(ISP_INPUT_C) = ENC_NV12_C;

    /* ---- Stream buffer (lines 1116-1131) ---- */
    VE(AVC_TRIGGER) = 0;               /* reset trigger */
    VE(AVC_STM_OFF) = 0;
    VE(AVC_STM_ADDR) = ENC_STREAM_ADDR;
    VE(AVC_STM_END) = ENC_STREAM_ADDR + ENC_STREAM_SIZE - 1;
    VE(AVC_STM_MAX) = ENC_STREAM_SIZE * 8;
    VE(AVC_STM_LEN) = 0;
    VE(AVC_HEADER_BITS) = 0;
    VE(AVC_RESIDUAL_BITS) = 0;

    /* ---- Headers via put_bits (lines 1062-1098) ---- */
    eptb_set(0);  /* disable EPTB during headers */

    write_sps(mb_w, mb_h);
    write_pps(qp);
    write_slice_header(qp);

    eptb_set(1);  /* enable EPTB for macroblock data */

    /* Wait for sync idle */
    for (int i = 0; i < 100000; i++)
        if ((VE(0x004) & ((1u << 9) | (1u << 8))) == ((1u << 9) | (1u << 8)))
            break;

    /* ---- Buffers (lines 1139-1182) ---- */
    VE(AVC_MB_INFO) = ENC_MB_INFO;
    VE(AVC_MV_BUF) = 0;
    VE(AVC_REC_Y) = ENC_REC;
    VE(AVC_REC_C) = ENC_REC + rec_luma_sz;
    VE(AVC_REF0_Y) = ENC_REC;           /* I-frame: ref = rec */
    VE(AVC_REF0_C) = ENC_REC + rec_luma_sz;
    VE(AVC_SUBPIX_NEW) = ENC_SUBPIX;
    VE(AVC_SUBPIX_LAST) = ENC_SUBPIX;
    VE(AVC_DEBLK) = 0;
    VE(AVC_CIR) = 0;

    /* ---- Parameters (lines 1194-1224) ---- */
    VE(AVC_PARA0) = (0u << 4);  /* I-slice, CAVLC, frame, no deblk */

    uint32_t stride_div48 = (stride / 16 + 47) / 48;
    VE(AVC_PARA1) = ((0 & 7) << 16) |           /* chroma_qp_offset */
                    ((stride_div48 & 0xF) << 10) | /* stride_mbs_div_48 */
                    (0u << 6) |                    /* RC_MODE_FIXED */
                    (qp & 0x3F);                   /* fixed QP */
    VE(AVC_PARA2) = 0;

    /* Motion estimation (disabled for I-frame) */
    VE(AVC_ME_PARA) = (1u << 24) | (2u << 0);  /* WB_MV_INFO_DIS | FME_SEARCH_LEVEL(2) */

    /* Rate control + stats = 0 */
    VE(0xB30) = 0; VE(0xB34) = 0; VE(0xB38) = 0; VE(0xB3C) = 0; VE(0xB40) = 0;  /* RC */
    VE(0xB44) = 0;  /* MAD */
    VE(AVC_OVERTIME_MB) = 0;
    VE(0xB48) = 0;  /* ME_INFO */

    uart_puts("[cedar] enc trigger...\n");

    /* ---- Trigger (lines 1255-1269) ---- */
    VE(AVC_INT_EN) = (1u << 1) | (1u << 0);     /* STALL + FINISH */
    VE(AVC_TRIGGER) = (0u << 16) | 8;            /* H264 mode, ENC_START */

    /* Wait */
    uint32_t status, timeout = 50000000;
    do { status = VE(AVC_STATUS); }
    while (!(status & STS_MASK) && --timeout);

    uint32_t enc_bits = VE(AVC_STM_LEN);
    uint32_t enc_bytes = enc_bits / 8;

    /* Read registers BEFORE idle */
    uint32_t stm_addr_rb = VE(AVC_STM_ADDR);

    VE(AVC_STATUS) = STS_MASK;
    VE(0x000) = 0x00130007;

    dcache_invalidate_range(ENC_STREAM_ADDR, enc_bytes + 64);

    uart_puts("[cedar] enc status=0x"); uart_puthex(status);
    uart_puts(" len="); uart_putdec(enc_bytes);
    uart_puts("B stm_addr=0x"); uart_puthex(stm_addr_rb);

    /* Check first bytes */
    volatile uint8_t *out = (volatile uint8_t *)ENC_STREAM_ADDR;
    uart_puts(" [0..7]=");
    for (int i = 0; i < 8; i++) { uart_puthex(out[i]); uart_puts(" "); }
    uart_puts("\n");

    if (status & STS_FINISH) {
        uart_puts("[cedar] encode OK\n");
        return (int)enc_bytes;
    }
    uart_puts("[cedar] encode FAIL\n");
    return -1;
}
