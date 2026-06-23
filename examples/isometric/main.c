/*
 * Jupiter SDK — 2.5D Isometric Diorama
 *
 * The thesis demo. This is Breath of Fire IV's rendering philosophy
 * running on bare metal: hand-drawn 2D sprites placed in a 3D world
 * with a rotating camera. Flat art breathing inside genuine depth.
 *
 * Ground plane: isometric diamond tiles on VI0, camera-rotated.
 * Sprites: depth-sorted and depth-scaled on UI0.
 * Camera: slowly orbits the scene center.
 *
 * Exercises every SDK capability in one cohesive scene:
 *   - memset32_neon      (overlay clear)
 *   - memcpy_neon        (row operations)
 *   - sprite_blit        (hero, alpha-key)
 *   - sprite_blit_flip   (hero facing)
 *   - sprite_blit_blend  (shadows under sprites)
 *   - sprite_blit_scaled (depth-scaled sprites)
 *   - sprite_blit_indexed(palette-swapped NPCs)
 *   - sprite_blit_rotscale(spinning pickups)
 *
 * This is what the VHC Manifesto describes. This is the road not taken.
 */
#include "jupiter.h"

/* ================================================================
 *  FIXED-POINT AND TRIG
 * ================================================================ */

#define FP       12                 /* 20.12 fixed point for world coords */
#define FP_ONE   (1 << FP)
#define FP_HALF  (1 << (FP - 1))

#define SIN_N    256
#define SIN_MASK 255
static int32_t sin_lut[SIN_N];
static int32_t cos_lut[SIN_N];

static void init_sincos(void)
{
    for (int i = 0; i < SIN_N; i++) {
        int j = i & 127;
        int32_t val = (4 * j * (128 - j)) >> (14 - FP);
        if (i >= 128) val = -val;
        sin_lut[i] = val;
    }
    for (int i = 0; i < SIN_N; i++)
        cos_lut[i] = sin_lut[(i + 64) & SIN_MASK];
}

/* ================================================================
 *  WORLD MAP — isometric tile grid
 * ================================================================ */

#define MAP_W       32
#define MAP_H       32
#define MAP_MASK    31              /* AND mask for power-of-2 wrapping */
#define MAP_W_BITS  5              /* log2(MAP_W) for shift indexing */

/* Tile types */
#define T_GRASS   0
#define T_STONE   1
#define T_WATER   2
#define T_PATH    3
#define T_DARK    4
#define NUM_TILES 5

static uint8_t world_map[MAP_H][MAP_W];

/* Interleaved color LUT: tile_colors[tile * 2 + ((tx^ty) & 1)]
 * Eliminates the checkerboard branch in the inner loop.
 * Entry 0 = even checker, entry 1 = odd checker. */
static uint32_t tile_colors[NUM_TILES * 2];

static void init_tile_colors(void)
{
    /* [even, odd] pairs per tile type */
    tile_colors[T_GRASS * 2 + 0] = 0xFF2D5A1E;
    tile_colors[T_GRASS * 2 + 1] = 0xFF3A7028;
    tile_colors[T_STONE * 2 + 0] = 0xFF8A8078;
    tile_colors[T_STONE * 2 + 1] = 0xFF9A9088;
    tile_colors[T_WATER * 2 + 0] = 0xFF1A3A6A;
    tile_colors[T_WATER * 2 + 1] = 0xFF2A4A7A;
    tile_colors[T_PATH  * 2 + 0] = 0xFF6A5A3A;
    tile_colors[T_PATH  * 2 + 1] = 0xFF7A6A4A;
    tile_colors[T_DARK  * 2 + 0] = 0xFF000000;
    tile_colors[T_DARK  * 2 + 1] = 0xFF000000;
}

