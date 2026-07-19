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

/* Buffer layout: the VE's DMA buffers live in the unused top halves of
 * the five 1 MB VRAM display slots (each display buffer occupies only
 * 510 KB of its slot — see FB0_ADDR..OVL1_ADDR in v3s.h). This keeps
 * them out of the CODE region (a war1-sized bss collides with anything
 * below 0x43800000 — cedar's per-frame writes used to corrupt the tile
 * map / unit state during the briefing cinematic) and clear of the
 * 0x43D00000 audio buffer + example scratch and the 0x43F00000 NAL
 * staging area.
 *
 * The encoder (cedar_enc.c) shares some of these slots: its NV12 input
 * occupies the same slots as the decoder's NV12 output, and its recon
 * buffer overlays BUF_INPUT/BUF_PIC_INFO. That is safe because encode
 * and decode never run concurrently — a decode may follow an encode
 * (the cedar_snes round-trip) since each phase rewrites its own
 * buffers on entry. Keep the two files' layouts in sync!
 *
 *   0x43880000  BUF_LUMA      512 KB  (= enc NV12 Y slot)
 *   0x43980000  BUF_CHROMA    256 KB  (= enc NV12 UV slot)
 *   0x439C0000  (enc SUBPIX   256 KB)
 *   0x43A80000  BUF_INPUT     256 KB  (= enc REC luma slot)
 *   0x43AC0000  BUF_PIC_INFO  256 KB  (      "        )
 *   0x43B80000  BUF_NEIGHBOR   32 KB
 *   0x43B88000  BUF_MV_COL     64 KB
 *   0x43B98000  (enc MB_INFO   16 KB)
 *   0x43BA0000  (enc REC chroma 384 KB)
 *   0x43F00000  (enc STREAM     1 MB — shared with example NAL staging)
 */
#define BUF_INPUT     0x43A80000  /* 256KB for H264 bitstream */
#define BUF_PIC_INFO  0x43AC0000  /* 256KB */
#define BUF_NEIGHBOR  0x43B80000  /* 32KB */
#define BUF_MV_COL    0x43B88000  /* 64KB */
#define BUF_LUMA      0x43880000  /* NV12 luma out, up to 512KB */
#define BUF_CHROMA    0x43980000  /* NV12 chroma out, up to 256KB */

#define BUF_INPUT_SIZE   0x40000
#define BUF_LUMA_SIZE    0x80000
#define BUF_CHROMA_SIZE  0x40000

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
        /* Bounded lock wait — a hung PLL must not hang the system */
        uint32_t lock_timeout = 10000000;
        while (!(REG32(CCU_BASE + 0x0018) & (1u << 28)) && --lock_timeout);
        if (!lock_timeout)
            uart_puts("[cedar] WARN: PLL_VE lock timeout\n");
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

    uart_puts("[cedar] VE=");
    uart_puthex(VE(0x0F0));
    uart_puts(" PLL=");
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

    /* Reject frames that would overflow the fixed output slots — the
     * VE would silently DMA past them into neighboring buffers. */
    if (stride * mb_h * 16 > BUF_LUMA_SIZE ||
        stride * mb_h * 8 > BUF_CHROMA_SIZE) {
        uart_puts("[cedar] frame too large for output buffers\n");
        return -1;
    }

    uint32_t nal_len = 0;
    const uint8_t *nal = find_nal(h264, h264_sz, 5, &nal_len);
    if (!nal) { uart_puts("[cedar] no IDR\n"); return -1; }

    /* Copy NAL to aligned buffer */
    uint32_t padded = (nal_len + 4095) & ~4095;
    if (padded > BUF_INPUT_SIZE) {
        uart_puts("[cedar] NAL too large for input buffer\n");
        return -1;
    }
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
    if (wait_vld() < 0 || skip_bits(hdr_bits) < 0) {
        uart_puts("[cedar] VLD flush fail\n");
        VE(0x000) = 0x00130007; return -2;
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
    uart_puts("[cedar] FAIL status=");
    uart_puthex(status);
    uart_puts("\n");
    return -3;
}

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
