/*
 * Jupiter Mode 4 "CINEMA" — NV12 direct scanout bring-up test
 *
 * Decodes the known-good 64x64 H.264 probe frame (same data as
 * cedar_jpeg) and shows it TWICE:
 *   left  — software path: cedar_nv12_to_argb into the framebuffer
 *   right — hardware path: VI0 scans the decoder's NV12 planes
 *           directly through the DE2 color-space converter
 *
 * If Mode 4 works, both squares look identical. If the CSC is
 * misprogrammed the right square shows wrong colors; if the format
 * bits are wrong it shows garbage. The QEMU display model is
 * ARGB-only, so on the emulator the right square is EXPECTED garbage
 * — this test means silicon.
 *
 * Wait — one VI0 can only scan one source, so "both at once" needs
 * the split: the ARGB reference goes on the UI0 overlay (left), the
 * NV12 frame is the VI0 window (right). UI0 composites over VI0, so
 * the two squares sit side by side on the backdrop fill color.
 *
 * Build: make GAME=examples/mode4_cinema/main.c
 */
#include "jupiter.h"
#include "pmu.h"

#include "../cedar_jpeg/test_frame.h"

#define FRAME_W 64
#define FRAME_H 64
#define STRIDE  64          /* mb-aligned luma width */

void main(void)
{
    uart_puts("\n=== Mode 4 CINEMA — NV12 scanout bring-up ===\n");
    timer_init();
    mmu_init();
    pmu_init();

    video_init();           /* clears the codec buffer region — decode after */
    cedar_init();

    int rc = cedar_h264_decode(test_h264, test_h264_size,
                               FRAME_W, FRAME_H, 36, 10, 0, 0, 1);
    uart_puts("[m4] decode rc=");
    if (rc < 0) { uart_putc('-'); uart_putdec((uint32_t)-rc); }
    else        { uart_putdec((uint32_t)rc); }
    uart_puts("\n");
    if (rc != 0) { uart_puts("[m4] decode failed — halting\n"); while (1) ; }

    /* Note: once Mode 4 takes VI0, FB0 is no longer scanned — the
     * screen backdrop is the blender's background color (black). */

    /* LEFT: software reference on the UI0 overlay (opaque square) */
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    {
        uint32_t *ovl = (uint32_t *)OVL_ADDR;
        uint32_t x0 = LCD_W / 4 - FRAME_W / 2;
        uint32_t y0 = (LCD_H - FRAME_H) / 2;
        cedar_nv12_to_argb(ovl + y0 * LCD_W + x0, LCD_W, FRAME_W, FRAME_H);
        /* cedar_nv12_to_argb writes 0xFF alpha — already opaque */
    }
    dcache_clean_range(OVL_ADDR, LCD_W * LCD_H * 4);

    /* RIGHT: hardware path — VI0 becomes a 64x64 NV12 window fed by
     * the decoder's planes, positioned at 3/4 width. The DE2 CSC does
     * the YUV->RGB during scanout; zero CPU pixel work. */
    video_mode4_nv12(cedar_dec_luma_addr(), cedar_dec_chroma_addr(),
                     FRAME_W, FRAME_H, STRIDE,
                     3 * LCD_W / 4 - FRAME_W / 2, (LCD_H - FRAME_H) / 2);

    uart_puts("[m4] scanout live: SW square left, HW NV12 square right\n");
    uart_puts("[m4] identical squares = Mode 4 verified on this board\n");

    while (1)
        video_wait_vblank();
}
