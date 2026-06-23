/*
 * Jupiter SDK — SNES Mode 2: Waving Bridge
 *
 * BG1: Bridge ONLY (rope + plank). Gets per-column vertical offset.
 * BG2: Everything static — sky, cliffs, ground, pillars.
 *
 * This is how real SNES games use Mode 2: the offset layer contains
 * ONLY the element that moves. Static scenery goes on the other layer.
 *
 * Build: make GAME=examples/mode2/main.c
 */
#include "jupiter.h"
#include "snes.h"

/* ---- Tiles ---- */
#define BG1_TILES 4
#define BG2_TILES 8
static uint8_t bg1_td[BG1_TILES * 32];
static uint8_t bg2_td[BG2_TILES * 32];

static void set4(uint8_t *base, int t, const uint8_t d[64])
{
    uint8_t *p = base + t * 32;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++)
            p[r*4+c] = (d[r*8+c*2] << 4) | d[r*8+c*2+1];
}

static void fill4(uint8_t *base, int t, uint8_t v)
{
    uint8_t d[64]; for (int i = 0; i < 64; i++) d[i] = v;
    set4(base, t, d);
}

/* Sine table: 64 entries, ±6 pixels */
#define SIN_N 64
static int16_t sin_tbl[SIN_N];
static void init_sin(void)
{
    /* 17 entries covers 0..16 inclusive (quarter wave) */
    static const int8_t qtr[17] = {0,1,1,2,3,3,4,4,5,5,5,6,6,6,6,5,5};
    for (int i = 0; i <= 16; i++) {
        sin_tbl[i & 63]        =  qtr[i];
        sin_tbl[(32 - i) & 63] =  qtr[i];
        sin_tbl[(32 + i) & 63] = -qtr[i];
        sin_tbl[(64 - i) & 63] = -qtr[i];
    }
    sin_tbl[0] = 0;
}

static void init_tiles(void)
{
    /* BG1 (bridge only):
     * 0: transparent
     * 1: bridge body — uniform wood grain, same pattern every row.
     *    Vertically seamless so offset boundary crossings are invisible. */
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = (x==0||x==7) ? 1 : ((x&1) ? 3 : 4);
      set4(bg1_td, 1, d); } /* vertical planks with rope edges */
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = (x==0||x==7) ? 1 : ((x&1) ? 4 : 3);
      set4(bg1_td, 2, d); } /* alternating plank (for variety) */
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = (x==0||x==7) ? 1 : ((x&1) ? 3 : 4);
      set4(bg1_td, 3, d); } /* same as tile 1 */

    /* BG2 (static scene):
     * 0: transparent
     * 1: sky light, 2: sky dark, 3: cliff, 4: cliff slope-L, 5: slope-R
     * 6: ground/earth, 7: grass top */
    fill4(bg2_td, 1, 1);
    fill4(bg2_td, 2, 2);
    fill4(bg2_td, 3, 3);
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = (x >= (7-y)) ? 3 : 0;
      set4(bg2_td, 4, d); }
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = (x <= y) ? 3 : 0;
      set4(bg2_td, 5, d); }
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = ((x+y*2)&3)==0 ? 5 : 4;
      set4(bg2_td, 6, d); } /* earth */
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = y<3 ? 6 : (((x+y*2)&3)==0 ? 5 : 4);
      set4(bg2_td, 7, d); } /* grass top */
}

/* ---- Palettes ---- */
static const uint32_t bg1_pal[16] = {
    0x00000000,     /* 0: transparent */
    0xFFAA8844,     /* 1: rope */
    0xFF6A4A2A,     /* 2: wood dark */
    0xFFA08050,     /* 3: wood light */
    0xFF8A6A40,     /* 4: wood mid */
    0xFF4A3020,     /* 5: plank edge */
    [6 ... 15] = 0xFF000000,
};
static const uint32_t bg2_pal[16] = {
    0x00000000,     /* 0: transparent */
    0xFF6A9ADA,     /* 1: sky light */
    0xFF4A7ABA,     /* 2: sky dark */
    0xFF7A6A5A,     /* 3: cliff/stone */
    0xFF5A4A38,     /* 4: earth */
    0xFF4A3A28,     /* 5: earth dark */
    0xFF40A030,     /* 6: grass */
    [7 ... 15] = 0xFF3A5A8A,
};

/* ---- Maps ---- */
#define MW 64
#define MH 64
static uint16_t bg1_map[MH * MW];
static uint16_t bg2_map[MH * MW];

#define BRIDGE_START 12
#define BRIDGE_END   48
#define PILLAR_L_COL 10
#define PILLAR_R_COL 50