static void build_map(void)
{
    /* Fill entire 32×32 with dark border */
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            world_map[y][x] = T_DARK;

    /* 16×16 playable area centered in the 32×32 map (offset 8,8) */
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            world_map[y + 8][x + 8] = T_GRASS;

    /* Stone plaza: 8×8 in the center */
    for (int y = 4; y < 12; y++)
        for (int x = 4; x < 12; x++)
            world_map[y + 8][x + 8] = T_STONE;

    /* Water feature: 4×4 in the very center */
    for (int y = 6; y < 10; y++)
        for (int x = 6; x < 10; x++)
            world_map[y + 8][x + 8] = T_WATER;

    /* Paths leading outward from plaza edges */
    for (int i = 0; i < 4; i++) {
        world_map[i + 8][7 + 8] = T_PATH; world_map[i + 8][8 + 8] = T_PATH;
        world_map[12 + i + 8][7 + 8] = T_PATH; world_map[12 + i + 8][8 + 8] = T_PATH;
        world_map[7 + 8][i + 8] = T_PATH; world_map[8 + 8][i + 8] = T_PATH;
        world_map[7 + 8][12 + i + 8] = T_PATH; world_map[8 + 8][12 + i + 8] = T_PATH;
    }
}

/* ================================================================
 *  CAMERA + PROJECTION
 * ================================================================ */

/* Camera orbits around map center at a fixed altitude.
 * The isometric projection:
 *   1. Rotate world point by camera angle
 *   2. Project: sx = rx,  sy = rz/2 - height
 *   3. Scale to screen pixels
 */

#define CAM_DIST     0               /* camera offset from center (0 = orbits center) */
#define ISO_SCALE    20              /* pixels per world unit on screen */
#define MAP_CENTER_X (MAP_W * FP_ONE / 2)
#define MAP_CENTER_Y (MAP_H * FP_ONE / 2)
#define SCREEN_CX    (LCD_W / 2)
#define SCREEN_CY    (LCD_H / 2 + 20) /* slightly below center for ground bias */

static uint8_t cam_angle;           /* 0–255 = 0°–360° */

/* World→screen projection. Returns screen (sx, sy) and depth for sorting. */
typedef struct {
    int sx, sy;                     /* screen pixel coordinates */
    int32_t depth;                  /* for sorting (higher = further back) */
} proj_t;

static proj_t project(int32_t wx, int32_t wz, int32_t wy)
{
    /* Center world around map center */
    int32_t dx = wx - MAP_CENTER_X;
    int32_t dz = wz - MAP_CENTER_Y;

    /* Rotate by camera angle */
    int32_t ca = cos_lut[cam_angle];
    int32_t sa = sin_lut[cam_angle];
    int32_t rx = (dx * ca - dz * sa) >> FP;
    int32_t rz = (dx * sa + dz * ca) >> FP;

    /* Isometric projection: x maps to screen x, z maps to screen y (halved).
     * Height (wy) lifts sprites upward on screen. */
    proj_t p;
    p.sx = SCREEN_CX + (int)((rx * ISO_SCALE) >> FP);
    p.sy = SCREEN_CY + (int)((rz * ISO_SCALE / 2) >> FP) - (int)((wy * ISO_SCALE) >> FP);
    p.depth = rz;  /* depth for sorting: positive = further from camera */
    return p;
}


/* ================================================================
 *  ISOMETRIC GROUND RENDERER — scanline affine sampler
 * ================================================================
 *
 * The correct approach: for each screen pixel, inverse-project
 * through the camera transform to find the world tile underneath.
 * The tiles stay still. The sampling rotates. No gaps, no seams.
 *
 * This is how the SNES did Mode 7 — per-pixel affine sampling
 * of a tile map. We're doing the same thing but with an isometric
 * (top-down oblique) camera instead of a perspective floor.
 *
 * Math:
 *   Screen → camera space:
 *     rx = (sx - center_x) / SCALE
 *     rz = (sy - center_y) * 2 / SCALE    (2x for isometric Y)
 *
 *   Camera → world (inverse rotation by camera angle):
 *     wx = rx * cos(a) + rz * sin(a) + map_center
 *     wz = -rx * sin(a) + rz * cos(a) + map_center
 *
 *   Per-pixel UV steps precomputed, then step across each scanline.
 */

