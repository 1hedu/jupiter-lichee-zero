/*
 * Jupiter SDK Example — Parallax Tile Scrolling
 * Two-speed tile background + glowing firefly overlay.
 * Demonstrates: tiles, parallax, double buffer, overlay, frame pacing.
 *
 * Scene: a moonlit pine ridge. The slow far layer carries the night
 * sky, a cratered moon, and a silhouetted treeline; the fast near
 * layer is a dark meadow with per-pixel grass blades and dew sparks.
 * tiles_render()'s per-pixel color callback does all the texturing —
 * each tile id is a tiny 8x8 procedural texture, not a flat color.
 */
#include "jupiter.h"

#define MAP_W      80
#define MAP_H_FAR  17
#define MAP_H_NEAR 17
#define HORIZON    (LCD_H / 2)

static uint8_t sky_map[MAP_H_FAR * MAP_W];
static uint8_t ground_map[MAP_H_NEAR * MAP_W];

/* Sky tile ids */
#define T_DEEP    0   /* zenith                     */
#define T_MID     1   /* mid sky                    */
#define T_HAZE    2   /* horizon haze               */
#define T_STAR_A  3   /* deep sky + star            */
#define T_STAR_B  4   /* mid sky + star             */
#define T_MOON_TL 5   /* 2x2-tile moon quadrants    */
#define T_MOON_TR 6
#define T_MOON_BL 7
#define T_MOON_BR 8
#define T_RIDGE   9   /* solid silhouette           */
#define T_PINE_T  10  /* pine crown (on haze)       */
#define T_PINE_M  11  /* pine flank (on haze)       */
#define T_D2M     12  /* deep→mid Bayer transition  */
#define T_M2H     13  /* mid→haze Bayer transition  */

#define C_DEEP  0xFF0A0E20
#define C_MID   0xFF101830
#define C_HAZE  0xFF1A2440
#define C_SILH  0xFF0E1526

/* 4x4 ordered-dither threshold (0..15) — the classic Bayer matrix */
static const uint8_t bayer4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

/* 50% pixel-level mix of two colors, retro ordered-dither style */
static inline uint32_t dither2(uint32_t a, uint32_t b,
                               uint32_t px, uint32_t py)
{
    return (bayer4[py & 3][px & 3] < 8) ? a : b;
}

static uint32_t sky_color(uint8_t id, uint32_t px, uint32_t py)
{
    switch (id) {
    case T_DEEP: return C_DEEP;
    case T_MID:  return C_MID;
    case T_HAZE: return C_HAZE;
    case T_STAR_A:
        if (px == 3 && py == 2) return 0xFFE8F0FF;
        if (px == 6 && py == 5) return 0xFF56659A;
        return C_DEEP;
    case T_STAR_B:
        if (px == 1 && py == 6) return 0xFFC8D4F0;
        return C_MID;
    case T_MOON_TL: case T_MOON_TR: case T_MOON_BL: case T_MOON_BR: {
        /* 16x16 disc assembled from four tiles */
        uint32_t mx = px + ((id == T_MOON_TR || id == T_MOON_BR) ? 8 : 0);
        uint32_t my = py + ((id == T_MOON_BL || id == T_MOON_BR) ? 8 : 0);
        int dx = (int)mx - 8, dy = (int)my - 8;
        int d2 = dx * dx + dy * dy;
        if (d2 > 49) return (my < 8) ? C_DEEP : C_MID;
        /* craters: fixed dark maria spots + rim shading */
        if ((mx == 5 && my == 6) || (mx == 6 && my == 6) ||
            (mx == 10 && my == 9) || (mx == 6 && my == 11) ||
            (mx == 11 && my == 5))
            return 0xFFC2BCA4;
        if (d2 > 40) return 0xFFD4CEB6;
        return 0xFFEAE6D2;
    }
    case T_RIDGE: return C_SILH;
    case T_D2M: return dither2(C_DEEP, C_MID, px, py);
    case T_M2H: return dither2(C_MID, C_HAZE, px, py);
    case T_PINE_T:   /* narrow crown widening downward */
        return ((int)px * 2 - 7 <= (int)py * 2 && 7 - (int)px * 2 <= (int)py * 2)
                   ? C_SILH : C_HAZE;
    case T_PINE_M:   /* wider flank, nearly full by the bottom row */
        return ((int)px * 2 - 7 <= (int)py * 2 + 6 && 7 - (int)px * 2 <= (int)py * 2 + 6)
                   ? C_SILH : C_HAZE;
    default: return 0xFF000000;
    }
}

/* Ground tile ids */
#define G_EDGE  16   /* moonlit horizon edge  */
#define G_HI    17
#define G_MID   18
#define G_LOW   19
#define G_DEEP  20
#define G_DEW   21   /* mid meadow + dew spark */
#define G_S1    22   /* hi→mid Bayer seam      */
#define G_S2    23   /* mid→low Bayer seam     */
#define G_S3    24   /* low→deep Bayer seam    */

static uint32_t blades(uint32_t base, uint32_t lite,
                       uint32_t px, uint32_t py, uint32_t seed)
{
    /* sparse vertical grass blades, deterministic per tile-pixel */
    uint32_t h = (px * 7u + seed * 5u) % 11u;
    if (h == 0 && py >= 2) return lite;
    if (h == 5 && py >= 5) return lite;
    return base;
}

