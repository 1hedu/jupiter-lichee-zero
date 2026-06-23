/*
 * Jupiter SDK — Mode 7 + Sprites (NEON ASM Edition)
 *
 * The full visual pipeline, now running through hand-tuned NEON:
 *
 *   1. Mode 7 floor    → mode7_scanline()       [mode7_neon.S]
 *   2. Sprite blit      → sprite_blit()          [sprite_neon.S: _sprite_row_keyed]
 *   3. Sprite flip      → sprite_blit_flip()     [sprite_neon.S: _sprite_row_keyed_flip]
 *   4. Alpha blend      → sprite_blit_blend()    [sprite_neon.S: _sprite_row_blend]
 *   5. Tile parallax    → tiles_render_fast()    [tiles_neon.S: _tile_row_blast]
 *
 * Compare these numbers to the C baselines in PROGRESS.md:
 *   Mode 7 + 25 sprites (C, D-cache):  12,960 µs
 *   Target (ASM):                       ~4,000–6,000 µs
 */
#include "jupiter.h"
#include "snes.h"   /* snes_mode7_render, snes_sin_lut */

#define SIN_MASK    255   /* still used for the camera-oscillation index below */

/* ---- Floor tile map ---- */
#define MAP_W       64
#define MAP_H       64
#define MAP_MASK    63
#define MAP_W_BITS  6

static uint8_t floor_map[MAP_H * MAP_W];

/* Synthwave palette:
 *   0 = deep black (cells)
 *   1 = magenta-pink grid line (along x)
 *   2 = cyan grid line (along y)
 *   3 = white-pink line crossings (brightest, where x+y lines meet)
 *   4 = dim pink (mid-row accent every 8 rows)
 */
static const uint32_t floor_lut[256] = {
    [0] = 0x00050008,   /* near-black background */
    [1] = 0x00FF2080,   /* hot magenta */
    [2] = 0x0020E0FF,   /* electric cyan */
    [3] = 0x00FFC0E0,   /* hot pink crossing */
    [4] = 0x00802050,   /* dim magenta accent */
};

static void build_map(void)
{
    /* Synthwave / OutRun grid: black floor, neon grid lines every cell
     * along both axes, brighter crossings. The mode 7 sampler scales the
     * grid down at the horizon, giving the iconic receding-line look.
     * 64×64 cells wraps cleanly — camera flies forever over the same
     * texture without seams. */
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t tile = 0;
            int on_x = (x % 4) == 0;   /* magenta line every 4 cells along x */
            int on_y = (y % 4) == 0;   /* cyan line every 4 cells along y */
            if (on_x && on_y)      tile = 3;
            else if (on_x)         tile = 1;
            else if (on_y)         tile = 2;
            else if ((y % 16) == 8 && (x & 1)) tile = 4;  /* mid-row dim accent */
            floor_map[y * MAP_W + x] = tile;
        }
    }
}

/* ---- Mode 7 rendering (now using library ASM scanline) ---- */
#define HORIZON     (LCD_H / 2)