/* Half-res: 240×136, doubled to 480×272 */
#define ISO_W   (LCD_W / 2)
#define ISO_H   (LCD_H / 2)

static void render_ground(uint32_t fb_addr)
{
    uint32_t *fb = (uint32_t *)fb_addr;

    int32_t ca = cos_lut[cam_angle];
    int32_t sa = sin_lut[cam_angle];

    int32_t du_x =  ca * 2 / ISO_SCALE;
    int32_t dv_x = -sa * 2 / ISO_SCALE;
    int32_t du_y = sa * 4 / ISO_SCALE;
    int32_t dv_y = ca * 4 / ISO_SCALE;

    int32_t px = -(int32_t)SCREEN_CX;
    int32_t py = -(int32_t)SCREEN_CY;
    int32_t rx_fp = px * (int32_t)FP_ONE / ISO_SCALE;
    int32_t rz_fp = py * 2 * (int32_t)FP_ONE / ISO_SCALE;
    int32_t u_row = ((rx_fp * ca + rz_fp * sa) >> FP) + MAP_CENTER_X;
    int32_t v_row = ((-rx_fp * sa + rz_fp * ca) >> FP) + MAP_CENTER_Y;

    for (uint32_t sy = 0; sy < ISO_H; sy++) {
        uint32_t *row = fb + (sy * 2) * LCD_W;

        iso_scanline(row, (const uint8_t *)world_map,
                     tile_colors, u_row, v_row, du_x, dv_x, ISO_W);

        memcpy_neon(row + LCD_W, row, LCD_W * 4);

        u_row += du_y;
        v_row += dv_y;
    }
}


/* ================================================================
 *  SPRITE DATA
 * ================================================================ */

/* Hero: 24×24 ARGB (same procedural hero as other demos) */
#define HERO_W 24
#define HERO_H 24
static uint32_t hero_data[HERO_W * HERO_H];

static void gen_hero(void)
{
    for (int y = 0; y < HERO_H; y++)
        for (int x = 0; x < HERO_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 12, cy = y - 12;
            if (cy >= -11 && cy <= -6) {
                int ddx = cx, ddy = cy + 8;
                if (ddx*ddx + ddy*ddy <= 12) px = 0xFFFFCC66;
            }
            if (cy == -9 && (cx == -2 || cx == 2)) px = 0xFF202020;
            if (cy >= -5 && cy <= 3 && cx >= -4 && cx <= 4) px = 0xFFFF8800;
            if (cy == 1 && cx >= -4 && cx <= 4) px = 0xFF804000;
            if (cy >= -4 && cy <= 0 && (cx == -5 || cx == 5)) px = 0xFFFFCC66;
            if (cy >= 4 && cy <= 8 && cx >= -3 && cx <= -1) px = 0xFF4444AA;
            if (cy >= 4 && cy <= 8 && cx >= 1 && cx <= 3) px = 0xFF4444AA;
            if (cy >= 9 && cy <= 10 && ((cx >= -4 && cx <= -1) || (cx >= 1 && cx <= 4)))
                px = 0xFF663300;
            hero_data[y * HERO_W + x] = px;
        }
}

/* NPC: 16×16 indexed palette sprite (4 elemental variants) */
#define NPC_W 16
#define NPC_H 16
static uint8_t __attribute__((aligned(4))) npc_idx[NPC_W * NPC_H];

static uint32_t pal_fire[256];
static uint32_t pal_ice[256];
static uint32_t pal_earth[256];
static uint32_t pal_wind[256];

