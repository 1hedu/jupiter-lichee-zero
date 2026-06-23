/*
 * Jupiter SDK — cedar.c
 * CedarVE H.264 hardware decode for V3s.
 *
 * Critical init: SRAM_CTRL_REG0 (0x01C00000) = 0x7FFFFFFF
 * Without this, VE cannot DMA. From Allwinner BSP cedar_ve.c.
 */
#include "jupiter.h"
#include <string.h>

/* VE Register Access */
#define VE_BASE  0x01C0E000
#define VE(off)  REG32(VE_BASE + (off))
#define VEB(off) (*(volatile uint8_t *)(VE_BASE + (off)))

/* Buffer layout in upper DRAM. Originally at 0x43000000-0x432FFFFF —
 * but with 38 MB of bss in the war1 build that range collides with
 * live in-use bss, and cedar's per-frame writes corrupted the tile
 * map / unit state during the briefing cinematic. Moved into the
 * free VRAM gap (0x43E00000-0x43F00000, 1MB) which sits between
 * AUDIO_BUF (0x43D00000) and NAL_STAGE (0x43F00000). */
#define BUF_INPUT     0x43E00000  /* 256KB for H264 bitstream */
#define BUF_PIC_INFO  0x43E40000  /* 256KB */
#define BUF_NEIGHBOR  0x43E80000  /* 32KB */
#define BUF_MV_COL    0x43E88000  /* 64KB */
#define BUF_LUMA      0x43E98000  /* up to 512*480 = 240KB */
#define BUF_CHROMA    0x43ED8000  /* up to 120KB */

/* Encoder buffers — reuse decoder buffer addresses (known VE-accessible) */
#define BUF_ENC_LUMA   BUF_LUMA     /* 0x43100000 — decoder writes here OK */
#define BUF_ENC_CHROMA BUF_CHROMA   /* 0x43200000 */
#define BUF_ENC_OUT    BUF_INPUT    /* 0x43000000 — decoder reads here OK */
#define BUF_ENC_OUT_SZ 0x00100000

/* ISP registers (input for encoder) */
#define ISP_INPUT_SIZE    0xA00
#define ISP_INPUT_STRIDE  0xA04
#define ISP_CTRL          0xA08
#define ISP_INPUT_LUMA    0xA78
#define ISP_INPUT_CHROMA  0xA7C

/* VLD address encoding */
#define VLD_ADDR_VAL(x)  (((x)&0x0FFFFFF0)|((x)>>28))
#define VLD_FLAGS        ((1u<<30)|(1u<<29)|(1u<<28))

/* ================================================================== */
void cedar_init(void)
{
    /* SRAM mapping — THE critical step from Allwinner BSP */
    uint32_t val = REG32(0x01C00000);
    val &= 0x80000000;
    REG32(0x01C00000) = val;
    val = REG32(0x01C00000);
    val |= 0x7FFFFFFF;
    REG32(0x01C00000) = val;
    SRAM_CTRL_REG1 &= ~(1u << 24);
    __asm__ volatile("dsb");

    /* PLL_VE at CCU+0x018: 402 MHz (N=67, M=3) */
    {
        uint32_t pll = REG32(CCU_BASE + 0x0018);
        pll &= ~(0xFFu << 8);
        pll &= ~(0xFu << 0);
        pll |= (67u << 8) | (3u << 0) | (1u << 31);
        REG32(CCU_BASE + 0x0018) = pll;
        while (!(REG32(CCU_BASE + 0x0018) & (1u << 28)));
    }

    /* All clocks ON, then reset (aodzip/BSP order) */
    REG32(CCU_BASE + 0x0060) |= (1u << 0);
    REG32(CCU_BASE + 0x013C) = (1u << 31);
    REG32(CCU_BASE + 0x0100) |= (1u << 0);
    __asm__ volatile("dsb");

    REG32(CCU_BASE + 0x02C0) &= ~(1u << 0);
    for (volatile int i = 0; i < 10000; i++);
    REG32(CCU_BASE + 0x02C0) |= (1u << 0);
    for (volatile int i = 0; i < 10000; i++);

    VE(0x000) = 0x00130007;  /* park idle */

    uart_puts("[cedar] VE=0x");
    uart_puthex(VE(0x0F0));
    uart_puts(" PLL=0x");
    uart_puthex(REG32(CCU_BASE + 0x0018));
    uart_puts("\n");
}

