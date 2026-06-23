/*
 * Jupiter SDK — Mode 7 + Sprites
 *
 * The full visual pipeline: Mode 7 affine vortex floor on VI0,
 * hero + enemies + stars on UI0 overlay, hardware composited.
 */
#include "jupiter.h"
#include "snes.h"   /* snes_mode7_render, snes_sin_lut, snes_cos_lut */

#define SIN_MASK    255   /* still used for the camera-oscillation index below */

/* ---- Floor tile map ---- */
#define MAP_W       64
#define MAP_H       64
#define MAP_MASK    63

static uint8_t floor_map[MAP_H * MAP_W];

static const uint32_t floor_lut[256] = {
    [0] = 0x001A1A40, [1] = 0x00303070,
    [2] = 0x00402020, [3] = 0x00704030,
    [4] = 0x00103010, [5] = 0x00206020,
    [6] = 0x00501050, [7] = 0x00802080,
};

static void build_map(void)
{
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t tile = ((x ^ y) & 1) ? 1 : 0;
            if ((x & 7) == 0 || (y & 7) == 0)
                tile = ((x ^ y) & 1) ? 3 : 2;
            int cx = (x - 32) < 0 ? -(x - 32) : (x - 32);
            int cy = (y - 32) < 0 ? -(y - 32) : (y - 32);
            int dist = cx + cy;
            if (dist >= 12 && dist <= 14) tile = ((x ^ y) & 1) ? 5 : 4;
            if (dist >= 24 && dist <= 26) tile = ((x ^ y) & 1) ? 5 : 4;
            if (cx <= 2 && cy <= 2) tile = ((x ^ y) & 1) ? 7 : 6;
            floor_map[y * MAP_W + x] = tile;
        }
    }
}

/* ---- Sky (above mode 7 horizon) ---- */
#define HORIZON     (LCD_H / 2)

static void render_sky(uint32_t fb_addr)
{
    for (uint32_t y = 0; y < HORIZON; y++) {
        uint32_t t = (y * 256) / HORIZON;
        uint32_t r = 0x08 + (t * 0x30 >> 8);
        uint32_t g = 0x04 + (t * 0x18 >> 8);
        uint32_t b = 0x20 + (t * 0x50 >> 8);
        if (t > 200) {
            uint32_t glow = (t - 200) * 5;
            r += glow >> 2; g += glow >> 3; b += glow >> 4;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
        }
        memset32_neon(fb_addr + y * LCD_W * 4, (r << 16) | (g << 8) | b, LCD_W * 4);
    }
}

/* Mode 7 floor is now snes_mode7_render() from lib/snes.c — no
 * per-demo affine-projection code needed. */

/* ---- Procedural sprites (same as sprites demo) ---- */

#define HERO_W 24
#define HERO_H 24
static uint32_t hero_data[HERO_W * HERO_H];

static void gen_hero(void)
{
    for (int y = 0; y < HERO_H; y++)
        for (int x = 0; x < HERO_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 12, cy = y - 12;
            if (cy >= -11 && cy <= -6) { int dx=cx, dy=cy+8; if(dx*dx+dy*dy<=12) px=0xFFFFCC66; }
            if (cy == -9 && (cx == -2 || cx == 2)) px = 0xFF202020;
            if (cy >= -5 && cy <= 3 && cx >= -4 && cx <= 4) px = 0xFFFF8800;
            if (cy == 1 && cx >= -4 && cx <= 4) px = 0xFF804000;
            if (cy >= -4 && cy <= 0 && (cx == -5 || cx == 5)) px = 0xFFFFCC66;
            if (cy >= 4 && cy <= 8 && cx >= -3 && cx <= -1) px = 0xFF4444AA;
            if (cy >= 4 && cy <= 8 && cx >= 1 && cx <= 3) px = 0xFF4444AA;
            if (cy >= 9 && cy <= 10 && ((cx >= -4 && cx <= -1) || (cx >= 1 && cx <= 4))) px = 0xFF663300;
            hero_data[y * HERO_W + x] = px;
        }
}

#define ENEMY_W 16
#define ENEMY_H 16
static uint32_t enemy_data[ENEMY_W * ENEMY_H];

static void gen_enemy(void)
{
    for (int y = 0; y < ENEMY_H; y++)
        for (int x = 0; x < ENEMY_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 8, cy = y - 8;
            int d2 = cx*cx + cy*cy;
            if (d2 <= 42) px = 0xFFCC2244;
            if (d2 <= 20) px = 0xFFFF4466;
            if (cy >= -2 && cy <= 0 && (cx == -3 || cx == 3)) px = (cy == -1) ? 0xFFFFFF00 : 0xFFFF8800;
            if (cy <= -6 && cy >= -7 && (cx == 0 || cx == -4 || cx == 4)) px = 0xFFCC2244;
            enemy_data[y * ENEMY_W + x] = px;
        }
}

#define STAR_W 8
#define STAR_H 8
static uint32_t star_data[STAR_W * STAR_H];

static void gen_star(void)
{
    for (int y = 0; y < STAR_H; y++)
        for (int x = 0; x < STAR_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 4, cy = y - 4;
            if ((cx == 0 && cy >= -2 && cy <= 2) || (cy == 0 && cx >= -2 && cx <= 2))
                px = 0xCCFFFFFF;
            if (cx == 0 && cy == 0) px = 0xFFFFFFCC;
            star_data[y * STAR_W + x] = px;
        }
}

/* ---- Movers ---- */
#define NUM_ENEMIES 8
#define NUM_STARS   16