static void render_sky(uint32_t fb_addr)
{
    /* Synthwave sky: black-purple gradient with a pixelated OutRun sun
     * sitting just above the horizon. Sun is a circle banded with
     * horizontal black "scan slits" (gets denser toward the bottom),
     * core is hot yellow fading out to magenta/red at the rim — the
     * 80s VHS-album-cover look. */
    const int sun_cx = LCD_W / 2;
    const int sun_cy = HORIZON - 16;  /* anchor sun center above horizon */
    const int sun_r  = 56;             /* radius */
    const int sun_r2 = sun_r * sun_r;

    for (uint32_t y = 0; y < HORIZON; y++) {
        /* Sky gradient: deep black at the top, deeper purple near the
         * horizon. Goes from #050008 (near-black) to #20084A (deep violet). */
        uint32_t t = (y * 256) / HORIZON;
        uint32_t sr = 0x05 + ((t * 0x1B) >> 8);
        uint32_t sg = 0x00 + ((t * 0x08) >> 8);
        uint32_t sb = 0x08 + ((t * 0x42) >> 8);
        uint32_t sky = (sr << 16) | (sg << 8) | sb;
        uint32_t *row = (uint32_t *)(fb_addr + y * LCD_W * 4);
        memset32_neon((uint32_t)row, sky, LCD_W * 4);

        /* Sun overlay. */
        int dy = (int)y - sun_cy;
        int dy2 = dy * dy;
        if (dy2 > sun_r2) continue;
        /* Horizontal extent of the sun on this scanline */
        int half_w = 0;
        { int v = sun_r2 - dy2;
          /* integer sqrt — small range, just walk it */
          while ((half_w + 1) * (half_w + 1) <= v) half_w++; }
        int x0 = sun_cx - half_w; if (x0 < 0) x0 = 0;
        int x1 = sun_cx + half_w; if (x1 > (int)LCD_W) x1 = LCD_W;

        /* Scan-slit pattern: every 4 rows, leave the bottom 1 row as
         * gradient (sun looks "sliced" the further down it goes). The
         * slits get thicker toward the bottom. */
        int slit_mod = 4;
        if (dy > sun_r / 3)      slit_mod = 3;
        if (dy > 2 * sun_r / 3)  slit_mod = 2;
        if ((dy + sun_r) % slit_mod == 0) continue;  /* leave gradient — slit */

        /* Sun color: center bright yellow → edge magenta-pink, vertical
         * gradient bottom red → top yellow. */
        int top_to_bot = (dy + sun_r) * 256 / (2 * sun_r);   /* 0..255 */
        uint32_t r = 0xFF;
        uint32_t g = 0xC0 - (top_to_bot * 0x80 >> 8);        /* dim toward bottom */
        uint32_t b = 0x20 + (top_to_bot * 0x60 >> 8);        /* more pink toward bottom */
        uint32_t sun = (r << 16) | (g << 8) | b;
        for (int x = x0; x < x1; x++) row[x] = sun;
    }
}

/* Floor rendering is now snes_mode7_render() from lib/snes.c. The
 * synthwave forward-flight camera math is plumbed via the m7 struct
 * at the call site (twist=0 = flat-camera straight flight). */

/* ---- Procedural sprites ---- */

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

/* Indexed version of the same enemy: 8-bit palette indices.
 * Palette index 0 = transparent. Same visual, 4x less memory. */
static uint8_t __attribute__((aligned(4))) enemy_idx[ENEMY_W * ENEMY_H];

/* Palette variants: same sprite, different colors.
 * Index 0 = transparent (alpha 0), 1–4 = the sprite colors. */
static uint32_t pal_fire[256];    /* red/orange — default */
static uint32_t pal_ice[256];     /* blue/cyan */
static uint32_t pal_poison[256];  /* green/purple */
static uint32_t pal_ghost[256];   /* white/translucent */

