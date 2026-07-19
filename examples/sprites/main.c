/*
 * Jupiter SDK — Sprite Blitting Demo
 *
 * Proves: alpha-key sprite blitting on UI0 overlay, clipping, flip,
 * multiple moving sprites over a scrolling tile background on VI0.
 *
 * Procedurally generates sprite data (no asset loading yet):
 *   - 24×24 "hero" sprite (outlined, cel-shaded adventurer)
 *   - 16×16 "voidling" enemies (8 of them)
 *   - 8×8 twinkling star particles (16, two frames)
 *
 * Sprites are authored as ASCII pixel maps (one char per pixel, legend
 * below) — readable, editable pixel art without an asset pipeline.
 *
 * All sprites use alpha-key: 0x00000000 = transparent, 0xFFrrggbb = opaque.
 * The DE2 blender composites UI0 over VI0 in hardware.
 */
#include "jupiter.h"

/* ---- Tile background: dusk ridge under a low sun ----
 * tiles_render_fast() is flat-color-per-8px-tile, so the map is used
 * as a chunky 60x34-cell canvas: dithered sky gradient, a mountain
 * silhouette, a sun disc, and a banded twilight meadow below. */
#define MAP_W       64
#define MAP_H_FAR   17
#define MAP_H_NEAR  17
#define HORIZON     (LCD_H / 2)

static uint8_t sky_map[MAP_H_FAR * MAP_W];
static uint8_t ground_map[MAP_H_NEAR * MAP_W];

/* Sky: 0-7 gradient (zenith → horizon glow), 8 ridge, 9 star, 10 sun */
static const uint32_t sky_lut[256] = {
    [0] = 0xFF0B1026, [1] = 0xFF141A3A,
    [2] = 0xFF232558, [3] = 0xFF3A2E6E,
    [4] = 0xFF5C3B7E, [5] = 0xFF8A4A86,
    [6] = 0xFFC75B79, [7] = 0xFFF07A5A,
    [8] = 0xFF191230,                      /* mountain silhouette   */
    [9] = 0xFF4A5590,                      /* faint star cell       */
    [10] = 0xFFFFD9A0,                     /* sun disc              */
    [11] = 0xFFF2A46B,                     /* sun halo              */
};
/* Ground: 16 lit edge, 17-20 meadow bands, 21 deep shadow, 22/23 blooms */
static const uint32_t ground_lut[256] = {
    [16] = 0xFF4A7A52, [17] = 0xFF2F6849,
    [18] = 0xFF26543F, [19] = 0xFF1E4034,
    [20] = 0xFF16302A, [21] = 0xFF102420,
    [22] = 0xFF7FE0C3, [23] = 0xFFC75B79,
};

/* Cheap deterministic cell hash for speckle placement */
static uint32_t cell_hash(uint32_t x, uint32_t y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void build_maps(void)
{
    /* Sky: vertical gradient with 1-row checker-dithered transitions.
     * Gradient index per row (17 rows): which band, and whether the row
     * blends with the next band. */
    static const uint8_t sky_row[MAP_H_FAR] =
        { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7 };

    for (uint32_t x = 0; x < MAP_W; x++) {
        /* Mountain ridge height (rows from bottom of sky): rolling,
         * with taller peaks on a regular beat */
        uint32_t ridge = 1 + ((cell_hash(x >> 1, 7) >> 4) % 3);
        if ((x & 15) == 6) ridge += 2;
        if ((x & 15) == 7 || (x & 15) == 5) ridge += 1;
        uint32_t ridge_top = MAP_H_FAR - ridge;      /* first ridge row */

        for (uint32_t y = 0; y < MAP_H_FAR; y++) {
            uint8_t c = sky_row[y];
            /* dither the band seams with a checker of the next shade */
            if (y + 1 < MAP_H_FAR && sky_row[y + 1] != c && ((x + y) & 1))
                c = sky_row[y + 1];

            /* sun: disc + one-cell halo ring, sitting on the ridge */
            int sx = (int)x - 45, sy = (int)y - 12;
            int d2 = sx * sx + sy * sy * 3;
            if (d2 <= 30) c = 11;
            if (d2 <= 14) c = 10;

            /* very sparse faint stars high in the sky */
            if (y < 6 && (cell_hash(x, y) % 61) == 0) c = 9;

            /* mountain silhouette wins over everything */
            if (y >= ridge_top) c = 8;

            sky_map[y * MAP_W + x] = c;
        }
    }

    /* Meadow: lit horizon edge, then bands falling into shadow, with
     * scattered glow-blooms. */
    static const uint8_t gnd_row[MAP_H_NEAR] =
        { 16, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 21, 21 };

    for (uint32_t y = 0; y < MAP_H_NEAR; y++)
        for (uint32_t x = 0; x < MAP_W; x++) {
            uint8_t c = gnd_row[y];
            if (y + 1 < MAP_H_NEAR && gnd_row[y + 1] != c && ((x + y) & 1))
                c = gnd_row[y + 1];
            /* blooms: sparse, cyan mostly, magenta sometimes */
            uint32_t h = cell_hash(x, y + 64);
            if (y > 1 && (h % 53) == 0)
                c = ((h >> 8) % 5 == 0) ? 23 : 22;
            ground_map[y * MAP_W + x] = c;
        }
}

/* ---- Sprites: ASCII pixel maps ----
 * One char per pixel. '.' = transparent; everything else looks up the
 * legend. Draw in the string, see it on screen — no asset pipeline. */

typedef struct { char ch; uint32_t argb; } pal_entry_t;

static void decode_sprite(const char *const *rows, int w, int h,
                          const pal_entry_t *pal, uint32_t *out)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            char ch = rows[y][x];
            uint32_t px = 0x00000000;
            for (const pal_entry_t *p = pal; p->ch; p++)
                if (p->ch == ch) { px = p->argb; break; }
            out[y * w + x] = px;
        }
}

