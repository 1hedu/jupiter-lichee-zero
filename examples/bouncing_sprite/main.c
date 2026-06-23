/*
 * Jupiter SDK Example — Bouncing Sprite
 * Yellow box bouncing off edges on UI overlay, blue background on VI0.
 * Demonstrates: overlay layer, per-pixel alpha, frame pacing.
 */
#include "jupiter.h"

void main(void)
{
    uart_puts("\n\n=== Bouncing Sprite ===\n");
    timer_init();

    memset32_neon(FB0_ADDR,  0x00003080, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    uart_puts("Display live.\n");

    /* Double-buffer the UI0 overlay (paint back, flush, flip, swap).
     * Without this, painting into the live OVL_ADDR while DE2 reads
     * from it produces visible tearing + leftover-pixel trails on the
     * sprite when this demo runs after the menu (whose UI text was
     * sitting on OVL_ADDR). Same pattern jupiter_logo uses. */
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    int sx = 100, sy = 60, dx = 3, dy = 2, sw = 24, sh = 24;
    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();

        sx += dx; sy += dy;
        if (sx <= 0 || sx + sw >= (int)LCD_W) { dx = -dx; sx += dx; }
        if (sy <= 0 || sy + sh >= (int)LCD_H) { dy = -dy; sy += dy; }

        /* Clear the whole back overlay then draw the sprite — cheaper to
         * memset 512 KB with NEON than to track dirty regions across
         * two buffers, and removes all trail risk. */
        memset32_neon(back_ovl, 0x00000000, LCD_W * LCD_H * 4);
        draw_rect((volatile uint32_t *)back_ovl, LCD_W, sx, sy, sw, sh, 0xFFFFFF00);
        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);
        video_set_overlay(back_ovl);

        /* Swap front/back. */
        uint32_t tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        if ((frame++ % 120) == 0) {
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" "); uart_putdec(ticks_to_us(timer_elapsed(t0, timer_read())));
            uart_puts("us\n");
        }
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;
    }
}