static void gen_npc(void)
{
    for (int y = 0; y < NPC_H; y++)
        for (int x = 0; x < NPC_W; x++) {
            uint8_t idx = 0;
            int cx = x - 8, cy = y - 8;
            int d2 = cx*cx + cy*cy;
            /* Body */
            if (d2 <= 42) idx = 1;
            if (d2 <= 20) idx = 2;
            /* Eyes */
            if (cy >= -2 && cy <= 0 && (cx == -3 || cx == 3))
                idx = (cy == -1) ? 3 : 4;
            /* Horn/antennae */
            if (cy <= -6 && cy >= -7 && (cx == 0 || cx == -4 || cx == 4))
                idx = 1;
            npc_idx[y * NPC_W + x] = idx;
        }

    /* 0=transparent, 1=body dark, 2=body light, 3=eye bright, 4=eye dim */
    pal_fire[0]=0x00000000; pal_fire[1]=0xFFCC2244; pal_fire[2]=0xFFFF4466;
    pal_fire[3]=0xFFFFFF00; pal_fire[4]=0xFFFF8800;

    pal_ice[0]=0x00000000; pal_ice[1]=0xFF2244CC; pal_ice[2]=0xFF4466FF;
    pal_ice[3]=0xFF00FFFF; pal_ice[4]=0xFF0088FF;

    pal_earth[0]=0x00000000; pal_earth[1]=0xFF22CC44; pal_earth[2]=0xFF44FF66;
    pal_earth[3]=0xFFFFFF00; pal_earth[4]=0xFF88FF00;

    pal_wind[0]=0x00000000; pal_wind[1]=0xFFAA88CC; pal_wind[2]=0xFFCCAAFF;
    pal_wind[3]=0xFFFFFFFF; pal_wind[4]=0xFFDDCCEE;
}

/* Pickup item: 12×12 ARGB for rotscale spinning */
#define ITEM_W 12
#define ITEM_H 12
static uint32_t item_data[ITEM_W * ITEM_H];

static void gen_item(void)
{
    for (int y = 0; y < ITEM_H; y++)
        for (int x = 0; x < ITEM_W; x++) {
            uint32_t px = 0x00000000;
            int cx = x - 6, cy = y - 6;
            /* Diamond shape */
            int d = (cx < 0 ? -cx : cx) + (cy < 0 ? -cy : cy);
            if (d <= 4) px = 0xFFFFDD44;
            if (d <= 2) px = 0xFFFFFF88;
            if (d == 0) px = 0xFFFFFFCC;
            item_data[y * ITEM_W + x] = px;
        }
}

/* Shadow: small ellipse for all sprites */
#define SHADOW_W 24
#define SHADOW_H 8
static uint32_t shadow_data[SHADOW_W * SHADOW_H];

static void gen_shadow(void)
{
    for (int y = 0; y < SHADOW_H; y++)
        for (int x = 0; x < SHADOW_W; x++) {
            int cx = x - SHADOW_W / 2;
            int cy = y - SHADOW_H / 2;
            int d = cx * cx * 4 + cy * cy * 36;
            int max_d = SHADOW_W * SHADOW_W / 4 * 4;
            if (d < max_d) {
                uint32_t a = (uint32_t)(100 * (max_d - d) / max_d);
                shadow_data[y * SHADOW_W + x] = a << 24;
            } else {
                shadow_data[y * SHADOW_W + x] = 0x00000000;
            }
        }
}


/* ================================================================
 *  SCENE OBJECTS — world-space positions
 * ================================================================ */

#define NUM_NPCS   6
#define NUM_ITEMS  4

typedef struct {
    int32_t wx, wz;                /* world position (FP 20.12) */
    int32_t wy;                    /* height (FP) */
    uint8_t type;                  /* 0=hero, 1=npc, 2=item */
    uint8_t variant;               /* palette / item type */
    /* Computed per frame: */
    int sx, sy;
    int32_t depth;
} scene_obj_t;

#define MAX_OBJS (1 + NUM_NPCS + NUM_ITEMS)
static scene_obj_t objects[MAX_OBJS];
static scene_obj_t *sorted[MAX_OBJS];
static int num_objects;