typedef struct { int x, y, dx, dy; } mover_t;

static mover_t hero_m;
static mover_t enemies[NUM_ENEMIES];
static mover_t stars[NUM_STARS];

static void init_movers(void)
{
    hero_m = (mover_t){ 220, 130, 2, 1 };
    for (int i = 0; i < NUM_ENEMIES; i++)
        enemies[i] = (mover_t){
            50 + (i * 53) % 400, 30 + (i * 37) % 200,
            ((i & 1) ? 1 : -1) * (1 + i % 3),
            ((i & 2) ? 1 : -1) * (1 + (i+1) % 2)
        };
    for (int i = 0; i < NUM_STARS; i++)
        stars[i] = (mover_t){
            (i * 31) % (int)LCD_W, (i * 17) % (int)LCD_H,
            ((i & 1) ? 1 : -1), ((i & 2) ? 1 : -1)
        };
}

static void bounce(mover_t *m, int w, int h)
{
    m->x += m->dx; m->y += m->dy;
    if (m->x < -w/2 || m->x + w/2 >= (int)LCD_W) { m->dx = -m->dx; m->x += m->dx; }
    if (m->y < -h/2 || m->y + h/2 >= (int)LCD_H) { m->dy = -m->dy; m->y += m->dy; }
}

/* ==== ENTRY ==== */
void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — Mode 7 + Sprites ===\n");
    timer_init();

    build_map();
    gen_hero();
    gen_enemy();
    gen_star();
    init_movers();

    memset32_neon(FB0_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    mmu_init();
    uart_puts("Display + MMU active.\n");
    uart_puts("Entering Mode 7 + Sprites (double-buffered OVL)...\n\n");

    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;

    /* Grab once — snes_sin/cos_lut lazily build on first call. */
    const int32_t *s_sin = snes_sin_lut();
    const int32_t *s_cos = snes_cos_lut();

    while (1) {
        uint32_t t0 = timer_read();

        /* Background: Mode 7 (sky painted by us, floor by lib helper). */
        render_sky(back_fb);
        snes_mode7_t m7 = {
            .cam_x       = (int32_t)(frame * 30) + (s_sin[(frame * 3) & SIN_MASK] >> 6),
            .cam_y       = (int32_t)(frame * 20) + (s_cos[(frame * 2) & SIN_MASK] >> 6),
            .angle       = (uint8_t)frame,
            .twist       = 1,                /* matches old TWIST=4 → lam/4 swirl */
            .horizon     = HORIZON,
            .space_z     = 8000,
            .map         = floor_map,
            .palette     = floor_lut,
            .map_w_bits  = 6,                /* log2(MAP_W=64) */
            .map_mask    = MAP_MASK,         /* MAP_W - 1 */
        };
        snes_mode7_render((uint32_t *)back_fb, (uint32_t *)0, LCD_W, LCD_H, &m7, 0);
        uint32_t t_bg = timer_read();

        /* Sprites: clear entire back overlay, draw fresh */
        volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;
        memset32_neon(back_ovl, 0x00000000, LCD_W * LCD_H * 4);

        bounce(&hero_m, HERO_W, HERO_H);
        for (int i = 0; i < NUM_ENEMIES; i++) bounce(&enemies[i], ENEMY_W, ENEMY_H);
        for (int i = 0; i < NUM_STARS; i++) bounce(&stars[i], STAR_W, STAR_H);

        if (hero_m.dx > 0)
            sprite_blit(ovl, LCD_W, hero_data, HERO_W, HERO_H, hero_m.x, hero_m.y);
        else
            sprite_blit_flip(ovl, LCD_W, hero_data, HERO_W, HERO_H, hero_m.x, hero_m.y);

        for (int i = 0; i < NUM_ENEMIES; i++) {
            if (enemies[i].dx > 0)
                sprite_blit(ovl, LCD_W, enemy_data, ENEMY_W, ENEMY_H, enemies[i].x, enemies[i].y);
            else
                sprite_blit_flip(ovl, LCD_W, enemy_data, ENEMY_W, ENEMY_H, enemies[i].x, enemies[i].y);
        }
        for (int i = 0; i < NUM_STARS; i++)
            sprite_blit(ovl, LCD_W, star_data, STAR_W, STAR_H, stars[i].x, stars[i].y);

        uint32_t t_spr = timer_read();

        /* Flush + swap BOTH layers — overlay needs its OWN flush
         * (dcache_clean_fb is for FB-sized buffers; OVL writes via
         * sprite_blit cache lines and DE2 reads from DRAM, so without
         * dcache_clean_range on back_ovl the sprite layer is stale). */
        dcache_clean_fb(back_fb);
        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);
        video_swap(back_fb);
        video_set_overlay(back_ovl);

        uint32_t tmp;
        tmp = back_fb;  back_fb  = front_fb;  front_fb  = tmp;
        tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        uint32_t us_bg  = ticks_to_us(timer_elapsed(t0, t_bg));
        uint32_t us_spr = ticks_to_us(timer_elapsed(t_bg, t_spr));
        uint32_t us_tot = ticks_to_us(timer_elapsed(t0, timer_read()));

        if ((frame % 120) == 0) {
            uart_puts("f=");
            uart_putdec(frame);
            uart_puts(" m7=");
            uart_putdec(us_bg);
            uart_puts(" spr=");
            uart_putdec(us_spr);
            uart_puts(" total=");
            uart_putdec(us_tot);
            uart_puts("us\n");
        }
        frame++;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            ;
    }
}