static void build_maps(void)
{
    int bridge_y = 20; /* tile row for bridge */

    for (int y = 0; y < MH; y++) {
        for (int x = 0; x < MW; x++) {
            /* BG1: bridge only. Transparent everywhere else. */
            uint16_t t1 = 0;
            if (x >= BRIDGE_START && x <= BRIDGE_END) {
                if (y >= bridge_y && y <= bridge_y + 4)
                    t1 = SNES_ENTRY(1 + (y & 1), 0, 0, 0, 0); /* alternating plank tiles */
            }
            bg1_map[y * MW + x] = t1;

            /* BG2: sky, cliffs, ground, pillars — all static */
            uint16_t t2;
            if (y < 12)
                t2 = SNES_ENTRY(1, 0, 0, 0, 0);  /* sky light */
            else if (y < 16)
                t2 = SNES_ENTRY(2, 0, 0, 0, 0);  /* sky dark */
            else if (y == 16) {
                int m = x % 18;
                t2 = (m < 9) ? SNES_ENTRY(4,0,0,0,0) : SNES_ENTRY(5,0,0,0,0);
            } else if (y >= 17 && y < 20)
                t2 = SNES_ENTRY(3, 0, 0, 0, 0);  /* cliff */
            else
                t2 = SNES_ENTRY(2, 0, 0, 0, 0);  /* dark / abyss */

            /* Ground on sides — priority 1 so it draws in front of bridge */
            if (x <= PILLAR_L_COL || x >= PILLAR_R_COL) {
                if (y == bridge_y)
                    t2 = SNES_ENTRY(7, 0, 1, 0, 0); /* grass top, pri=1 */
                else if (y > bridge_y)
                    t2 = SNES_ENTRY(6, 0, 1, 0, 0); /* earth, pri=1 */
            }

            /* Pillars — priority 1, in front of bridge */
            if (x == PILLAR_L_COL || x == PILLAR_R_COL ||
                x == PILLAR_L_COL+1 || x == PILLAR_R_COL-1) {
                if (y >= bridge_y - 3 && y <= bridge_y + 4)
                    t2 = SNES_ENTRY(3, 0, 1, 0, 0); /* stone pillar, pri=1 */
            }

            bg2_map[y * MW + x] = t2;
        }
    }
}

/* ---- Offset table ---- */
#define MAX_COLS 64
static int16_t offset_table[MAX_COLS];

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — SNES Mode 2: Waving Bridge ===\n");
    uart_puts("BG1=bridge (offset). BG2=scenery (static).\n\n");

    timer_init();
    mmu_init();
    init_sin();
    init_tiles();
    build_maps();

    snes_bg_t bg1 = { .tiles=bg1_td, .map=bg1_map, .palette=bg1_pal,
                       .map_w=MW, .map_h=MH, .bpp=4, .enabled=1 };
    snes_bg_t bg2 = { .tiles=bg2_td, .map=bg2_map, .palette=bg2_pal,
                       .map_w=MW, .map_h=MH, .bpp=4, .enabled=1 };

    snes_tile_offset_t ofs = {
        .col_offset = offset_table,
        .vertical = 1,
    };

    video_init();
    uart_puts("Display active.\n\n");

    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t *ovl = (uint32_t *)OVL_ADDR;
    uint32_t frame = 0;
    uint32_t backdrop = 0xFF4A7ABA;

    bg1.scroll_y = 60;
    bg2.scroll_y = 60;  /* SAME scroll_y — bridge must align with pillars */

    while (1) {
        uint32_t t0 = timer_read();

        /* Both BGs scroll together — bridge stays anchored to pillars.
         * (Parallax was Mode 1's demo. Mode 2 is about per-tile offset.) */
        int32_t sx = (int32_t)(frame / 3);
        bg1.scroll_x = sx;
        bg2.scroll_x = sx;

        /* Continuous sine wave — every column, no discontinuity */
        for (int c = 0; c < MAX_COLS; c++) {
            int phase = (int)(frame + c * 2) & (SIN_N - 1);
            offset_table[c] = sin_tbl[phase];
        }

        snes_mode2_render((uint32_t *)back_fb, ovl, LCD_W, LCD_H,
                          backdrop, &bg1, &bg2, &ofs, 0);

        dcache_clean_fb(back_fb);
        dcache_clean_fb(OVL_ADDR);
        uint32_t t1 = timer_read();

        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;

        if ((frame % 120) == 0) {
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" render="); uart_putdec(ticks_to_us(timer_elapsed(t0, t1)));
            uart_puts("us\n");
        }
        frame++;
    }
}
