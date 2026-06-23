/*
 * Jupiter SDK Example — Color Bars
 * The simplest possible test: fill the screen with 8 color bars.
 */
#include "jupiter.h"

void main(void)
{
    uart_puts("\n\n=== Color Bars ===\n");
    timer_init();
    mmu_init();   /* enable D-cache so dcache_clean_fb has something to flush */

    volatile uint32_t *fb = (volatile uint32_t *)FB0_ADDR;
    uint32_t bar_w = LCD_W / 8;
    uint32_t colors[8] = {
        0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFF00,
        0x00FF00FF, 0x0000FFFF, 0x00FFFFFF, 0x00000000
    };
    for (uint32_t y = 0; y < LCD_H; y++)
        for (uint32_t x = 0; x < LCD_W; x++) {
            uint32_t bar = x / bar_w;
            fb[y * LCD_W + x] = colors[bar > 7 ? 7 : bar];
        }

    /* Pixel writes went through the D-cache; DE2 reads from DRAM. Without
     * this flush, DE2 sees stale / uninitialized memory and the screen
     * shows a "blip" of partial colorbars as cache lines naturally evict,
     * then freezes incomplete. Flush before handing the buffer to DE2. */
    dcache_clean_fb(FB0_ADDR);

    video_init();
    uart_puts("Color bars displayed. Halted.\n");
    while (1);
}
