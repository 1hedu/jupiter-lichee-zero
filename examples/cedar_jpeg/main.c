/*
 * CedarVE H.264 Hardware Decode Test
 *
 * Decodes an embedded 64x64 H.264 I-frame (solid red) using the V3s
 * H.264 decode engine, converts NV12 → ARGB, displays scaled on screen.
 *
 * Build: make GAME=examples/cedar_jpeg/main.c
 */
#include "jupiter.h"
#include "pmu.h"

/* 64x64 solid red, Constrained Baseline, CAVLC, 1 ref, no B-frames */
#include "test_frame.h"

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    cedar_init();

    uart_puts("\n========================================\n");
    uart_puts("  CedarVE H.264 Decode Test\n");
    uart_puts("========================================\n\n");

    /* Quick engine scan — check what's alive with H264 selected */
    REG32(0x01C0E000) = 0x00130001; /* H264 */
    uart_puts("[scan] H264 +0x228=0x"); uart_puthex(REG32(0x01C0E228));
    uart_puts(" +0x2DC=0x"); uart_puthex(REG32(0x01C0E2DC));
    uart_puts("\n");
    /* Check ISP_NEW JPEG engine at +0xE00 (V3 CEDARV_ISP_NEW) */
    uart_puts("[scan] JPEG@E00: ");
    for (uint32_t off = 0xE00; off <= 0xE20; off += 4) {
        uint32_t v = REG32(0x01C0E000 + off);
        if (v) { uart_puts("+0x"); uart_puthex(off); uart_puts("=0x"); uart_puthex(v); uart_puts(" "); }
    }
    uart_puts("\n");
    REG32(0x01C0E000) = 0x00130007; /* idle */

    /* Init display BEFORE decoding — video_init() clears the DRAM
     * region the cedar buffers live in, so decoding first would have
     * its output wiped before the NV12→ARGB conversion below. */
    video_init();
    volatile uint32_t *fb = (volatile uint32_t *)FB0_ADDR;

    /* Clear to dark gray */
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++)
        fb[i] = 0xFF222222;

    /* Decode: 64x64, header_bit_size=36 (from ffmpeg trace_headers).
     * Extra args added when the API grew: pic_init_qp, slice_qp_delta,
     * chroma_qp_off, disable_deblock — match what cedar_genesis uses. */
    uint32_t t0 = pmu_cycles();
    int rc = cedar_h264_decode(test_h264, test_h264_size, 64, 64, 36, 10, 0, 0, 1);
    uint32_t t1 = pmu_cycles();

    uart_puts("[main] decode ");
    if (rc == 0) {
        uart_puts("OK, ");
        uart_putdec(t1 - t0);
        uart_puts(" cycles\n");
    } else {
        uart_puts("FAILED rc=");
        uart_putdec((uint32_t)(-rc));
        uart_puts("\n");
    }

    if (rc == 0) {
        uart_puts("[main] converting NV12->ARGB...\n");

        /* Convert and scale 4x centered */
        static uint32_t temp[64 * 64];
        cedar_nv12_to_argb(temp, 64, 64, 64);

        uint32_t scale = 4;
        uint32_t dx = (LCD_W - 64 * scale) / 2;
        uint32_t dy = (LCD_H - 64 * scale) / 2;
        for (uint32_t sy = 0; sy < 64 * scale && (dy + sy) < LCD_H; sy++)
            for (uint32_t sx = 0; sx < 64 * scale && (dx + sx) < LCD_W; sx++)
                fb[(dy + sy) * LCD_W + (dx + sx)] = temp[(sy / scale) * 64 + (sx / scale)];
    } else {
        draw_rect(fb, LCD_W, 100, 100, 280, 72, 0xFFFF0000);
    }

    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    video_swap(FB0_ADDR);

    uart_puts("[main] done\n");
    while (1);
    return 0;
}
