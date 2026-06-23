/*
 * Jupiter SDK Example — Parallax Tile Scrolling
 * Two-speed tile background + bouncing sprite overlay.
 * Demonstrates: tiles, parallax, double buffer, overlay, frame pacing.
 */
#include "jupiter.h"

#define MAP_W      80
#define MAP_H_FAR  17
#define MAP_H_NEAR 17
#define HORIZON    (LCD_H / 2)

static uint8_t sky_map[MAP_H_FAR * MAP_W];
static uint8_t ground_map[MAP_H_NEAR * MAP_W];

static uint32_t sky_color(uint8_t id, uint32_t px, uint32_t py)
{
    (void)px; (void)py;
    switch (id) {
    case 0: return 0x00102040;
    case 1: return 0x00183058;
    case 2: return 0x00203860;
    case 3: return 0x00284068;
    default: return 0x00000000;
    }
}

static uint32_t ground_color(uint8_t id, uint32_t px, uint32_t py)
{
    (void)px; (void)py;
    switch (id) {
    case 4: return 0x00206020;
    case 5: return 0x00308030;
    case 6: return 0x0040A040;
    case 7: return 0x00604020;
    default: return 0x00000000;
    }
}

void main(void)
{
    uart_puts("\n\n=== Parallax Demo ===\n");
    timer_init();

    /* Build tile maps */
    for (uint32_t y = 0; y < MAP_H_FAR; y++)
        for (uint32_t x = 0; x < MAP_W; x++) {
            uint8_t base = (y < 5) ? 0 : (y < 10) ? 1 : (y < 14) ? 2 : 3;
            if (((x * 7 + y * 13) % 23) == 0) base = 1;
            sky_map[y * MAP_W + x] = base;
        }
    for (uint32_t y = 0; y < MAP_H_NEAR; y++)
        for (uint32_t x = 0; x < MAP_W; x++) {
            uint8_t base = 5;
            if (y == 0) base = 6;
            if (y > 12) base = 4;
            if (((x + y * 3) % 11) == 0) base = 7;
            ground_map[y * MAP_W + x] = base;
        }

    memset32_neon(FB0_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0, LCD_W * LCD_H * 4);

    video_init();
    uart_puts("Display live.\n");

    int sx = 200, sy = 60, dx = 3, dy = 2, sw = 24, sh = 24;
    int scroll_far = 0, scroll_near = 0;
    uint32_t back_fb  = FB1_ADDR,  front_fb  = FB0_ADDR;
    /* UI0 overlay double-buffered same as VI0 — paint back, dcache
     * flush, video_set_overlay, swap. Without the back/front pair the
     * sprite painted into the live OVL_ADDR while DE2 was reading it,
     * which showed as a trail / garbled overlay (especially noticeable
     * when launched from the menu — its UI text was on OVL_ADDR at
     * entry). */
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();
        volatile uint32_t *fb  = (volatile uint32_t *)back_fb;
        volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;

        tiles_render(fb, LCD_W, sky_map, MAP_W, 0, MAP_H_FAR,
                     scroll_far, sky_color);
        tiles_render(fb, LCD_W, ground_map, MAP_W, HORIZON, MAP_H_NEAR,
                     scroll_near, ground_color);
        scroll_far += 1;
        scroll_near += 3;

        sx += dx; sy += dy;
        if (sx <= 0 || sx + sw >= (int)LCD_W) { dx = -dx; sx += dx; }
        if (sy <= 0 || sy + sh >= (int)LCD_H) { dy = -dy; sy += dy; }
        /* Full-clear back overlay each frame then draw sprite. */
        memset32_neon(back_ovl, 0, LCD_W * LCD_H * 4);
        draw_rect(ovl, LCD_W, sx, sy, sw, sh, 0xFFFFFF00);

        /* Flush + flip BOTH layers */
        dcache_clean_fb(back_fb);
        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);
        video_swap(back_fb);
        video_set_overlay(back_ovl);
        uint32_t tmp;
        tmp = back_fb;  back_fb  = front_fb;  front_fb  = tmp;
        tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        if ((frame % 120) == 0) {
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" "); uart_putdec(ticks_to_us(timer_elapsed(t0, timer_read())));
            uart_puts("us\n");
        }
        frame++;
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 33333) ;
    }
}
