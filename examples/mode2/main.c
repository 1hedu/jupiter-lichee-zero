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
#define BG2_TILES 18
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
        d[y*8+x] = (y<2) ? ((x==0||x==7) ? 1 : 6)
                         : ((x==0||x==7) ? 1 : ((x&1) ? 3 : 4));
      set4(bg1_td, 3, d); } /* top plank row: sunlit rim on plank tops */

    /* BG2 (static scene):
     * 0: transparent
     * 1: sky dusk, 2: chasm shadow, 3: cliff, 4: cliff rim battlement
     * 6: ground/earth, 7: grass top
     * 8..10: sky bands rose/amber/glow, 11..13: band dither teeth
     * 14..17: low sun disc (2x2 tiles) */
    fill4(bg2_td, 1, 1);
    fill4(bg2_td, 2, 2);
    fill4(bg2_td, 3, 3);
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++) {
        int b = (x&4) ? 2 : 6;              /* battlement boundary */
        d[y*8+x] = (y < b) ? 10 : (y == b) ? 7 : 3;
      } set4(bg2_td, 4, d); } /* cliff-top teeth, sun-rim on the edge */
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = ((x+y*2)&3)==0 ? 5 : 4;
      set4(bg2_td, 6, d); } /* earth */
    { uint8_t d[64]; for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = y<1 ? 11 : y<3 ? 6 : (((x+y*2)&3)==0 ? 5 : 4);
      set4(bg2_td, 7, d); } /* grass top, lit edge */
    fill4(bg2_td, 8, 8);   /* rose band */
    fill4(bg2_td, 9, 9);   /* amber band */
    fill4(bg2_td, 10, 10); /* glow band */
    /* battlement dither tiles: 4px teeth between adjacent sky bands */
    { static const uint8_t pair[3][2] = { {1,8}, {8,9}, {9,10} };
      for (int i = 0; i < 3; i++) {
        uint8_t d[64];
        for (int y=0;y<8;y++) for (int x=0;x<8;x++)
            d[y*8+x] = (y < ((x&4)?2:6)) ? pair[i][0] : pair[i][1];
        set4(bg2_td, 11+i, d);
      } }
    /* low sun: 16x16 disc over the amber/glow horizon bands */
    for (int q = 0; q < 4; q++) {
        uint8_t d[64];
        for (int y=0;y<8;y++) for (int x=0;x<8;x++) {
            int gx = (q&1)*8+x, gy = (q>>1)*8+y;
            int dx = gx-8, dy = gy-11;
            uint8_t bg = (gy<8) ? ((y < ((x&4)?2:6)) ? 9 : 10) : 10;
            d[y*8+x] = (dx*dx + dy*dy < 49) ? 12 : bg;
        }
        set4(bg2_td, 14+q, d);
    }
}

/* ---- Palettes ---- */
static const uint32_t bg1_pal[16] = {
    0x00000000,     /* 0: transparent */
    0xFFC89858,     /* 1: rope, sunlit */
    0xFF3A241A,     /* 2: wood dark */
    0xFF7A4E2E,     /* 3: wood light, warm-lit */
    0xFF5A3822,     /* 4: wood mid */
    0xFF2A1810,     /* 5: plank edge */
    0xFFF0B068,     /* 6: golden rim on plank tops */
    [7 ... 15] = 0xFF000000,
};
static const uint32_t bg2_pal[16] = {
    0x00000000,     /* 0: transparent */
    0xFF3E3A68,     /* 1: sky dusk violet-blue (top) */
    0xFF241F38,     /* 2: chasm shadow */
    0xFF54382C,     /* 3: cliff, warm-lit dark */
    0xFF483226,     /* 4: earth */
    0xFF32211A,     /* 5: earth dark */
    0xFF6A8A38,     /* 6: grass, warm-tinted */
    0xFFC08048,     /* 7: cliff rim highlight */
    0xFFB05868,     /* 8: sky rose band */
    0xFFE08850,     /* 9: sky amber band */
    0xFFF6B468,     /* 10: horizon glow band */
    0xFF9AB048,     /* 11: grass lit edge */
    0xFFFFDC96,     /* 12: sun disc */
    [13 ... 15] = 0xFF3E3A68,
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
                if (y == bridge_y)
                    t1 = SNES_ENTRY(3, 0, 0, 0, 0); /* top row: sunlit rim */
                else if (y > bridge_y && y <= bridge_y + 4)
                    t1 = SNES_ENTRY(1 + (y & 1), 0, 0, 0, 0); /* alternating plank tiles */
            }
            bg1_map[y * MW + x] = t1;

            /* BG2: sky, cliffs, ground, pillars — all static */
            uint16_t t2;
            if (y < 9)
                t2 = SNES_ENTRY(1, 0, 0, 0, 0);   /* dusk violet-blue */
            else if (y == 9)
                t2 = SNES_ENTRY(11, 0, 0, 0, 0);  /* teeth dusk/rose */
            else if (y == 10 || y == 11)
                t2 = SNES_ENTRY(8, 0, 0, 0, 0);   /* rose */
            else if (y == 12)
                t2 = SNES_ENTRY(12, 0, 0, 0, 0);  /* teeth rose/amber */
            else if (y == 13)
                t2 = SNES_ENTRY(9, 0, 0, 0, 0);   /* amber */
            else if (y == 14 || y == 15) {
                /* horizon: teeth amber/glow, then glow — low sun at cols 30-31 */
                if (x == 30 || x == 31)
                    t2 = SNES_ENTRY(14 + (x-30) + (y-14)*2, 0, 0, 0, 0);
                else
                    t2 = (y == 14) ? SNES_ENTRY(13, 0, 0, 0, 0)
                                   : SNES_ENTRY(10, 0, 0, 0, 0);
            }
            else if (y == 16)
                t2 = SNES_ENTRY(4, 0, 0, 0, 0);   /* cliff rim battlement */
            else if (y >= 17 && y < 20)
                t2 = SNES_ENTRY(3, 0, 0, 0, 0);  /* cliff */
            else
                t2 = SNES_ENTRY(2, 0, 0, 0, 0);  /* chasm shadow */

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
    uint32_t backdrop = 0xFFF6B468;

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
