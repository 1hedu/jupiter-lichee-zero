/*
 * Jupiter SDK — Sprite Blitting Demo
 *
 * Proves: alpha-key sprite blitting on UI0 overlay, clipping, flip,
 * multiple moving sprites over a scrolling tile background on VI0.
 *
 * Procedurally generates sprite data (no asset loading yet):
 *   - 24×24 "hero" sprite (orange/yellow humanoid shape)
 *   - 16×16 "enemy" sprites (red/purple, 8 of them)
 *   - 8×8 "star" particles (white dots, 16 of them)
 *
 * All sprites use alpha-key: 0x00000000 = transparent, 0xFFrrggbb = opaque.
 * The DE2 blender composites UI0 over VI0 in hardware.
 */
#include "jupiter.h"

/* ---- Tile background (same as fast_tiles) ---- */
#define MAP_W       64
#define MAP_H_FAR   17
#define MAP_H_NEAR  17
#define HORIZON     (LCD_H / 2)

static uint8_t sky_map[MAP_H_FAR * MAP_W];
static uint8_t ground_map[MAP_H_NEAR * MAP_W];

static const uint32_t sky_lut[256] = {
    [0] = 0x00102040, [1] = 0x00183058,
    [2] = 0x00203860, [3] = 0x00284068,
};
static const uint32_t ground_lut[256] = {
    [4] = 0x00206020, [5] = 0x00308030,
    [6] = 0x0040A040, [7] = 0x00604020,
};

static void build_maps(void)
{
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
}

/* ---- Procedural sprite generation ---- */

/* Hero: 24×24 humanoid silhouette */
#define HERO_W 24
#define HERO_H 24
static uint32_t hero_data[HERO_W * HERO_H];

static void gen_hero(void)
{
    for (int y = 0; y < HERO_H; y++) {
        for (int x = 0; x < HERO_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 12, cy = y - 12;

            /* Head: circle at top */
            if (cy >= -11 && cy <= -6) {
                int dx = cx, dy = cy + 8;
                if (dx*dx + dy*dy <= 12)
                    px = 0xFFFFCC66;  /* skin tone */
            }
            /* Eyes */
            if (cy == -9 && (cx == -2 || cx == 2))
                px = 0xFF202020;
            /* Body: rectangle */
            if (cy >= -5 && cy <= 3 && cx >= -4 && cx <= 4)
                px = 0xFFFF8800;  /* orange tunic */
            /* Belt */
            if (cy == 1 && cx >= -4 && cx <= 4)
                px = 0xFF804000;
            /* Arms */
            if (cy >= -4 && cy <= 0 && (cx == -5 || cx == 5))
                px = 0xFFFFCC66;
            /* Legs */
            if (cy >= 4 && cy <= 8 && (cx >= -3 && cx <= -1))
                px = 0xFF4444AA;  /* blue pants */
            if (cy >= 4 && cy <= 8 && (cx >= 1 && cx <= 3))
                px = 0xFF4444AA;
            /* Boots */
            if (cy >= 9 && cy <= 10 && ((cx >= -4 && cx <= -1) || (cx >= 1 && cx <= 4)))
                px = 0xFF663300;

            hero_data[y * HERO_W + x] = px;
        }
    }
}

/* Enemy: 16×16 floating skull-ish thing */
#define ENEMY_W 16
#define ENEMY_H 16
static uint32_t enemy_data[ENEMY_W * ENEMY_H];

static void gen_enemy(void)
{
    for (int y = 0; y < ENEMY_H; y++) {
        for (int x = 0; x < ENEMY_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 8, cy = y - 8;
            int d2 = cx*cx + cy*cy;

            /* Body: filled circle */
            if (d2 <= 42)
                px = 0xFFCC2244;  /* deep red */
            /* Inner glow */
            if (d2 <= 20)
                px = 0xFFFF4466;  /* lighter red */
            /* Eyes: two bright dots */
            if (cy >= -2 && cy <= 0) {
                if ((cx == -3 || cx == 3) && cy == -1)
                    px = 0xFFFFFF00;  /* yellow eyes */
                if ((cx == -3 || cx == 3) && cy == 0)
                    px = 0xFFFF8800;
            }
            /* Spikes: top */
            if (cy <= -6 && (cx == 0 || cx == -4 || cx == 4))
                if (cy >= -7)
                    px = 0xFFCC2244;

            enemy_data[y * ENEMY_W + x] = px;
        }
    }
}

/* Star: 8×8 sparkle */
#define STAR_W 8
#define STAR_H 8
static uint32_t star_data[STAR_W * STAR_H];

static void gen_star(void)
{
    for (int y = 0; y < STAR_H; y++) {
        for (int x = 0; x < STAR_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 4, cy = y - 4;

            /* Cross shape */
            if ((cx == 0 && cy >= -2 && cy <= 2) ||
                (cy == 0 && cx >= -2 && cx <= 2))
                px = 0xCCFFFFFF;  /* white, slightly transparent */

            /* Center bright */
            if (cx == 0 && cy == 0)
                px = 0xFFFFFFCC;  /* warm white */

            star_data[y * STAR_W + x] = px;
        }
    }
}

