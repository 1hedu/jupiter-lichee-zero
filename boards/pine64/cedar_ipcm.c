/*
 * Minimal CPU H.264 encoder for the PINE64 sprite-atlas demonstrations.
 * It emits standard Baseline I_PCM macroblocks, not a private/raw container:
 * the resulting Annex B stream can be decoded by Cedar or a software decoder.
 *
 * A64 has an encoder, but the SDK's existing Bootlin-derived encoder register
 * sequence is specific to V3/V3s/S3. Until that block is separately ported and
 * tested, this path provides a real encode/decode round trip without touching
 * unverified A64 encoder registers. Conversion to 4:2:0 remains lossy; I_PCM
 * preserves those converted samples exactly and does not compress them.
 */
#include "cedar_ipcm.h"

struct bitstream {
    uint8_t *out;
    size_t length, capacity;
    unsigned byte, bits, zeros;
    int failed;
};

static void raw_byte(struct bitstream *s, unsigned value)
{
    if (s->length >= s->capacity) {
        s->failed = 1;
        return;
    }
    s->out[s->length++] = (uint8_t)value;
}

static void rbsp_byte(struct bitstream *s, unsigned value)
{
    if (s->failed) return;
    /* Escape every NAL payload, including headers and I_PCM sample data. */
    if (s->zeros >= 2 && value <= 3) {
        raw_byte(s, 3);
        s->zeros = 0;
    }
    raw_byte(s, value);
    s->zeros = value == 0 ? s->zeros + 1 : 0;
}

static void bits(struct bitstream *s, uint32_t value, unsigned count)
{
    if (s->failed) return;
    while (count) {
        --count;
        s->byte = (s->byte << 1) | ((value >> count) & 1u);
        if (++s->bits == 8) {
            rbsp_byte(s, s->byte);
            s->byte = s->bits = 0;
        }
    }
}

static void ue(struct bitstream *s, uint32_t value)
{
    unsigned leading = 0;
    uint32_t code = value + 1;
    for (uint32_t v = code; v > 1; v >>= 1) ++leading;
    bits(s, 0, leading);
    bits(s, code, leading + 1);
}

static void se(struct bitstream *s, int value)
{
    ue(s, value > 0 ? (uint32_t)(value * 2 - 1) : (uint32_t)(-value * 2));
}

static void align_zero(struct bitstream *s)
{
    if (s->bits) bits(s, 0, 8 - s->bits);
}

static void trailing_bits(struct bitstream *s)
{
    bits(s, 1, 1);
    align_zero(s);
}

static void nal_start(struct bitstream *s, unsigned header)
{
    raw_byte(s, 0); raw_byte(s, 0); raw_byte(s, 0); raw_byte(s, 1);
    s->zeros = 0;
    rbsp_byte(s, header);
}

int pine64_h264_ipcm(const uint8_t *y, const uint8_t *uv, uint32_t stride,
                     uint32_t width, uint32_t height, int qp,
                     uint8_t *output, size_t capacity)
{
    if (!y || !uv || !output || !width || !height ||
        (width & 1) || (height & 1) || width > 512 || height > 1024 ||
        stride < ((width + 15) & ~15u) || stride > 512 ||
        qp < 0 || qp > 51 || capacity < 64)
        return -1;

    const uint32_t mb_w = (width + 15) / 16;
    const uint32_t mb_h = (height + 15) / 16;
    struct bitstream s = { .out = output, .capacity = capacity };

    nal_start(&s, 0x67);              /* SPS */
    bits(&s, 66, 8);                 /* Baseline */
    bits(&s, 0xc0, 8);               /* constraint_set0/1 */
    bits(&s, 31, 8);                 /* Level 3.1 */
    ue(&s, 0);                      /* seq_parameter_set_id */
    ue(&s, 4);                      /* 8-bit frame_num */
    ue(&s, 0);                      /* pic_order_cnt_type */
    ue(&s, 4);                      /* 8-bit pic_order_cnt_lsb */
    ue(&s, 1);                      /* max_num_ref_frames */
    bits(&s, 0, 1);                 /* gaps_in_frame_num_allowed */
    ue(&s, mb_w - 1); ue(&s, mb_h - 1);
    bits(&s, 1, 1);                 /* frame_mbs_only */
    bits(&s, 0, 1);                 /* direct_8x8_inference */
    /* 4:2:0 crop units are 2 pixels in each dimension. */
    unsigned crop = (width & 15) || (height & 15);
    bits(&s, crop, 1);
    if (crop) {
        ue(&s, 0); ue(&s, (mb_w * 16 - width) / 2);
        ue(&s, 0); ue(&s, (mb_h * 16 - height) / 2);
    }
    bits(&s, 0, 1);                 /* no VUI */
    trailing_bits(&s);

    nal_start(&s, 0x68);              /* PPS */
    ue(&s, 0); ue(&s, 0);            /* PPS/SPS IDs */
    bits(&s, 0, 1);                 /* CAVLC */
    bits(&s, 0, 1);                 /* bottom_field_pic_order_in_frame */
    ue(&s, 0);                      /* no slice groups */
    ue(&s, 0); ue(&s, 0);            /* default active reference counts */
    bits(&s, 0, 1); bits(&s, 0, 2); /* no weighted prediction */
    se(&s, qp - 26); se(&s, qp - 26);
    se(&s, 0);                      /* chroma_qp_index_offset */
    bits(&s, 1, 1);                 /* deblocking_filter_control_present */
    bits(&s, 0, 1); bits(&s, 0, 1); /* constrained / redundant */
    trailing_bits(&s);

    nal_start(&s, 0x65);              /* IDR, 36 RBSP header bits incl. NAL */
    ue(&s, 0); ue(&s, 2); ue(&s, 0); /* first_mb, I slice, PPS */
    bits(&s, 0, 8); ue(&s, 0);      /* frame_num, idr_pic_id */
    bits(&s, 0, 8);                 /* pic_order_cnt_lsb */
    bits(&s, 0, 1); bits(&s, 0, 1); /* reference picture marking */
    se(&s, 0);                      /* slice_qp_delta */
    ue(&s, 1);                      /* disable deblocking */

    for (uint32_t my = 0; my < mb_h && !s.failed; ++my) {
        for (uint32_t mx = 0; mx < mb_w && !s.failed; ++mx) {
            ue(&s, 25);             /* I_PCM in an I slice */
            align_zero(&s);         /* pcm_alignment_zero_bit */
            for (unsigned row = 0; row < 16; ++row)
                for (unsigned col = 0; col < 16; ++col)
                    rbsp_byte(&s, y[(my * 16 + row) * stride + mx * 16 + col]);
            /* H.264 I_PCM uses planar Cb followed by Cr; input is NV12. */
            for (unsigned component = 0; component < 2; ++component)
                for (unsigned row = 0; row < 8; ++row)
                    for (unsigned col = 0; col < 8; ++col)
                        rbsp_byte(&s, uv[(my * 8 + row) * stride +
                                        mx * 16 + col * 2 + component]);
        }
    }
    trailing_bits(&s);
    return s.failed ? -2 : (int)s.length;
}
