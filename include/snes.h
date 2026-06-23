/*
 * Jupiter SDK — SNES-style background renderer
 *
 * Supports all 8 SNES BG modes (0–7). Mode 7 uses existing
 * mode7_scanline ASM. Modes 0–6 use software tile compositing.
 *
 * Tile pixel formats:
 *   2bpp: 4px/byte  — 16 bytes/tile  — 4 colors/palette
 *   4bpp: 2px/byte  — 32 bytes/tile  — 16 colors/palette
 *   8bpp: 1px/byte  — 64 bytes/tile  — 256 colors/palette
 *
 * Tilemap entry (16-bit):
 *   [9:0]   tile index (0–1023)
 *   [12:10] palette number (0–7)
 *   [13]    priority (0=low, 1=high)
 *   [14]    flip X
 *   [15]    flip Y
 */
#ifndef SNES_H
#define SNES_H

#include <stdint.h>

/* Native SNES resolution. Used when authentic=1. */
#define SNES_NATIVE_W 256
#define SNES_NATIVE_H 224

/* Tilemap entry macros */
#define SNES_TILE(idx)        ((idx) & 0x3FF)
#define SNES_PAL(p)           (((p) & 7) << 10)
#define SNES_PRIO             (1 << 13)
#define SNES_FLIPX            (1 << 14)
#define SNES_FLIPY            (1 << 15)

#define SNES_ENTRY(idx, pal, pri, fx, fy) \
    (SNES_TILE(idx) | SNES_PAL(pal) | ((pri) ? SNES_PRIO : 0) | \
     ((fx) ? SNES_FLIPX : 0) | ((fy) ? SNES_FLIPY : 0))

#define SNES_GET_TILE(e)   ((e) & 0x3FF)
#define SNES_GET_PAL(e)    (((e) >> 10) & 7)
#define SNES_GET_PRIO(e)   (((e) >> 13) & 1)
#define SNES_GET_FLIPX(e)  (((e) >> 14) & 1)
#define SNES_GET_FLIPY(e)  (((e) >> 15) & 1)

/* Background layer descriptor */
typedef struct {
    const uint8_t  *tiles;       /* tile pixel data (packed) */
    const uint16_t *map;         /* tilemap: map_w × map_h entries */
    const uint32_t *palette;     /* ARGB8888 colors */
    int32_t scroll_x, scroll_y;
    uint16_t map_w, map_h;       /* must be power of 2 */
    uint8_t bpp;                 /* 2, 4, or 8 */
    uint8_t enabled;
} snes_bg_t;

/* Per-tile-column offset table (Modes 2, 4, 6).
 * One entry per SCREEN tile column (fb_w/8 entries).
 * Each entry is a pixel offset added to the BG's scroll
 * for that 8-pixel column, creating column-independent scrolling.
 * On real SNES, BG3's tilemap doubles as this table. */
typedef struct {
    const int16_t *col_offset;   /* array of offsets, one per screen tile column */
    uint8_t vertical;            /* 0 = offsets apply to scroll_x, 1 = scroll_y */
} snes_tile_offset_t;

/* === Mode compositors — dual output (VI0 + UI0) ===
 *
 * Bottom BG → fb  (VI0): opaque, filled with backdrop where empty
 * Upper BGs → ovl (UI0): alpha-keyed (0 where empty), hardware-blended
 *
 * Each mode takes `int authentic`:
 *   0 = full-screen render covering (fb_w × fb_h)
 *   1 = centered SNES_NATIVE_W × SNES_NATIVE_H render with pillarbox.
 *       Caller must pre-clear both framebuffers to black once — the
 *       renderer never touches the pillarbox in authentic mode.
 * Scroll coords are relative to the rendered rect's top-left. */

/* Mode 0: 4 BGs, all 2bpp */
void snes_mode0_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       const snes_bg_t *bg3, const snes_bg_t *bg4,
                       int authentic);

/* Mode 1: BG1(4bpp) + BG2(4bpp) + BG3(2bpp) */
void snes_mode1_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       const snes_bg_t *bg3,
                       int authentic);

/* Mode 2: BG1(4bpp) + BG2(4bpp), per-tile column offset */
void snes_mode2_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       const snes_tile_offset_t *ofs,
                       int authentic);

/* Mode 3: BG1(8bpp) + BG2(4bpp) */
void snes_mode3_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       int authentic);

/* Mode 4: BG1(8bpp) + BG2(2bpp), per-tile column offset on BG1 */
void snes_mode4_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       const snes_tile_offset_t *ofs,
                       int authentic);

/* Mode 5: BG1(4bpp) + BG2(2bpp), hi-res */
void snes_mode5_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       int authentic);

/* Mode 6: BG1(4bpp), hi-res + per-tile column offset */
void snes_mode6_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1,
                       const snes_tile_offset_t *ofs,
                       int authentic);