/* ================================================================== */
/* Cache invalidation for VE output buffers                            */
/* ================================================================== */
void dcache_invalidate_range(uint32_t addr, uint32_t size)
{
    uint32_t ctr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));
    uint32_t line = 4U << ((ctr >> 16) & 0xF);
    uint32_t end = addr + size;
    addr &= ~(line - 1);
    for (; addr < end; addr += line)
        __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" :: "r"(addr));
    __asm__ volatile("dsb");
}

/* ================================================================== */
/* SRAM + VLD helpers                                                   */
/* ================================================================== */
static void sram_write(uint32_t off, const void *data, uint32_t len)
{
    const uint32_t *p = (const uint32_t *)data;
    VE(0x2E0) = off << 2;
    for (uint32_t i = 0; i < (len + 3) / 4; i++)
        VE(0x2E4) = p[i];
}

static int wait_vld(void)
{
    for (uint32_t t = 0; t < 1000000; t++)
        if (!(VE(0x228) & (1 << 8))) return 0;
    return -1;
}

static int skip_bits(int n)
{
    while (n > 0) {
        int b = n > 32 ? 32 : n;
        VE(0x224) = 3 | (b << 8);
        if (wait_vld() < 0) return -1;
        n -= b;
    }
    return 0;
}

/* Find NAL unit by type */
static const uint8_t *find_nal(const uint8_t *d, uint32_t sz,
                                uint8_t type, uint32_t *len)
{
    for (uint32_t i = 0; i + 3 < sz; i++) {
        int sc = (d[i]==0 && d[i+1]==0 && d[i+2]==1) ? 3 :
                 (i+3<sz && d[i]==0 && d[i+1]==0 && d[i+2]==0 && d[i+3]==1) ? 4 : 0;
        if (!sc) continue;
        uint32_t s = i + sc;
        if (s < sz && (d[s] & 0x1F) == type) {
            uint32_t e = sz;
            for (uint32_t j = s+1; j+2 < sz; j++)
                if (d[j]==0 && d[j+1]==0 && (d[j+2]==1 || (j+3<sz && d[j+2]==0 && d[j+3]==1)))
                    { e = j; break; }
            *len = e - s;
            return &d[s];
        }
        i += sc - 1;
    }
    return NULL;
}

