/*
 * Jupiter SDK — Sega Genesis VDP-style plane renderer
 *
 * Simpler than SNES: fixed 4bpp tiles, two scroll planes, one fixed
 * window region. No modes — the VDP just has Plane A (front) over
 * Plane B (back), plus a Window plane that replaces Plane A in a
 * screen-aligned rectangle.
 *
 * Tile format:
 *   Always 4bpp, 32 bytes per tile (8x8 pixels, 2 pixels/byte)
 *
 * Nametable entry (16-bit), matching Genesis VDP bit layout:
 *   [10:0]  tile index (0–2047)
 *   [12:11] palette number (0–3, selects one of 4 CRAM palettes)
 *   [13]    horizontal flip
 *   [14]    vertical flip
 *   [15]    priority (0=low, 1=high) — ignored by BG layering here,
 *           priority bits only matter for sprite-vs-BG, which this
 *           renderer doesn't model. Plane A is always in front of B.
 *
 * Rendering model:
 *   Plane B → VI0 (back, opaque with backdrop)
 *   Plane A → UI0 (front, alpha-keyed, hardware-blended over B)
 *   Window  → UI0 in its rectangle, replacing Plane A there
 */
#ifndef GENESIS_H
#define GENESIS_H

#include <stdint.h>

/* Native Genesis resolution (H40 mode). Used when authentic=1. */
#define GEN_NATIVE_W 320
#define GEN_NATIVE_H 224

/* Nametable entry macros */
#define GEN_TILE(idx)       ((idx) & 0x7FF)
#define GEN_PAL(p)          (((p) & 3) << 11)
#define GEN_FLIPH           (1 << 13)
#define GEN_FLIPV           (1 << 14)
#define GEN_PRIO            (1 << 15)

#define GEN_ENTRY(idx, pal, fh, fv) \
    (GEN_TILE(idx) | GEN_PAL(pal) | ((fh) ? GEN_FLIPH : 0) | \
     ((fv) ? GEN_FLIPV : 0))

#define GEN_GET_TILE(e)   ((e) & 0x7FF)
#define GEN_GET_PAL(e)    (((e) >> 11) & 3)
#define GEN_GET_FLIPH(e)  (((e) >> 13) & 1)
#define GEN_GET_FLIPV(e)  (((e) >> 14) & 1)

/* Scroll plane descriptor */
typedef struct {
    const uint8_t  *tiles;    /* 4bpp tile data, 32 bytes per tile */
    const uint16_t *map;      /* nametable: map_w × map_h entries */
    const uint32_t *cram;     /* 64 ARGB8888 colors (4 palettes × 16) */
    int32_t scroll_x, scroll_y;
    /* Optional per-scanline horizontal scroll offset, added to scroll_x.
     * Length must be at least the render rect's height. NULL to disable.
     * Genesis trick: water waves, dizzy effects, forward-motion fake 3D. */
    const int16_t *line_hscroll;
    uint16_t map_w, map_h;    /* must be power of 2 */
    uint8_t enabled;
} genesis_plane_t;

/* Sprite descriptor.
 * Size is in tiles — 1×1 through 4×4, so 8×8 up to 32×32 pixels.
 * Tile data is laid out column-major (Genesis VDP convention): the tile
 * at sprite coords (tx, ty) lives at index `tile + tx * h + ty`. */
typedef struct {
    int16_t  x, y;            /* top-left position on the native screen */
    uint16_t tile;            /* first tile index in shared tile array */
    uint8_t  w, h;            /* size in tiles (1–4) */
    uint8_t  pal;             /* palette 0–3 */
    uint8_t  fliph, flipv;
    uint8_t  enabled;
} genesis_sprite_t;

/* Window plane — replaces Plane A inside a screen-aligned rectangle.
 * Unlike Plane A, the window does not scroll (its map is locked to
 * screen coordinates). Leave `map` NULL to disable. */
typedef struct {
    const uint16_t *map;      /* NULL = window disabled */
    uint16_t x, y;            /* top-left in pixels */
    uint16_t w, h;            /* size in pixels (must be multiple of 8) */
    uint16_t map_w, map_h;    /* nametable dimensions */
} genesis_window_t;

/* Shared tile data pointer — sprites borrow from the same 4bpp tile
 * array as the planes, so one tile ROM serves everything. */

/* Render one frame.
 *   fb          VI0 framebuffer (XRGB8888)
 *   ovl         UI0 overlay     (ARGB8888, transparent where empty)
 *   backdrop    opaque backdrop color written to fb where both planes are empty
 *   plane_a     foreground plane (drawn to UI0)
 *   plane_b     background plane (drawn to VI0)
 *   window      optional window (drawn to UI0 in its rectangle); pass NULL or
 *               a struct with map=NULL to disable
 *   sprites     array of sprites; rendered on top of Plane A and Window.
 *               Sprites share Plane A's tile array and CRAM. Pass NULL/0 to skip.
 *   num_sprites number of entries in the sprite array
 *   authentic   0 = full-screen render; 1 = centered GEN_NATIVE_W × GEN_NATIVE_H
 *               render with pillarbox. Caller must pre-clear both framebuffers
 *               to black once when authentic mode is used — the renderer never
 *               touches the pillarbox.
 */
void genesis_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                    uint32_t backdrop,
                    const genesis_plane_t *plane_a,
                    const genesis_plane_t *plane_b,
                    const genesis_window_t *window,
                    const genesis_sprite_t *sprites, uint32_t num_sprites,
                    int authentic);

#endif /* GENESIS_H */
