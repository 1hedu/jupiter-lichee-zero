/*
 * Jupiter SDK — Parallax Layer Test
 *
 * Pure parallax demonstration for every multi-layer SNES mode.
 * Each layer = different colored checkerboard at different scale.
 * Each layer scrolls at a different speed.
 *
 * Mode 0: 4 layers (2/2/2/2bpp)
 * Mode 1: 3 layers (4/4/2bpp)
 * Mode 2: 2 layers (4/4bpp)
 * Mode 3: 2 layers (8/4bpp)
 * Mode 4: 2 layers (8/2bpp)
 * Mode 5: 2 layers (4/2bpp)
 *
 * Build: make GAME=examples/parallax_test/main.c
 */
#include "jupiter.h"
#include "snes.h"

#define FRAMES_PER_MODE 360  /* 6 seconds each */

/* ---- Tiles: just solid blocks ---- */
static uint8_t t2[2 * 16];   /* 2bpp: tile 0=transparent, tile 1=solid */
static uint8_t t4[2 * 32];   /* 4bpp: tile 0=transparent, tile 1=solid */
static uint8_t t8[2 * 64];   /* 8bpp: tile 0=transparent, tile 1=solid */

static void init_tiles(void)
{
    /* 2bpp tile 1: all pixels = color index 1 */
    for (int r = 0; r < 8; r++) {
        t2[16 + r*2 + 0] = 0x55; /* 01010101 → all pixels = 1 */
        t2[16 + r*2 + 1] = 0x55;
    }
    /* 4bpp tile 1: all pixels = color index 1 */
    for (int r = 0; r < 8; r++) {
        t4[32 + r*4 + 0] = 0x11;
        t4[32 + r*4 + 1] = 0x11;
        t4[32 + r*4 + 2] = 0x11;
        t4[32 + r*4 + 3] = 0x11;
    }
    /* 8bpp tile 1: all pixels = color index 1 */
    for (int i = 0; i < 64; i++)
        t8[64 + i] = 1;
}

/* ---- Palettes: one color per layer ---- */
static const uint32_t pal_red_4[16]    = { 0x00000000, 0xFFDD4444, [2 ... 15] = 0xFFDD4444 };
static const uint32_t pal_blue_4[16]   = { 0x00000000, 0xFF4444DD, [2 ... 15] = 0xFF4444DD };
static const uint32_t pal_red_2[4]     = { 0x00000000, 0xFFDD4444, 0xFFDD4444, 0xFFDD4444 };
static const uint32_t pal_blue_2[4]    = { 0x00000000, 0xFF4444DD, 0xFF4444DD, 0xFF4444DD };
static const uint32_t pal_green_2[4]   = { 0x00000000, 0xFF44DD44, 0xFF44DD44, 0xFF44DD44 };
static const uint32_t pal_yellow_2[4]  = { 0x00000000, 0xFFDDDD44, 0xFFDDDD44, 0xFFDDDD44 };
/* 8bpp: 256 entries, only index 1 used */
static uint32_t pal_red_8[256];
static uint32_t pal_blue_8[256];

/* ---- Maps: checkerboards at different scales ---- */
#define MW 32
#define MH 32
static uint16_t map1[MH * MW]; /* 1×1 tile checker */
static uint16_t map2[MH * MW]; /* 2×2 tile checker */
static uint16_t map3[MH * MW]; /* 3×3 tile checker */
static uint16_t map4[MH * MW]; /* 4×4 tile checker */

static void init_maps(void)
{
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            map1[y*MW+x] = ((x+y) & 1)         ? SNES_ENTRY(1,0,0,0,0) : 0;
            map2[y*MW+x] = (((x>>1)+(y>>1)) & 1) ? SNES_ENTRY(1,0,0,0,0) : 0;
            map3[y*MW+x] = (((x/3)+(y/3)) & 1)  ? SNES_ENTRY(1,0,0,0,0) : 0;
            map4[y*MW+x] = (((x>>2)+(y>>2)) & 1) ? SNES_ENTRY(1,0,0,0,0) : 0;
        }
}

/* ---- Mode labels ---- */
static const char *labels[6] = {
    "Mode 0: 4x 2bpp",
    "Mode 1: 2x 4bpp + 1x 2bpp",
    "Mode 2: 2x 4bpp",
    "Mode 3: 1x 8bpp + 1x 4bpp",
    "Mode 4: 1x 8bpp + 1x 2bpp",
    "Mode 5: 1x 4bpp + 1x 2bpp",
};

/* Offset table (unused here but needed by API) */
static int16_t ofs_dummy[64];
static snes_tile_offset_t ofs_none = { .col_offset = ofs_dummy, .vertical = 0 };