/* ================================================================== */
/* H.264 I-frame decode                                                 */
/* ================================================================== */
int cedar_h264_decode(const uint8_t *h264, uint32_t h264_sz,
                      uint32_t w, uint32_t h, uint32_t hdr_bits,
                      int pic_init_qp, int slice_qp_delta,
                      int chroma_qp_off, int disable_deblock)
{
    uint32_t mb_w = (w + 15) / 16;
    uint32_t mb_h = (h + 15) / 16;
    uint32_t stride = mb_w * 16;

    uint32_t nal_len = 0;
    const uint8_t *nal = find_nal(h264, h264_sz, 5, &nal_len);
    if (!nal) { uart_puts("[cedar] no IDR\n"); return -1; }

    /* Copy NAL to aligned buffer */
    uint32_t padded = (nal_len + 4095) & ~4095;
    memset((void *)BUF_INPUT, 0, padded);
    memcpy((void *)BUF_INPUT, nal, nal_len);
    dcache_clean_range(BUF_INPUT, padded);

    /* Clear working buffers */
    memset((void *)BUF_PIC_INFO, 0, 0x40000);
    memset((void *)BUF_NEIGHBOR, 0, 0x8000);
    memset((void *)BUF_MV_COL, 0, 0x10000);
    dcache_clean_range(BUF_PIC_INFO, 0x40000);
    dcache_clean_range(BUF_NEIGHBOR, 0x8000);
    dcache_clean_range(BUF_MV_COL, 0x10000);

    /* Clear output */
    uint32_t luma_sz = stride * mb_h * 16;
    uint32_t chroma_sz = luma_sz / 2;
    memset((void *)BUF_LUMA, 0, luma_sz);
    memset((void *)BUF_CHROMA, 0x80, chroma_sz);
    dcache_clean_range(BUF_LUMA, luma_sz);
    dcache_clean_range(BUF_CHROMA, chroma_sz);

    /* Select H264 engine */
    VE(0x000) = 0x00130001;

    /* NV12 output */
    VE(0x0EC) = (0x04 << 4);  /* NV12 output */
    VE(0x0C4) = chroma_sz / 2;
    VE(0x0C8) = ((stride / 2) << 16) | stride;

    /* Setup */
    VE(0x240) = 0;
    VE(0x250) = BUF_PIC_INFO;
    VE(0x254) = BUF_NEIGHBOR;
    VE(0x050) = 0;

    /* Frame list */
    uint32_t frame[8 * 18];
    memset(frame, 0, sizeof(frame));
    frame[3] = BUF_LUMA;
    frame[4] = BUF_CHROMA;
    frame[5] = BUF_MV_COL;
    frame[6] = BUF_MV_COL;
    sram_write(0x100, frame, sizeof(frame));
    VE(0x24C) = 0;

    /* VLD */
    VE(0x238) = nal_len * 8;
    VE(0x234) = 0;
    VE(0x23C) = BUF_INPUT + padded - 1;
    VE(0x230) = VLD_ADDR_VAL(BUF_INPUT) | VLD_FLAGS;

    VE(0x224) = 7;  /* INIT_SWDEC */
    wait_vld();
    if (skip_bits(hdr_bits) < 0) {
        uart_puts("[cedar] VLD flush fail\n");
        VE(0x000) = 7; return -2;
    }

    /* SPS: chroma=1, frame_mbs_only=1
     * direct_8x8_inference: 0 for on-device encode, 1 for ffmpeg encode */
    VE(0x200) = (1u << 19) | ((mb_w-1) << 8) | (mb_h-1) | (1u<<18);
    /* PPS */
    VE(0x204) = 0;
    /* Slice header */
    VE(0x208) = (1u << 12) | (2u << 8) | (1u << 5);
    /* Slice header 2 */
    {
        uint32_t shs2 = (1u << 12);
        shs2 |= (disable_deblock & 0x3) << 8;
        VE(0x20C) = shs2;
    }
    /* QP */
    {
        int qp = pic_init_qp + slice_qp_delta;
        VE(0x21C) = (1u << 24) |
                    ((chroma_qp_off & 0x3F) << 16) |
                    ((chroma_qp_off & 0x3F) << 8) |
                    (qp & 0x3F);
    }

    /* Clear status + enable IRQs */
    VE(0x228) = VE(0x228);
    VE(0x220) = 7;

    /* Trigger */
    VE(0x224) = 8;  /* AVC_SLICE_DECODE */

    /* Poll */
    uint32_t status, timeout = 50000000;
    do { status = VE(0x228); }
    while (!(status & 7) && --timeout);

    VE(0x228) = 7;
    VE(0x000) = 0x00130007;

    /* Invalidate CPU cache for output buffers */
    dcache_invalidate_range(BUF_LUMA, luma_sz);
    dcache_invalidate_range(BUF_CHROMA, chroma_sz);

    if (status & 1) {
        /* Print success only once per ~64 frames so video playback at
         * 15-30 fps doesn't drown the UART. */
        static uint32_t ok_count = 0;
        if ((ok_count++ & 0x3F) == 0) {
            uart_puts("[cedar] OK MB=");
            uart_putdec(mb_w * mb_h);
            uart_puts(" (frame ");
            uart_putdec(ok_count);
            uart_puts(")\n");
        }
        return 0;
    }
    uart_puts("[cedar] FAIL status=0x");
    uart_puthex(status);
    uart_puts("\n");
    return -3;
}

