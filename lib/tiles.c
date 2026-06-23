/*
 * Jupiter SDK — tiles.c
 * Software tile renderers with horizontal scrolling.
 *
 * Two renderers:
 *   tiles_render()      — Original. Function pointer per pixel. Any map width.
 *   tiles_render_fast() — Optimized. LUT colors. Power-of-2 map width.
 *                         Full-tile inner loop is NEON ASM (tiles_neon.S).
 *                         C handles scroll math and partial edge tiles.
 */
#include "jupiter.h"

#define TILE_W 8
#define TILE_H 8

/* ASM inner loop (tiles_neon.S): blast full 8-pixel tiles via NEON */
extern void _tile_row_blast(uint32_t *dst,
                             const uint8_t *map_row,
                             const uint32_t *color_lut,
                             uint32_t tx_start,
                             uint32_t map_mask,
                             uint32_t num_tiles);

/* ---- Original renderer (kept for compatibility) ---- */

void tiles_render(volatile uint32_t *fb, uint32_t fb_pitch,
                  const uint8_t *map, uint32_t map_w,
                  uint32_t y_start, uint32_t rows,
                  int scroll_x, tile_color_fn color_fn)
{
    /* Wrap scroll into positive range */
    int wrap = (int)(map_w * TILE_W);
    scroll_x = ((scroll_x % wrap) + wrap) % wrap;

    for (uint32_t row = 0; row < rows * TILE_H; row++) {
        uint32_t tile_y = row / TILE_H;
        uint32_t py = row % TILE_H;
        uint32_t screen_y = y_start + row;

        if (screen_y >= LCD_H) break;

        for (uint32_t col = 0; col < fb_pitch; col++) {
            uint32_t world_x = (uint32_t)(col + scroll_x);
            uint32_t tile_x = (world_x / TILE_W) % map_w;
            uint32_t px = world_x % TILE_W;

            uint8_t tile_id = map[tile_y * map_w + tile_x];
            fb[screen_y * fb_pitch + col] = color_fn(tile_id, px, py);
        }
    }
}

/* ---- Fast renderer ----
 *
 * Kills the three per-pixel bottlenecks:
 *
 *   1. Function pointer call (~50-100 cycles on in-order A7)
 *      → Replaced by color_lut[tile_id] — single indexed load
 *
 *   2. Non-power-of-2 modulo (% map_w → UDIV + MLS, ~20-40 cycles)
 *      → Replaced by & map_mask — single AND instruction
 *
 *   3. Per-pixel tile lookup (same tile looked up 8 times)
 *      → Tile-run loop: look up once, write 8 pixels
 *
 * Constraints:
 *   - map_w MUST be power of 2 (32, 64, 128...)
 *   - Flat color per tile (color_lut[tile_id] = one uint32_t)
 *   - If you need per-pixel texture within a tile, use tiles_render()
 */
void tiles_render_fast(volatile uint32_t *fb, uint32_t fb_pitch,
                       const uint8_t *map, uint32_t map_w,
                       const uint32_t *color_lut,
                       uint32_t y_start, uint32_t rows,
                       int scroll_x)
{
    uint32_t map_mask = map_w - 1;   /* power-of-2 requirement */
    uint32_t total_h = rows * TILE_H;

    /* Wrap scroll to positive pixel range */
    uint32_t wrap_px = map_w * TILE_W;       /* also power of 2 */
    scroll_x = ((scroll_x % (int)wrap_px) + (int)wrap_px) % (int)wrap_px;

    /* Decompose scroll into tile offset + pixel remainder */
    uint32_t first_tile = (uint32_t)scroll_x >> 3;  /* / TILE_W */
    uint32_t pixel_off  = (uint32_t)scroll_x & 7;   /* % TILE_W */

    for (uint32_t row = 0; row < total_h; row++) {
        uint32_t screen_y = y_start + row;
        if (screen_y >= LCD_H) break;

        uint32_t tile_y = row >> 3;                  /* / TILE_H */
        const uint8_t *map_row = map + tile_y * map_w;
        volatile uint32_t *dst = fb + screen_y * fb_pitch;

        uint32_t col = 0;
        uint32_t tx = first_tile;

        /* ---- First partial tile (if scroll isn't tile-aligned) ---- */
        if (pixel_off > 0) {
            uint32_t color = color_lut[map_row[tx & map_mask]];
            uint32_t run = TILE_W - pixel_off;
            if (run > fb_pitch) run = fb_pitch;
            for (uint32_t p = 0; p < run; p++)
                dst[col++] = color;
            tx++;
        }

        /* ---- Full 8-pixel tiles: NEON ASM inner loop ---- */
        uint32_t remaining = fb_pitch - col;
        uint32_t full_tiles = remaining >> 3;  /* / TILE_W */

        if (full_tiles > 0) {
            _tile_row_blast((uint32_t *)(dst + col), map_row,
                            color_lut, tx, map_mask, full_tiles);
            col += full_tiles * TILE_W;
            tx  += full_tiles;
        }

        /* ---- Last partial tile ---- */
        if (col < fb_pitch) {
            uint32_t color = color_lut[map_row[tx & map_mask]];
            while (col < fb_pitch)
                dst[col++] = color;
        }
    }
}