/* ================================================================ */

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — Parallax Layer Test ===\n");
    uart_puts("Modes 0-5, colored checkerboards, different scroll speeds.\n\n");

    timer_init();
    mmu_init();
    init_tiles();
    init_maps();

    /* Init 8bpp palettes */
    for (int i = 0; i < 256; i++) pal_red_8[i] = 0xFFDD4444;
    for (int i = 0; i < 256; i++) pal_blue_8[i] = 0xFF4444DD;
    pal_red_8[0] = 0x00000000;
    pal_blue_8[0] = 0x00000000;

    video_init();
    uart_puts("Display active.\n\n");

    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t frame = 0;
    int cur_mode = -1;
    snes_bg_t bg[4];

    while (1) {
        uint32_t t0 = timer_read();
        int mode = (int)((frame / FRAMES_PER_MODE) % 6);
        uint32_t mf = frame % FRAMES_PER_MODE;

        if (mode != cur_mode) {
            cur_mode = mode;
            uart_puts(">> "); uart_puts(labels[mode]); uart_puts("\n");
            for (int i = 0; i < 4; i++) bg[i].enabled = 0;
        }

        /* Scroll speeds: layer 0 fastest, each subsequent slower */
        int32_t s0 = (int32_t)mf;
        int32_t s1 = (int32_t)(mf * 3 / 4);
        int32_t s2 = (int32_t)(mf / 2);
        int32_t s3 = (int32_t)(mf / 4);

        uint32_t *fb = (uint32_t *)back_fb;
        uint32_t *ovl = (uint32_t *)OVL_ADDR;
        uint32_t backdrop = 0xFF1A1A2A;

        switch (mode) {
        case 0: /* 4× 2bpp: red(1x1), blue(2x2), green(3x3), yellow(4x4) */
            bg[0] = (snes_bg_t){t2,map1,pal_red_2,   s0,0, MW,MH, 2, 1};
            bg[1] = (snes_bg_t){t2,map2,pal_blue_2,  s1,0, MW,MH, 2, 1};
            bg[2] = (snes_bg_t){t2,map3,pal_green_2, s2,0, MW,MH, 2, 1};
            bg[3] = (snes_bg_t){t2,map4,pal_yellow_2,s3,0, MW,MH, 2, 1};
            snes_mode0_render(fb, ovl, LCD_W, LCD_H, backdrop, &bg[0],&bg[1],&bg[2],&bg[3], 0);
            break;

        case 1: /* 2× 4bpp + 1× 2bpp: red(1x1), blue(2x2), green(3x3) */
            bg[0] = (snes_bg_t){t4,map1,pal_red_4,   s0,0, MW,MH, 4, 1};
            bg[1] = (snes_bg_t){t4,map2,pal_blue_4,  s1,0, MW,MH, 4, 1};
            bg[2] = (snes_bg_t){t2,map3,pal_green_2, s2,0, MW,MH, 2, 1};
            snes_mode1_render(fb, ovl, LCD_W, LCD_H, backdrop, &bg[0],&bg[1],&bg[2], 0);
            break;

        case 2: /* 2× 4bpp: red(1x1), blue(2x2) */
            bg[0] = (snes_bg_t){t4,map1,pal_red_4,  s0,0, MW,MH, 4, 1};
            bg[1] = (snes_bg_t){t4,map2,pal_blue_4, s1,0, MW,MH, 4, 1};
            snes_mode2_render(fb, ovl, LCD_W, LCD_H, backdrop, &bg[0],&bg[1], &ofs_none, 0);
            break;

        case 3: /* 1× 8bpp + 1× 4bpp: red(2x2), blue(3x3) */
            bg[0] = (snes_bg_t){t8,map2,pal_red_8,  s0,0, MW,MH, 8, 1};
            bg[1] = (snes_bg_t){t4,map3,pal_blue_4, s1,0, MW,MH, 4, 1};
            snes_mode3_render(fb, ovl, LCD_W, LCD_H, backdrop, &bg[0],&bg[1], 0);
            break;

        case 4: /* 1× 8bpp + 1× 2bpp: red(2x2), blue(3x3) */
            bg[0] = (snes_bg_t){t8,map2,pal_red_8,  s0,0, MW,MH, 8, 1};
            bg[1] = (snes_bg_t){t2,map3,pal_blue_2, s1,0, MW,MH, 2, 1};
            snes_mode4_render(fb, ovl, LCD_W, LCD_H, backdrop, &bg[0],&bg[1], &ofs_none, 0);
            break;

        case 5: /* 1× 4bpp + 1× 2bpp: red(1x1), blue(2x2) */
            bg[0] = (snes_bg_t){t4,map1,pal_red_4,  s0,0, MW,MH, 4, 1};
            bg[1] = (snes_bg_t){t2,map2,pal_blue_2, s1,0, MW,MH, 2, 1};
            snes_mode5_render(fb, ovl, LCD_W, LCD_H, backdrop, &bg[0],&bg[1], 0);
            break;
        }

        dcache_clean_fb(back_fb);
        dcache_clean_fb(OVL_ADDR);
        uint32_t t1 = timer_read();

        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;

        if ((frame % 120) == 0) {
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" m="); uart_putdec(mode);
            uart_puts(" render="); uart_putdec(ticks_to_us(timer_elapsed(t0, t1)));
            uart_puts("us\n");
        }
        frame++;
    }
}