/* Encoder moved to cedar_enc.c */
#if 0 /* OLD ENCODER CODE */
static void old_argb_to_nv12(const uint32_t *src, uint32_t src_pitch,
                        uint32_t w, uint32_t h)
{
    uint32_t stride = (w + 15) & ~15;
    uint8_t *Y  = (uint8_t *)BUF_ENC_LUMA;
    uint8_t *UV = (uint8_t *)BUF_ENC_CHROMA;

    for (uint32_t r = 0; r < h; r++) {
        for (uint32_t c = 0; c < w; c++) {
            uint32_t px = src[r * src_pitch + c];
            int R = (px >> 16) & 0xFF;
            int G = (px >> 8)  & 0xFF;
            int B =  px        & 0xFF;
            /* BT.601 full-range */
            int y = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
            if (y < 0) y = 0; if (y > 255) y = 255;
            Y[r * stride + c] = (uint8_t)y;

            if ((r & 1) == 0 && (c & 1) == 0) {
                int u = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
                int v = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
                if (u < 0) u = 0; if (u > 255) u = 255;
                if (v < 0) v = 0; if (v > 255) v = 255;
                UV[(r/2) * stride + (c & ~1)]     = (uint8_t)u;
                UV[(r/2) * stride + (c & ~1) + 1] = (uint8_t)v;
            }
        }
        /* Pad stride */
        for (uint32_t c = w; c < stride; c++)
            Y[r * stride + c] = 16;
    }

    uint32_t luma_sz = stride * ((h + 15) & ~15);
    uint32_t chroma_sz = luma_sz / 2;
    dcache_clean_range(BUF_ENC_LUMA, luma_sz);
    dcache_clean_range(BUF_ENC_CHROMA, chroma_sz);
}