static void gen_enemy(void)
{
    /* Generate ARGB version (used by scaler) */
    for (int y = 0; y < ENEMY_H; y++)
        for (int x = 0; x < ENEMY_W; x++) {
            uint32_t px = 0x00000000;
            uint8_t idx = 0;
            int cx = x - 8, cy = y - 8;
            int d2 = cx*cx + cy*cy;
            if (d2 <= 42) { px = 0xFFCC2244; idx = 1; }
            if (d2 <= 20) { px = 0xFFFF4466; idx = 2; }
            if (cy >= -2 && cy <= 0 && (cx == -3 || cx == 3)) {
                if (cy == -1) { px = 0xFFFFFF00; idx = 3; }
                else          { px = 0xFFFF8800; idx = 4; }
            }
            if (cy <= -6 && cy >= -7 && (cx == 0 || cx == -4 || cx == 4)) {
                px = 0xFFCC2244; idx = 1;
            }
            enemy_data[y * ENEMY_W + x] = px;
            enemy_idx[y * ENEMY_W + x] = idx;
        }

    /* Fire palette (original colors) */
    pal_fire[0] = 0x00000000;  /* transparent */
    pal_fire[1] = 0xFFCC2244;  /* dark red */
    pal_fire[2] = 0xFFFF4466;  /* bright red-pink */
    pal_fire[3] = 0xFFFFFF00;  /* yellow eye */
    pal_fire[4] = 0xFFFF8800;  /* orange eye */

    /* Ice palette: swap reds→blues */
    pal_ice[0] = 0x00000000;
    pal_ice[1] = 0xFF2244CC;
    pal_ice[2] = 0xFF4466FF;
    pal_ice[3] = 0xFF00FFFF;
    pal_ice[4] = 0xFF0088FF;

    /* Poison palette: swap to greens */
    pal_poison[0] = 0x00000000;
    pal_poison[1] = 0xFF22CC44;
    pal_poison[2] = 0xFF44FF66;
    pal_poison[3] = 0xFFFFFF00;
    pal_poison[4] = 0xFF88FF00;

    /* Ghost palette: desaturated + translucent alpha */
    pal_ghost[0] = 0x00000000;
    pal_ghost[1] = 0x80AAAAAA;
    pal_ghost[2] = 0x80CCCCCC;
    pal_ghost[3] = 0xA0FFFFFF;
    pal_ghost[4] = 0x80DDDDDD;
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

/* ---- Shadow sprite: premultiplied alpha for blend testing ---- */
#define SHADOW_W 32
#define SHADOW_H 16
static uint32_t shadow_data[SHADOW_W * SHADOW_H];

static void gen_shadow(void)
{
    /* Elliptical shadow blob — premultiplied ARGB.
     * 50% black = alpha 0x80, RGB all 0 (premul of 0×anything = 0).
     * Edges fade to 0 alpha for smooth falloff. */
    for (int y = 0; y < SHADOW_H; y++)
        for (int x = 0; x < SHADOW_W; x++) {
            int cx = x - SHADOW_W/2;
            int cy = y - SHADOW_H/2;
            /* Ellipse: (cx/16)² + (cy/8)² ≤ 1 → cx²/256 + cy²/64 ≤ 1 */
            int d = (cx * cx * 64 + cy * cy * 256);
            int max_d = 256 * 64;  /* = 1.0 on the ellipse boundary */
            if (d < max_d) {
                /* Smooth falloff: alpha = 128 × (1 - d/max_d) */
                uint32_t a = (uint32_t)(128 * (max_d - d) / max_d);
                /* Premultiplied: RGB = 0 (shadow is black), A = alpha */
                shadow_data[y * SHADOW_W + x] = a << 24;
            } else {
                shadow_data[y * SHADOW_W + x] = 0x00000000;
            }
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
    uart_puts("\n\n=== Jupiter SDK — Full Pipeline (NEON ASM) ===\n");
    uart_puts("ASM inner loops:\n");
    uart_puts("  1. sprite_blit         → _sprite_row_keyed\n");
    uart_puts("  2. sprite_blit_flip    → _sprite_row_keyed_flip\n");
    uart_puts("  3. sprite_blit_blend   → _sprite_row_blend\n");
    uart_puts("  4. tiles_render_fast   → _tile_row_blast\n");
    uart_puts("  5. mode7_scanline      → mode7_neon.S\n");
    uart_puts("  6. sprite_blit_scaled  → _sprite_row_scaled\n");
    uart_puts("  7. sprite_blit_indexed → _sprite_row_indexed  [NEW]\n");
    uart_puts("  8. sprite_blit_rotscale→ _sprite_row_rotscale [NEW]\n\n");
    timer_init();

    build_map();
    gen_hero();
    gen_enemy();
    gen_star();
    gen_shadow();
    init_movers();

    memset32_neon(FB0_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    mmu_init();
    uart_puts("Display + MMU active. Entering NEON benchmark loop...\n\n");

    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;

    /* snes_sin/cos build lazily on first call. Grab once. */
    const int32_t *s_sin = snes_sin_lut();

    while (1) {
        uint32_t t0 = timer_read();

        /* ---- Background: sky + Mode 7 floor (NEON ASM via snes_mode7_render) ---- */
        render_sky(back_fb);
        snes_mode7_t m7 = {
            .cam_x       = s_sin[(frame * 2) & SIN_MASK] >> 5,   /* mild lateral wobble */
            .cam_y       = (int32_t)(frame * 80),                /* forward at high speed */
            .angle       = 0,                                     /* no rotation */
            .twist       = 0,                                     /* flat camera = straight flight */
            .horizon     = HORIZON,
            .space_z     = 8000,
            .map         = floor_map,
            .palette     = floor_lut,
            .map_w_bits  = 6,                                     /* log2(MAP_W=64) */
            .map_mask    = 63,
        };
        snes_mode7_render((uint32_t *)back_fb, (uint32_t *)0, LCD_W, LCD_H, &m7, 0);
        uint32_t t_bg = timer_read();

        /* ---- Sprites on overlay ---- */
        volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;
        memset32_neon(back_ovl, 0x00000000, LCD_W * LCD_H * 4);

        bounce(&hero_m, HERO_W, HERO_H);
        for (int i = 0; i < NUM_ENEMIES; i++) bounce(&enemies[i], ENEMY_W, ENEMY_H);
        for (int i = 0; i < NUM_STARS; i++) bounce(&stars[i], STAR_W, STAR_H);

        /* Shadow under hero: premultiplied alpha blend (ASM #3) */
        sprite_blit_blend(ovl, LCD_W, shadow_data, SHADOW_W, SHADOW_H,
                          hero_m.x - 4, hero_m.y + HERO_H - 8);

        /* Hero: alpha-key blit or flip (ASM #1 / #2) */
        if (hero_m.dx > 0)
            sprite_blit(ovl, LCD_W, hero_data, HERO_W, HERO_H, hero_m.x, hero_m.y);
        else
            sprite_blit_flip(ovl, LCD_W, hero_data, HERO_W, HERO_H, hero_m.x, hero_m.y);

        /* Enemies: mix of depth-scaled ARGB and indexed palette sprites.
         *
         * Even enemies: depth-scaled ARGB (ASM: _sprite_row_scaled)
         * Odd enemies:  indexed palette, each a different element (ASM: _sprite_row_indexed)
         *   - Palette swap = zero cost, one pointer change.
         *   - Same 256-byte indexed sprite, 4 different looks.
         */
        const uint32_t *palettes[4] = { pal_fire, pal_ice, pal_poison, pal_ghost };

        for (int i = 0; i < NUM_ENEMIES; i++) {
            int ey = enemies[i].y;
            if (ey < 0) ey = 0;
            if (ey >= (int)LCD_H) ey = (int)LCD_H - 1;

            /* Shadow under each */
            uint32_t dst_ew = ENEMY_W / 2 + (uint32_t)ey * (ENEMY_W * 3 / 2) / LCD_H;
            uint32_t dst_eh = ENEMY_H / 2 + (uint32_t)ey * (ENEMY_H * 3 / 2) / LCD_H;
            if (dst_ew < 4) dst_ew = 4;
            if (dst_eh < 4) dst_eh = 4;
            sprite_blit_blend(ovl, LCD_W, shadow_data, SHADOW_W, SHADOW_H,
                              enemies[i].x - (int)(dst_ew) / 2,
                              enemies[i].y + (int)dst_eh - 4);

            if (i & 1) {
                /* Odd: indexed palette sprite — original size, palette varies */
                sprite_blit_indexed(ovl, LCD_W,
                                    enemy_idx, ENEMY_W, ENEMY_H,
                                    palettes[(i >> 1) & 3],
                                    enemies[i].x, enemies[i].y);
            } else {
                /* Even: depth-scaled ARGB */
                sprite_blit_scaled(ovl, LCD_W, enemy_data, ENEMY_W, ENEMY_H,
                                   enemies[i].x, enemies[i].y,
                                   dst_ew, dst_eh);
            }
        }

        /* Stars: spinning via rotscale (ASM: _sprite_row_rotscale)
         *
         * Each star rotates at a different speed. The rotation angle
         * is derived from the frame counter. Scale pulses gently
         * between 0.8× and 1.5× for a "twinkle" effect.
         *
         * This is what the GBA's OAM rotation hardware did —
         * we're doing it in software at 1000× the clock speed. */
        for (int i = 0; i < NUM_STARS; i++) {
            uint8_t rot_angle = (uint8_t)(frame * (2 + i) + i * 37);
            /* Scale oscillates: 0.8 to 1.5 in 16.16 FP */
            uint32_t pulse = 0xCCCC + (uint32_t)((s_sin[(frame * 3 + i * 41) & SIN_MASK]
                                                    + (1 << 12)) >> 5);   /* sin LUT is Q12 */
            sprite_blit_rotscale(ovl, LCD_W, star_data, STAR_W, STAR_H,
                                 stars[i].x + STAR_W/2,
                                 stars[i].y + STAR_H/2,
                                 rot_angle, pulse);
        }

        uint32_t t_spr = timer_read();

        /* ---- Flush + swap BOTH layers ---- */
        dcache_clean_fb(back_fb);
        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);  /* OVL needs its own flush */
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
            uart_puts("us (budget=16667)\n");

            if (frame == 0) {
                uart_puts("\n--- C baselines (from PROGRESS.md) ---\n");
                uart_puts("  Mode 7 + 25 sprites (C):  12960 us\n");
                uart_puts("  Mode 7 floor (C, no cache): 15570 us\n");
                uart_puts("--- Now running NEON ASM ---\n\n");
            }
        }
        frame++;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            ;
    }
}