static void init_scene(void)
{
    num_objects = 0;

    /* Hero: center of the plaza (offset +8 for 32×32 map) */
    objects[num_objects++] = (scene_obj_t){
        .wx = 16 * FP_ONE, .wz = 15 * FP_ONE, .wy = 0,
        .type = 0, .variant = 0
    };

    /* NPCs: stationed around the plaza, each a different element */
    int32_t npc_pos[][2] = {
        {13 * FP_ONE,13 * FP_ONE },   /* top-left of plaza */
        {19 * FP_ONE,13 * FP_ONE },   /* top-right */
        {13 * FP_ONE,19 * FP_ONE },   /* bottom-left */
        {19 * FP_ONE,19 * FP_ONE },   /* bottom-right */
        {16 * FP_ONE,12 * FP_ONE },   /* north path */
        {16 * FP_ONE,20 * FP_ONE },   /* south path */
    };
    for (int i = 0; i < NUM_NPCS; i++) {
        objects[num_objects++] = (scene_obj_t){
            .wx = npc_pos[i][0], .wz = npc_pos[i][1], .wy = 0,
            .type = 1, .variant = (uint8_t)(i & 3)
        };
    }

    /* Spinning pickup items: floating above the water */
    int32_t item_pos[][2] = {
        {15 * FP_ONE,15 * FP_ONE },
        {17 * FP_ONE,15 * FP_ONE },
        {15 * FP_ONE,17 * FP_ONE },
        {17 * FP_ONE,17 * FP_ONE },
    };
    for (int i = 0; i < NUM_ITEMS; i++) {
        objects[num_objects++] = (scene_obj_t){
            .wx = item_pos[i][0], .wz = item_pos[i][1],
            .wy = FP_ONE + FP_HALF,  /* floating above ground */
            .type = 2, .variant = (uint8_t)i
        };
    }
}