static uint32_t ground_color(uint8_t id, uint32_t px, uint32_t py)
{
    switch (id) {
    case G_EDGE: return blades(0xFF23483A, 0xFF35604C, px, py, 1);
    case G_HI:   return blades(0xFF1B3A2E, 0xFF26523F, px, py, 2);
    case G_MID:  return blades(0xFF152E24, 0xFF1E4433, px, py, 3);
    case G_LOW:  return blades(0xFF10241C, 0xFF16342A, px, py, 4);
    case G_DEEP: return blades(0xFF0B1914, 0xFF102820, px, py, 5);
    case G_DEW:
        if (px == 3 && py == 3) return 0xFF9FD8C8;
        if ((px == 2 && py == 3) || (px == 4 && py == 3) ||
            (px == 3 && py == 2) || (px == 3 && py == 4)) return 0xFF4E8878;
        return blades(0xFF152E24, 0xFF1E4433, px, py, 3);
    case G_S1: return dither2(0xFF1B3A2E, 0xFF152E24, px, py);
    case G_S2: return dither2(0xFF152E24, 0xFF10241C, px, py);
    case G_S3: return dither2(0xFF10241C, 0xFF0B1914, px, py);
    default: return 0xFF000000;
    }
}

/* Firefly: 16x16 radial glow with a hot core. Semi-transparent halo —
 * the DE2 blends UI0 per-pixel alpha over the scene. */
#define FLY_W 16
#define FLY_H 16
static uint32_t fly_data[FLY_W * FLY_H];

static void gen_firefly(void)
{
    for (int y = 0; y < FLY_H; y++)
        for (int x = 0; x < FLY_W; x++) {
            int dx = x - 8, dy = y - 8;
            int d2 = dx * dx + dy * dy;
            uint32_t px = 0x00000000;
            if (d2 <= 2)       px = 0xFFFFF6D0;   /* core        */
            else if (d2 <= 6)  px = 0xFFFFDF8A;
            else if (d2 <= 14) px = 0xB0F0C060;   /* warm glow   */
            else if (d2 <= 26) px = 0x60B08838;
            else if (d2 <= 42) px = 0x30705A28;   /* faint halo  */
            fly_data[y * FLY_W + x] = px;
        }
}

static uint32_t hash2(uint32_t x, uint32_t y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void main(void)
{
    uart_puts("\n\n=== Parallax Demo ===\n");
    timer_init();
    gen_firefly();

    /* ---- Sky map: gradient rows, stars, moon, treeline ---- */
    for (uint32_t y = 0; y < MAP_H_FAR; y++)
        for (uint32_t x = 0; x < MAP_W; x++) {
            uint8_t t = (y < 6) ? T_DEEP : (y < 11) ? T_MID : T_HAZE;
            /* chunky cell-level dither on the band seams */
            if ((y == 5 || y == 10) && (hash2(x, y) & 1))
                t = (y == 5) ? T_MID : T_HAZE;
            /* sparse stars, denser high up */
            uint32_t h = hash2(x, y);
            if (y < 5 && h % 7 == 0) t = T_STAR_A;
            else if (y < 10 && h % 11 == 0) t = T_STAR_B;
            sky_map[y * MAP_W + x] = t;
        }
    /* moon: 2x2 tiles, high in the deep band */
    sky_map[1 * MAP_W + 58] = T_MOON_TL;
    sky_map[1 * MAP_W + 59] = T_MOON_TR;
    sky_map[2 * MAP_W + 58] = T_MOON_BL;
    sky_map[2 * MAP_W + 59] = T_MOON_BR;
    /* treeline: a gappy forest — tall pines (crown 13, flank 14-15),
     * short pines (crown 14, flank 15), gaps showing haze, ridge 16 */
    for (uint32_t x = 0; x < MAP_W; x++) {
        uint32_t h = hash2(x, 99) % 5;
        if (h < 2) {                      /* tall pine */
            sky_map[13 * MAP_W + x] = T_PINE_T;
            sky_map[14 * MAP_W + x] = T_PINE_M;
            sky_map[15 * MAP_W + x] = T_RIDGE;
        } else if (h < 4) {               /* short pine */
            sky_map[14 * MAP_W + x] = T_PINE_T;
            sky_map[15 * MAP_W + x] = T_PINE_M;
        } else {                          /* gap */
            sky_map[15 * MAP_W + x] = T_PINE_T;
        }
        sky_map[16 * MAP_W + x] = T_RIDGE;
    }

    /* ---- Ground map: banded meadow with dew sparks ---- */
    for (uint32_t y = 0; y < MAP_H_NEAR; y++)
        for (uint32_t x = 0; x < MAP_W; x++) {
            uint8_t t = (y == 0) ? G_EDGE :
                        (y < 5)  ? G_HI  :
                        (y < 9)  ? G_MID :
                        (y < 13) ? G_LOW : G_DEEP;
            /* chunky cell-level dither on band seams */
            if ((y == 4 || y == 8 || y == 12) && (hash2(x, y) & 1))
                t = (y == 4) ? G_MID : (y == 8) ? G_LOW : G_DEEP;
            if (y >= 2 && y < 11 && (hash2(x, y + 40) % 37) == 0)
                t = G_DEW;
            ground_map[y * MAP_W + x] = t;
        }

    memset32_neon(FB0_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0, LCD_W * LCD_H * 4);

    video_init();
    uart_puts("Display live.\n");

    int sx = 200, sy = 60, dx = 3, dy = 2, sw = FLY_W, sh = FLY_H;
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
        /* Full-clear back overlay each frame then draw the firefly
         * (gentle sine bob layered on the bounce). */
        memset32_neon(back_ovl, 0, LCD_W * LCD_H * 4);
        {
            static const int8_t bob[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
            sprite_blit(ovl, LCD_W, fly_data, FLY_W, FLY_H,
                        sx, sy + bob[(frame >> 2) & 7]);
        }

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