/* Hero: 24×24 caped adventurer, 1px outline, 3-tone cel shading.
 * Faces right (sprite_blit_flip mirrors for leftward motion). */
#define HERO_W 24
#define HERO_H 24
static uint32_t hero_data[HERO_W * HERO_H];

static const pal_entry_t hero_pal[] = {
    { 'K', 0xFF141020 },  /* outline           */
    { 'S', 0xFFF0C896 },  /* skin              */
    { 's', 0xFFC89060 },  /* skin shade        */
    { 'H', 0xFF7A3B2E },  /* hair              */
    { 'T', 0xFF2FA5A0 },  /* teal tunic        */
    { 't', 0xFF1E6E6C },  /* tunic shade       */
    { 'C', 0xFFC7327A },  /* magenta cape      */
    { 'c', 0xFF8A1F56 },  /* cape shade        */
    { 'G', 0xFFF2C14E },  /* gold trim         */
    { 'B', 0xFF4A3A5E },  /* boots/legs        */
    { 'b', 0xFF322645 },  /* boots shade       */
    { 'W', 0xFFFFF7E8 },  /* eye glint         */
    { 0, 0 }
};

static const char *const hero_map[HERO_H] = {
    "........KKKKKK..........",
    ".......KHHHHHHK.........",
    "......KHHHHHHHHK........",
    "......KHHSSSSSHK........",
    "......KSSSSSSSSK........",
    "......KSKWSSKWSK........",
    "......KSSSSSSSSK........",
    "......KsSSKKSSsK........",
    ".......KsSSSSsK.........",
    "....KKKKTTTTTTKKK.......",
    "...KCCKTTGGGGTTKsK......",
    "..KCCcKTTTTTTTTKSK......",
    ".KCCcKTTtTTTTtTTKK......",
    ".KCccKTTtTTTTtTTK.......",
    ".KCccKTGGGGGGGGTK.......",
    ".KCccKtTTTTTTTTtK.......",
    ".KCccKKtTTTTTTtKK.......",
    ".KCcccKKKKKKKKKK........",
    ".KCccccKBBKKBBK.........",
    "..KccccKBBKKBBK.........",
    "...KKKKKBbKKBbK.........",
    "......KBBbKKBBbK........",
    "......KbbbKKbbbK........",
    ".......KKK..KKK.........",
};

static void gen_hero(void)
{ decode_sprite(hero_map, HERO_W, HERO_H, hero_pal, hero_data); }

/* Enemy: 16×16 "voidling" — horned wisp, glowing core, cyan eyes */
#define ENEMY_W 16
#define ENEMY_H 16
static uint32_t enemy_data[ENEMY_W * ENEMY_H];

static const pal_entry_t enemy_pal[] = {
    { 'K', 0xFF140E1E },  /* outline      */
    { 'V', 0xFF6E2A8A },  /* violet body  */
    { 'v', 0xFF4A1C60 },  /* body shade   */
    { 'L', 0xFF9A4AC0 },  /* body light   */
    { 'O', 0xFFC77ADF },  /* core glow    */
    { 'E', 0xFF7FF3E8 },  /* cyan eye     */
    { 'M', 0xFF2A1038 },  /* mouth        */
    { 0, 0 }
};

static const char *const enemy_map[ENEMY_H] = {
    "..K..........K..",
    ".KVK........KVK.",
    ".KVVK..KK..KVVK.",
    "..KVVKKLLKKVVK..",
    "..KVLLLLLLLLVK..",
    ".KVLLOOLLOOLLVK.",
    ".KVLOEOLLOEOLVK.",
    ".KVLLOOLLOOLLVK.",
    "KVVLLLLLLLLLLVVK",
    "KVVLLMMMMMMLLVVK",
    "KVvVLLLLLLLLVvVK",
    ".KvVVLLLLLLVVvK.",
    "..KvVVvVVvVVvK..",
    "...KvK.KvK.KvK..",
    "....K...K...K...",
    "................",
};

static void gen_enemy(void)
{ decode_sprite(enemy_map, ENEMY_W, ENEMY_H, enemy_pal, enemy_data); }

/* Star: 8×8 sparkle, two twinkle frames (bright + dim) */
#define STAR_W 8
#define STAR_H 8
static uint32_t star_data[STAR_W * STAR_H];
static uint32_t star_dim_data[STAR_W * STAR_H];

static const pal_entry_t star_pal[] = {
    { 'W', 0xFFFFFFFF },
    { 'w', 0xCCBFE8FF },
    { 'o', 0x668FB8E8 },
    { 0, 0 }
};

static const char *const star_map[STAR_H] = {
    "...w....",
    "...w....",
    ".o.W.o..",
    "wwWWWww.",
    ".o.W.o..",
    "...w....",
    "...w....",
    "........",
};
static const char *const star_dim_map[STAR_H] = {
    "........",
    "...o....",
    "........",
    ".o.W.o..",
    "........",
    "...o....",
    "........",
    "........",
};

static void gen_star(void)
{
    decode_sprite(star_map, STAR_W, STAR_H, star_pal, star_data);
    decode_sprite(star_dim_map, STAR_W, STAR_H, star_pal, star_dim_data);
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

        for (int i = 0; i < NUM_STARS; i++) {
            /* twinkle: each star alternates bright/dim on its own beat */
            const uint32_t *img =
                (((frame >> 3) + i) & 3) ? star_data : star_dim_data;
            sprite_blit(ovl, LCD_W, img, STAR_W, STAR_H,
                        stars[i].x, stars[i].y);
        }

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
