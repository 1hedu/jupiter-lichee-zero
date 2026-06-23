/*
 * Jupiter SDK — Game Template
 *
 * This is your starting point. Fill in game_init() and game_frame().
 *
 * The SDK boilerplate below handles:
 *   - MMU + D-cache (all DRAM cached, flush before swap)
 *   - Double-buffered VI0 background (FB0/FB1)
 *   - Double-buffered UI0 overlay (OVL/OVL1) — tear-free sprites
 *   - NEON overlay clear each frame (you draw fresh, no need to erase)
 *   - D-cache flush + layer swap
 *   - 60fps timer pacing
 *   - UART frame time diagnostics every 2 seconds
 *
 * Build: make
 * Flash: copy build/jupiter.bin + build/boot.scr to SD card FAT partition
 * Run:   insert SD card, power on
 */
#include "jupiter.h"

/* ---- Your game state goes here ---- */

/* ---- Called once at startup (after display + MMU are live) ---- */
static void game_init(void)
{
    uart_puts("Game initialized.\n");
}

/* ---- Called every frame ----
 *
 * bg_addr:  address of VI0 back buffer — draw your world here.
 *           Use as uint32_t* (non-volatile, cached). Flushed for you.
 * ovl:      pointer to UI0 back overlay — draw sprites/HUD here.
 *           Already cleared to transparent. Just blit what you need.
 * frame:    frame counter (0, 1, 2, ...)
 *
 * When you return, both layers are swapped and displayed.
 */
static void game_frame(uint32_t bg_addr, volatile uint32_t *ovl, uint32_t frame)
{
    (void)bg_addr;
    (void)ovl;
    (void)frame;

    /* TODO: Your game logic and rendering here.
     *
     * Background (VI0) — your game world:
     *   uint32_t *fb = (uint32_t *)bg_addr;
     *   fb[50 * LCD_W + 100] = 0x00FF0000;             // red pixel
     *   draw_rect((volatile uint32_t *)fb, LCD_W,       // rectangle
     *             10, 10, 32, 32, 0x00FF0000);
     *   tiles_render_fast((volatile uint32_t *)fb, ...); // scrolling tiles
     *
     * Overlay (UI0) — sprites, HUD, effects:
     *   sprite_blit(ovl, LCD_W, my_sprite, 24, 24, x, y);  // sprite
     *   draw_rect(ovl, LCD_W, 0, 0, 100, 8, 0xFFFF0000);   // health bar
     *
     * The overlay is cleared every frame — just draw what's visible.
     * No need to erase old positions.
     */
}

/* ---- Main loop (SDK boilerplate — you shouldn't need to touch this) ---- */
void main(void)
{
    uart_puts("\n\n=== Jupiter SDK ===\n");
    timer_init();

    /* Clear all buffers */
    memset32_neon(FB0_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    /* Display + cache */
    video_init();
    mmu_init();
    uart_puts("Display + MMU active.\n");

    game_init();

    /* Double buffer state: background (VI0) + overlay (UI0) */
    uint32_t back_fb  = FB1_ADDR,  front_fb  = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();

        /* Clear overlay back buffer (NEON, ~425µs) */
        memset32_neon(back_ovl, 0x00000000, LCD_W * LCD_H * 4);

        /* Run game frame */
        volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;
        game_frame(back_fb, ovl, frame);

        /* Flush background to DRAM + swap both layers */
        dcache_clean_fb(back_fb);
        video_swap(back_fb);
        video_set_overlay(back_ovl);

        /* Flip buffers */
        uint32_t tmp;
        tmp = back_fb;  back_fb  = front_fb;  front_fb  = tmp;
        tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        /* Diagnostics */
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        if ((frame % 120) == 0) {
            uart_puts("f=");
            uart_putdec(frame);
            uart_puts(" ");
            uart_putdec(us);
            uart_puts("us\n");
        }
        frame++;

        /* Pace to 60fps */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            ;
    }
}