/* ---- Sprite instances ---- */
#define NUM_ENEMIES 8
#define NUM_STARS   16

typedef struct {
    int x, y, dx, dy;
} mover_t;

static mover_t hero;
static mover_t enemies[NUM_ENEMIES];
static mover_t stars[NUM_STARS];

static void init_movers(void)
{
    hero = (mover_t){ 200, 120, 2, 1 };

    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i] = (mover_t){
            50 + (i * 53) % 400,
            30 + (i * 37) % 200,
            ((i & 1) ? 1 : -1) * (1 + (i % 3)),
            ((i & 2) ? 1 : -1) * (1 + ((i+1) % 2))
        };
    }

    for (int i = 0; i < NUM_STARS; i++) {
        stars[i] = (mover_t){
            (i * 31) % (int)LCD_W,
            (i * 17) % (int)LCD_H,
            ((i & 1) ? 1 : -1),
            ((i & 2) ? 1 : -1)
        };
    }
}

static void bounce(mover_t *m, int w, int h)
{
    m->x += m->dx;
    m->y += m->dy;
    if (m->x < -w/2 || m->x + w/2 >= (int)LCD_W) { m->dx = -m->dx; m->x += m->dx; }
    if (m->y < -h/2 || m->y + h/2 >= (int)LCD_H) { m->dy = -m->dy; m->y += m->dy; }
}

/* ==== ENTRY ==== */
void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — Sprite Blitting Demo ===\n");
    timer_init();

    /* Generate assets */
    gen_hero();
    gen_enemy();
    gen_star();
    build_maps();
    init_movers();

    memset32_neon(FB0_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    mmu_init();
    uart_puts("Display + MMU active.\n\n");

    uint32_t back_fb   = FB1_ADDR,  front_fb   = FB0_ADDR;
    /* UI0 overlay double-buffered too — paint back, dcache flush,
     * video_set_overlay, swap. Previously this demo painted directly
     * to OVL_ADDR while DE2 was reading it, which produced tearing /
     * sprite trails / stale-pixel garbage when launched from the menu
     * (OVL_ADDR had menu UI on it at entry). */
    uint32_t back_ovl  = OVL1_ADDR, front_ovl  = OVL_ADDR;
    int scroll_far = 0, scroll_near = 0;
    uint32_t frame = 0;

    uart_puts("Entering sprite demo...\n\n");

    while (1) {
        uint32_t t0 = timer_read();

        /* ---- Background: parallax tiles on VI0 ---- */
        uint32_t *fb = (uint32_t *)back_fb;
        tiles_render_fast((volatile uint32_t *)fb, LCD_W, sky_map, MAP_W,
                          sky_lut, 0, MAP_H_FAR, scroll_far);
        tiles_render_fast((volatile uint32_t *)fb, LCD_W, ground_map, MAP_W,
                          ground_lut, HORIZON, MAP_H_NEAR, scroll_near);
        scroll_far  += 1;
        scroll_near += 3;

        uint32_t t_bg = timer_read();

        /* ---- Sprites on UI0 overlay ---- */

        /* Full-clear the back overlay each frame and re-blit. Cheaper
         * to memset 512 KB with NEON than to track per-sprite dirty
         * rects across two buffers, and eliminates all stale-pixel /
         * trail risk. */
        memset32_neon(back_ovl, 0x00000000, LCD_W * LCD_H * 4);
        volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;

        /* Move */
        bounce(&hero, HERO_W, HERO_H);
        for (int i = 0; i < NUM_ENEMIES; i++)
            bounce(&enemies[i], ENEMY_W, ENEMY_H);
        for (int i = 0; i < NUM_STARS; i++)
            bounce(&stars[i], STAR_W, STAR_H);

        /* Blit new positions */
        /* Hero: flip based on direction */
        if (hero.dx > 0)
            sprite_blit(ovl, LCD_W, hero_data, HERO_W, HERO_H, hero.x, hero.y);
        else
            sprite_blit_flip(ovl, LCD_W, hero_data, HERO_W, HERO_H, hero.x, hero.y);

        for (int i = 0; i < NUM_ENEMIES; i++) {
            if (enemies[i].dx > 0)
                sprite_blit(ovl, LCD_W, enemy_data, ENEMY_W, ENEMY_H,
                            enemies[i].x, enemies[i].y);
            else
                sprite_blit_flip(ovl, LCD_W, enemy_data, ENEMY_W, ENEMY_H,
                                 enemies[i].x, enemies[i].y);
        }

        for (int i = 0; i < NUM_STARS; i++)
            sprite_blit(ovl, LCD_W, star_data, STAR_W, STAR_H,
                        stars[i].x, stars[i].y);

        uint32_t t_spr = timer_read();

        /* Flush + swap BOTH layers */
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
            uart_puts(" bg=");
            uart_putdec(us_bg);
            uart_puts(" spr=");
            uart_putdec(us_spr);
            uart_puts(" total=");
            uart_putdec(us_tot);
            uart_puts("us");
            uart_puts(" (1hero+");
            uart_putdec(NUM_ENEMIES);
            uart_puts("enemy+");
            uart_putdec(NUM_STARS);
            uart_puts("star)\n");
        }
        frame++;

        /* Pace to 60fps */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            ;
    }
}
