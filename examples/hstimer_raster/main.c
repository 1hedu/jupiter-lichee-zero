/*
 * HSTimer Raster Effects Demo
 *
 * Mid-frame scanline interrupts change the DE2 backdrop color,
 * creating a gradient sky effect — the same technique used by
 * NES/SNES/Genesis for gradient skies, water effects, and split-scroll.
 *
 * Timer0: fires every 32 scanlines, cycles through sky colors
 * Timer1: fires at scanline 180, changes ground palette
 *
 * Build: make GAME=examples/hstimer_raster/main.c
 */
#include "jupiter.h"
#include "nes.h"
#include "hstimer.h"
#include "pmu.h"
#include <string.h>

/* Sky gradient: 8 bands of color, top to bottom */
static const uint32_t sky_colors[] = {
    0xFF000830,  /* deep navy */
    0xFF001050,
    0xFF002070,
    0xFF003090,
    0xFF0040B0,
    0xFF0050C8,
    0xFF0068E0,
    0xFF0080F0,  /* bright blue */
};
#define NUM_SKY_BANDS (sizeof(sky_colors) / sizeof(sky_colors[0]))
#define SCANLINES_PER_BAND 28  /* 224 / 8 = 28 scanlines per band */

/* Current raster state (modified in ISR) */
static volatile uint32_t raster_band = 0;
static volatile uint32_t frame_active = 0;

/* Sky band ISR — fires every SCANLINES_PER_BAND scanlines */
static void sky_band_isr(void)
{
    if (!frame_active) return;

    raster_band++;
    if (raster_band < NUM_SKY_BANDS) {
        /* Change BLD_BKCOLOR and commit via GLB_DBUFF so the mixer
         * latches the new value mid-frame (not deferred to vblank). */
        REG32(0x01100000 + 0x1088) = sky_colors[raster_band];
        REG32(0x01100000 + 0x08) = 1;  /* GLB_DBUFF commit */
    }
}

/* NES tiles */
static uint8_t bg_chr[256 * 16];
static uint8_t palette_ram[32];
static uint8_t nametable[NES_NT_TILES];
static uint8_t attribute[NES_NT_ATTRS];

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();

    uart_puts("\n========================================\n");
    uart_puts("  HSTimer Raster Effects Demo\n");
    uart_puts("========================================\n\n");

    /* GIC + HSTimer */
    irq_init();
    hstimer_init();

    /* NES background: just ground tiles, sky is the backdrop gradient */
    memset(bg_chr, 0, sizeof(bg_chr));

    /* Tile 1: ground surface */
    for (int r = 0; r < 8; r++) {
        bg_chr[1*16 + r]     = 0xFF;
        bg_chr[1*16 + r + 8] = (r < 2) ? 0xFF : 0x00;
    }
    /* Tile 2: dirt */
    for (int r = 0; r < 8; r++) {
        bg_chr[2*16 + r]     = 0xFF;
        bg_chr[2*16 + r + 8] = 0x00;
    }
    /* Tile 3: cloud piece */
    for (int r = 0; r < 8; r++) {
        int cx = 4;
        uint8_t bp0 = 0, bp1 = 0;
        for (int c = 0; c < 8; c++) {
            int dx = c - cx, dy = r - 3;
            int inside = (dx*dx + dy*dy*2) < 10;
            if (inside) { bp0 |= (1 << (7-c)); bp1 |= (1 << (7-c)); }
        }
        bg_chr[3*16 + r] = bp0;
        bg_chr[3*16 + r + 8] = bp1;
    }

    memset(palette_ram, 0x0D, sizeof(palette_ram));
    palette_ram[0] = 0x02;   /* backdrop: dark blue (overridden by raster) */
    palette_ram[1] = 0x17;   /* ground brown */
    palette_ram[2] = 0x27;   /* ground tan */
    palette_ram[3] = 0x30;   /* cloud white */
    palette_ram[5] = 0x19;   /* grass green */
    palette_ram[6] = 0x29;   /* light green */
    palette_ram[7] = 0x37;   /* bright */

    memset(nametable, 0, sizeof(nametable));
    /* Ground at row 25-29 */
    for (int x = 0; x < NES_NT_W; x++) {
        nametable[25 * NES_NT_W + x] = 1;
        for (int y = 26; y < NES_NT_H; y++)
            nametable[y * NES_NT_W + x] = 2;
    }
    /* Scatter some clouds */
    nametable[4 * NES_NT_W + 5] = 3;
    nametable[4 * NES_NT_W + 6] = 3;
    nametable[3 * NES_NT_W + 12] = 3;
    nametable[3 * NES_NT_W + 13] = 3;
    nametable[3 * NES_NT_W + 14] = 3;
    nametable[6 * NES_NT_W + 22] = 3;
    nametable[6 * NES_NT_W + 23] = 3;
    nametable[5 * NES_NT_W + 28] = 3;

    memset(attribute, 0, sizeof(attribute));
    for (int ax = 0; ax < 8; ax++) {
        attribute[6 * 8 + ax] = NES_ATTR(0, 0, 1, 1);
        attribute[7 * 8 + ax] = NES_ATTR(1, 1, 1, 1);
    }

    nes_bg_t bg = {
        .chr = bg_chr, .nametable = nametable, .attribute = attribute,
        .palette_ram = palette_ram, .scroll_x = 0, .scroll_y = 0,
        .line_scroll_x = NULL, .enabled = 1,
    };

    /* Display */
    video_init();
    irq_global_enable();

    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++) {
        fb0[i] = 0xFF000000;
        fb1[i] = 0xFF000000;
    }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    /* Start sky gradient timer: fires every SCANLINES_PER_BAND scanlines */
    hstimer_set_repeating(0, SCANLINES_PER_BAND, sky_band_isr);

    uart_puts("[main] raster effects active!\n");

    int buf = 0;
    int scroll_x = 0;

    while (1) {
        volatile uint32_t *fb = buf ? fb1 : fb0;
        volatile uint32_t *ovl = (volatile uint32_t *)(buf ? OVL1_ADDR : OVL_ADDR);
        uint32_t fb_addr = buf ? FB1_ADDR : FB0_ADDR;
        uint32_t ovl_addr = buf ? OVL1_ADDR : OVL_ADDR;

        /* Wait for vblank — this is when we reset the raster state */
        video_wait_vblank();

        /* Reset raster band counter and set first sky color */
        raster_band = 0;
        frame_active = 1;
        REG32(0x01100000 + 0x1088) = sky_colors[0];  /* BLD_BKCOLOR */
        REG32(0x01100000 + 0x08) = 1;  /* GLB_DBUFF commit */

        /* Slow cloud scroll */
        scroll_x++;
        bg.scroll_x = scroll_x / 3;

        /* Render NES background */
        memset((void *)ovl, 0, LCD_FB_BYTES);
        nes_render((uint32_t *)fb, (uint32_t *)ovl, LCD_W, LCD_H,
                   &bg, NULL, NULL, 0, 1);

        /* After render, mark frame done so ISR stops changing colors */
        frame_active = 0;

        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf = !buf;
    }

    return 0;
}