/* ================================================================== */
/* H.264 I-frame ENCODE via AVC engine (+0xB00)                         */
/*                                                                      */
/* From libcedarjpeg veavc.c + veisp.c + Bootlin encode work.           */
/* Input: NV12 at BUF_ENC_LUMA/CHROMA (from cedar_argb_to_nv12)         */
/* Output: H.264 bitstream at BUF_ENC_OUT                               */
/* Returns: encoded size in bytes, or negative on error                  */
/* ================================================================== */
int cedar_h264_encode(uint32_t w, uint32_t h, int qp,
                      const uint8_t *nal_header, uint32_t nal_header_len)
{
    uint32_t mb_w = (w + 15) / 16;
    uint32_t mb_h = (h + 15) / 16;
    uint32_t stride = mb_w * 16;

    /* Fill output with canary pattern (0xAA) to detect if encoder writes here */
    memset((void *)BUF_ENC_OUT, 0xAA, BUF_ENC_OUT_SZ);
    dcache_clean_range(BUF_ENC_OUT, BUF_ENC_OUT_SZ);

    /* Bootlin encoder init sequence (cedrus_enc.c lines 62-88):
     * 1. Disable encoder + ISP, set decoder disabled
     * 2. Reset encoder via VE_RESET_REG (offset 0x04)
     * 3. Enable encoder + ISP */
    {
        uint32_t mode = VE(0x000);
        mode &= ~((1u << 7) | (1u << 6));  /* clear enc + ISP enable */
        mode |= 0x07;                       /* decoder disabled */
        VE(0x000) = mode;

        /* Encoder reset pulse via VE_RESET_REG (0x04) bit 24 */
        VE(0x004) = VE(0x004) | (1u << 24);
        for (volatile int i = 0; i < 1000; i++);
        VE(0x004) = VE(0x004) & ~(1u << 24);
        for (volatile int i = 0; i < 1000; i++);

        /* Enable encoder (bit7) + ISP (bit6), keep decoder disabled */
        mode = VE(0x000);
        mode |= (1u << 7) | (1u << 6) | 0x07;
        VE(0x000) = mode;
    }

    /* ISP: set input NV12 buffers
     * Try DRAM-relative — the encoder ISP might not use full physical
     * like the decoder does */
    {
        uint32_t pic_size = ((mb_w & 0x3FF) << 16) | (mb_h & 0x3FF);
        uint32_t pic_stride = ((stride / 16) << 16);
        VE(ISP_INPUT_SIZE) = pic_size;
        VE(ISP_INPUT_STRIDE) = pic_stride;
        /* ISP scaler size (from Bootlin: 0xA00 + 0x2C = 0xA2C) */
        VE(0xA2C) = ((mb_h & 0x7FF) << 16) | (mb_w & 0x7FF);
        /* ISP ctrl: YUV420SP format + BT601 colorspace (from Bootlin) */
        VE(ISP_CTRL) = (0u << 27) | (0u << 20) | (0u << 1);
        VE(ISP_INPUT_LUMA) = BUF_ENC_LUMA - 0x40000000;
        VE(ISP_INPUT_CHROMA) = BUF_ENC_CHROMA - 0x40000000;
    }

    /* VLE: output bitstream buffer (DRAM-relative) */
    VE(0xB80) = BUF_ENC_OUT - 0x40000000;
    VE(0xB84) = BUF_ENC_OUT + BUF_ENC_OUT_SZ - 1 - 0x40000000;
    VE(0xB88) = 0;                                   /* VLE_OFFSET */
    uint32_t maxbits = BUF_ENC_OUT_SZ * 8;
    if (maxbits > 0x0FFF0000) maxbits = 0x0FFF0000;
    VE(0xB8C) = maxbits;                             /* VLE_MAX */

    /* AVC encoder init */
    VE(0xB14) = 0x0000000F;   /* AVC_CTRL: enable all IRQs */
    VE(0xB18) = (0u << 16);   /* AVC_TRIGGER: H264 mode init */

    /* Clear status */
    uint32_t status = VE(0xB1C);
    VE(0xB1C) = status | 0xF;

    VE(0xBA0) = BUF_ENC_LUMA - 0x40000000;
    VE(0xBA4) = BUF_ENC_CHROMA - 0x40000000;
    VE(0xBB0) = BUF_ENC_LUMA - 0x40000000;
    VE(0xBB4) = BUF_ENC_CHROMA - 0x40000000;

    /* AVC_PARAM: QP and encoding parameters */
    VE(0xB04) = (1u << 31) |    /* fill1 */
                (1u << 30) |    /* stuff byte enable */
                (0u << 16) |    /* chroma bias */
                (0u << 0);      /* luma bias */

    /* AVC_QP */
    VE(0xB08) = (qp & 0x3F) | ((qp & 0x3F) << 8);  /* QP luma + chroma */

    /* AVC_MOTION_EST: disable for I-frame */
    VE(0xB10) = 0;

    /* Write NAL headers via put_bits AFTER mode init (before launch).
     * The mode init trigger above reset VLE_OFFSET to 0.
     * put_bits writes sequentially, encoder appends after. */
    if (nal_header && nal_header_len > 0) {
        for (uint32_t i = 0; i < nal_header_len; i++) {
            VE(0xB20) = ((uint32_t)nal_header[i]) << 24;  /* MSB-first */
            VE(0xB18) = (0u << 16) | (8 << 8) | 1;        /* 8 bits, H264 mode */
        }
        uart_puts("[cedar] wrote "); uart_putdec(nal_header_len); uart_puts("B hdr via put_bits\n");
    }

    uart_puts("[cedar] encoding "); uart_putdec(w); uart_puts("x");
    uart_putdec(h); uart_puts(" QP="); uart_putdec(qp); uart_puts("...\n");

    /* Launch encoding — encoder appends macroblocks after headers */
    VE(0xB18) = (0u << 16) | 8;  /* H264 mode, encode trigger */

    /* Wait for completion */
    uint32_t timeout = 50000000;
    do { status = VE(0xB1C); }
    while (!(status & 0xF) && --timeout);

    uint32_t encoded_bits = VE(0xB90);  /* VLE_LENGTH */
    uint32_t encoded_bytes = encoded_bits / 8;

    /* Read registers BEFORE going idle (idle blanks everything) */
    uint32_t vle_addr_post = VE(0xB80);
    uint32_t vle_len_post = VE(0xB90);
    uint32_t isp_luma_post = VE(0xA78);
    uint32_t isp_chroma_post = VE(0xA7C);

    VE(0xB1C) = 0xF;
    VE(0x000) = 0x00130007;

    dcache_invalidate_range(BUF_ENC_OUT, encoded_bytes + 64);

    uart_puts("[cedar] encode status=0x"); uart_puthex(status);
    if (status & 1) {
        uart_puts(" OK ");
        uart_putdec(encoded_bytes); uart_puts("B");

        /* Register dump (captured before idle) */
        uart_puts("\n[cedar] VLE: ADDR=0x"); uart_puthex(vle_addr_post);
        uart_puts(" LEN=0x"); uart_puthex(vle_len_post);
        uart_puts("\n[cedar] ISP: LUMA=0x"); uart_puthex(isp_luma_post);
        uart_puts(" CHROMA=0x"); uart_puthex(isp_chroma_post);
        uart_puts("\n");

        /* Invalidate large range and check multiple locations */
        dcache_invalidate_range(BUF_ENC_OUT, 0x10000);

        /* Find first non-canary (non-0xAA) byte in the buffer */
        int first_diff = -1;
        volatile uint8_t *obuf = (volatile uint8_t *)BUF_ENC_OUT;
        for (int i = 0; i < 0x10000; i++) {
            if (obuf[i] != 0xAA) { first_diff = i; break; }
        }
        uart_puts("[cedar] first non-0xAA at offset ");
        if (first_diff >= 0) {
            uart_putdec(first_diff);
            uart_puts(": ");
            for (int i = first_diff; i < first_diff + 16 && i < 0x10000; i++) {
                uart_puthex(obuf[i]); uart_puts(" ");
            }
        } else {
            uart_puts("NONE (buffer untouched by encoder!)");
        }
        uart_puts("\n");

        /* Also check if VLE_ADDR readback suggests different address mapping */
        uint32_t vle_addr_rb = VE(0xB80);
        if (vle_addr_rb != BUF_ENC_OUT) {
            uart_puts("[cedar] VLE_ADDR mismatch! wrote 0x");
            uart_puthex(BUF_ENC_OUT);
            uart_puts(" read 0x"); uart_puthex(vle_addr_rb);
            /* Try reading from the readback address */
            dcache_invalidate_range(vle_addr_rb, 64);
            uart_puts("\n[cedar] @readback_addr: ");
            volatile uint8_t *alt = (volatile uint8_t *)(uintptr_t)vle_addr_rb;
            for (int i = 0; i < 8; i++) { uart_puthex(alt[i]); uart_puts(" "); }
            /* Also try readback + 0x40000000 */
            uint32_t alt2 = vle_addr_rb + 0x40000000;
            if (alt2 >= 0x40000000 && alt2 < 0x44000000) {
                dcache_invalidate_range(alt2, 64);
                uart_puts("\n[cedar] @rb+0x40M: ");
                volatile uint8_t *a2 = (volatile uint8_t *)(uintptr_t)alt2;
                for (int i = 0; i < 8; i++) { uart_puthex(a2[i]); uart_puts(" "); }
            }
        }

        /* Check SRAM_C for encoded data (VLE might write there) */
        uart_puts("\n[cedar] SRAM@0x01D00000: ");
        for (int i = 0; i < 8; i++) {
            uart_puthex(((volatile uint8_t *)0x01D00000)[i]); uart_puts(" ");
        }
        uart_puts("\n[cedar] SRAM@0x01D00100: ");
        for (int i = 0; i < 8; i++) {
            uart_puthex(((volatile uint8_t *)0x01D00100)[i]); uart_puts(" ");
        }
        uart_puts("\n");
        uart_puts("\n");
        return (int)encoded_bytes;
    }
    if (timeout == 0) uart_puts(" TIMEOUT");
    uart_puts(" FAIL\n");
#endif /* OLD ENCODER CODE */

/* ================================================================== */
/* NV12 → ARGB into a destination buffer                                */
/* ================================================================== */
void cedar_nv12_to_argb(uint32_t *dst, uint32_t dst_pitch,
                        uint32_t w, uint32_t h)
{
    uint32_t stride = (w + 15) & ~15;
    const uint8_t *Y = (const uint8_t *)BUF_LUMA;
    const uint8_t *UV = (const uint8_t *)BUF_CHROMA;

    for (uint32_t r = 0; r < h; r++) {
        for (uint32_t c = 0; c < w; c++) {
            int y = Y[r * stride + c];
            int u = UV[(r/2) * stride + (c & ~1)];
            int v = UV[(r/2) * stride + (c | 1)];
            int R = y + ((359 * (v-128)) >> 8);
            int G = y - ((88 * (u-128) + 183 * (v-128)) >> 8);
            int B = y + ((454 * (u-128)) >> 8);
            if (R<0) R=0; if (R>255) R=255;
            if (G<0) G=0; if (G>255) G=255;
            if (B<0) B=0; if (B>255) B=255;
            dst[r * dst_pitch + c] = 0xFF000000|(R<<16)|(G<<8)|B;
        }
    }
}