/* === Mode 7: affine ground projection ===
 *
 * Renders a textured ground plane with perspective, optionally twisted
 * (per-scanline angle drift) for spiral / vortex looks. Sky region
 * above `horizon` is left untouched — the caller paints it however
 * they want (gradient, sun, stars, etc.).
 *
 * Internally uses mode7_scanline (lib/mode7_neon.S) at half-vertical
 * resolution with NEON memcpy line-doubling — the same path the
 * earlier hand-rolled demos used, now shared. Sin/cos tables are
 * built lazily on first call.
 *
 * Parameter struct lets you reuse the renderer for very different
 * scenes (Star Fox death, F-Zero turbo, FF6 intro vortex, racing-
 * sim ground, infinite tiled plane) — change `twist`, `space_z`,
 * and the texture, keep everything else.
 */
typedef struct {
    /* Camera in world coords (Q0). */
    int32_t  cam_x, cam_y;
    uint8_t  angle;         /* yaw, 0..255 maps to 0..360° */
    /* Artistic per-scanline angle drift. 0 = flat camera (rotates as
     * a rigid plane — Star Fox style). Positive values add a swirl
     * that gets stronger toward the horizon — classic SNES vortex /
     * F-Zero turbo flash. 1 matches the previous hand-rolled
     * mode7_neon look; higher values give wilder spirals. */
    uint8_t  twist;
    uint16_t horizon;        /* y in the render rect where the ground starts */
    uint16_t space_z;        /* depth scale. 8000 is a good default. */
    /* Texture (tile-indexed). map_w must be a power of 2; map_w_bits
     * is log2(map_w) and map_mask is map_w - 1. Map repeats infinitely
     * via mask. */
    const uint8_t  *map;
    const uint32_t *palette;  /* 256 ARGB entries */
    uint8_t  map_w_bits;
    uint8_t  map_mask;
} snes_mode7_t;

void snes_mode7_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       const snes_mode7_t *m7,
                       int authentic);

/* Shared sin/cos LUTs in Q12 (multiply by 4096 to get fixed-point).
 * Useful when you want to do your own camera or sprite math against
 * the same angles snes_mode7_render uses. Indexed 0..255 (one byte
 * of angular resolution). */
extern const int32_t *snes_sin_lut(void);
extern const int32_t *snes_cos_lut(void);

/* Direct primitive (still exposed for demos that want non-affine
 * tunnel/vortex effects — see examples/mode7_vortex/main.c). */
/* mode7_scanline / iso_scanline come from lib/mode7_neon.S and are
 * declared in jupiter.h. */

/* ================================================================== */
/* SNES Sprite (OAM) System                                             */
/*                                                                      */
/* 128 sprites, 4bpp tiles (16 colors/palette), 8 sprite palettes,      */
/* priority 0-3, horizontal/vertical flip, variable sizes.              */
/* 32 sprites per scanline limit.                                        */
/*                                                                      */
/* Tile format: 4bpp, 32 bytes per 8x8 tile.                           */
/*   Bytes [0..1]: bitplanes 0-1, row 0                                 */
/*   Bytes [2..3]: bitplanes 0-1, row 1                                 */
/*   ...                                                                 */
/*   Bytes [14..15]: bitplanes 0-1, row 7                               */
/*   Bytes [16..17]: bitplanes 2-3, row 0                               */
/*   ...                                                                 */
/*   Bytes [30..31]: bitplanes 2-3, row 7                               */
/*                                                                      */
/* For row r, pixel p (0=left, 7=right):                                */
/*   bit0 = (tile[r*2]     >> (7-p)) & 1                                */
/*   bit1 = (tile[r*2+1]   >> (7-p)) & 1                                */
/*   bit2 = (tile[r*2+16]  >> (7-p)) & 1                                */
/*   bit3 = (tile[r*2+17]  >> (7-p)) & 1                                */
/*   color_index = (bit3<<3)|(bit2<<2)|(bit1<<1)|bit0   (0=transparent) */
/* ================================================================== */

typedef struct {
    int16_t  x, y;           /* screen position (signed, can be partially off-screen) */
    uint16_t tile;           /* tile index (into sprite CHR) */
    uint8_t  w, h;           /* size in tiles (1-8, so 8x8 to 64x64 pixels) */
    uint8_t  pal;            /* palette 0-7 */
    uint8_t  priority;       /* 0-3 (not modeled in simple renderer) */
    uint8_t  fliph, flipv;
    uint8_t  enabled;
} snes_sprite_t;

#define SNES_MAX_SPRITES      128
#define SNES_SPRITES_PER_LINE 32

/* Render sprites onto UI0 overlay (alpha-keyed).
 * Sprites use 4bpp tiles with column-major layout (like Genesis).
 * Tile index: base + tx * sprite.h + ty
 *
 *   ovl          UI0 overlay (ARGB8888)
 *   fb_w, fb_h   framebuffer dimensions
 *   sprite_chr   4bpp tile data (32 bytes per tile)
 *   sprite_pal   8 palettes × 16 ARGB colors = 128 entries
 *   sprites      array of sprite descriptors
 *   num_sprites  number of sprites
 *   authentic    1 = 256×224 centered render
 */
void snes_render_sprites(uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                         const uint8_t *sprite_chr,
                         const uint32_t *sprite_pal,
                         const snes_sprite_t *sprites, uint32_t num_sprites,
                         int authentic);

#endif /* SNES_H */