/* ================================================================
 *  MAIN LOOP
 * ================================================================ */

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — 2.5D Isometric Diorama ===\n");
    uart_puts("The thesis demo. BoF4-style 2D sprites in 3D space.\n");
    uart_puts("Camera orbits. Sprites depth-sort. Every frame, bare metal.\n\n");
    timer_init();

    /* MMU must be enabled BEFORE any array initialization.
     * With MMU off, all memory is Device type. Device memory faults
     * on unaligned access regardless of SCTLR.A. GCC optimizes byte
     * array fills into word stores at unaligned offsets (e.g.
     * world_map[6][6] = offset 102, not word-aligned). Once the MMU
     * maps DRAM as Normal/Cached, unaligned access works. */
    mmu_init();

    init_sincos();
    build_map();
    init_tile_colors();
    gen_hero();
    gen_npc();
    gen_item();
    gen_shadow();
    init_scene();

    memset32_neon(FB0_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    uart_puts("Display + MMU active. Orbiting...\n\n");

    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;

    const uint32_t *palettes[] = { pal_fire, pal_ice, pal_earth, pal_wind };

    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();

        /* ---- Camera: slow orbit ---- */
        cam_angle = (uint8_t)(frame >> 1);  /* full rotation every 512 frames (~8.5s) */

        /* ---- Animate items: bob up and down ---- */
        for (int i = 0; i < num_objects; i++) {
            if (objects[i].type == 2) {
                int bob = sin_lut[(frame * 4 + objects[i].variant * 64) & SIN_MASK];
                objects[i].wy = FP_ONE + FP_HALF + (bob >> 4);
            }
        }

        /* ---- Project all objects ---- */
        for (int i = 0; i < num_objects; i++) {
            proj_t p = project(objects[i].wx, objects[i].wz, objects[i].wy);
            objects[i].sx = p.sx;
            objects[i].sy = p.sy;
            objects[i].depth = p.depth;
            sorted[i] = &objects[i];
        }

        /* ---- Depth sort: back-to-front (insertion sort, N≤11) ---- */
        for (int i = 1; i < num_objects; i++) {
            scene_obj_t *tmp = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j]->depth < tmp->depth) {
                sorted[j + 1] = sorted[j];
                j--;
            }
            sorted[j + 1] = tmp;
        }

        /* ---- Render ground (VI0) ---- */
        render_ground(back_fb);
        dcache_clean_fb(back_fb);
        uint32_t t_ground = timer_read();

        /* ---- Render sprites (UI0) ---- */
        volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;
        memset32_neon(back_ovl, 0x00000000, LCD_W * LCD_H * 4);

        /* ==== PASS 1: All shadows (back-to-front) ====
         * Shadows always render under all sprites. */
        for (int i = 0; i < num_objects; i++) {
            scene_obj_t *obj = sorted[i];
            if (obj->type == 0 || obj->type == 1) {
                sprite_blit_blend(ovl, LCD_W, shadow_data, SHADOW_W, SHADOW_H,
                                  obj->sx - SHADOW_W/2, obj->sy + 2);
            }
        }

        /* ==== PASS 2: Ground sprites — hero + NPCs (back-to-front) ==== */
        for (int i = 0; i < num_objects; i++) {
            scene_obj_t *obj = sorted[i];
            int32_t depth_norm = obj->depth >> 8;
            int scale_pct = 100 - depth_norm / 3;
            if (scale_pct < 40) scale_pct = 40;
            if (scale_pct > 180) scale_pct = 180;

            if (obj->type == 0) {
                uint32_t dw = (uint32_t)(HERO_W * scale_pct / 100);
                uint32_t dh = (uint32_t)(HERO_H * scale_pct / 100);
                if (dw < 4) dw = 4;
                if (dh < 4) dh = 4;
                sprite_blit_scaled(ovl, LCD_W, hero_data, HERO_W, HERO_H,
                                   obj->sx - (int)dw/2, obj->sy - (int)dh + (int)dh/4,
                                   dw, dh);
            } else if (obj->type == 1) {
                sprite_blit_indexed(ovl, LCD_W, npc_idx, NPC_W, NPC_H,
                                    palettes[obj->variant],
                                    obj->sx - (int)NPC_W/2,
                                    obj->sy - (int)NPC_H + 4);
            }
        }

        /* ==== PASS 3: Items — always on top (back-to-front) ==== */
        for (int i = 0; i < num_objects; i++) {
            scene_obj_t *obj = sorted[i];
            if (obj->type == 2) {
                int32_t depth_norm = obj->depth >> 8;
                int scale_pct = 100 - depth_norm / 3;
                if (scale_pct < 40) scale_pct = 40;
                if (scale_pct > 180) scale_pct = 180;
                uint8_t rot = (uint8_t)(frame * 5 + obj->variant * 64);
                uint32_t sc = 0x10000 + (uint32_t)((scale_pct - 100) * 655);
                if (sc < 0x6000) sc = 0x6000;
                sprite_blit_rotscale(ovl, LCD_W, item_data, ITEM_W, ITEM_H,
                                     obj->sx, obj->sy, rot, sc);
            }
        }

        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);
        uint32_t t_sprites = timer_read();

        /* ---- Swap both layers ---- */
        video_swap(back_fb);
        video_set_overlay(back_ovl);

        uint32_t tmp;
        tmp = back_fb; back_fb = front_fb; front_fb = tmp;
        tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        uint32_t us_gnd = ticks_to_us(timer_elapsed(t0, t_ground));
        uint32_t us_spr = ticks_to_us(timer_elapsed(t_ground, t_sprites));
        uint32_t us_tot = ticks_to_us(timer_elapsed(t0, timer_read()));

        if ((frame % 120) == 0) {
            uart_puts("f=");
            uart_putdec(frame);
            uart_puts(" gnd=");
            uart_putdec(us_gnd);
            uart_puts(" spr=");
            uart_putdec(us_spr);
            uart_puts(" total=");
            uart_putdec(us_tot);
            uart_puts("us\n");
        }
        frame++;

        /* Frame pace to 60fps */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            ;
    }
}
