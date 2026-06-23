/*
 * Jupiter SDK — Sega Genesis VDP-style plane renderer
 *
 * Two scroll planes + one window, drawn across the DE2 hardware layers:
 *   Plane B → VI0 (back, opaque with backdrop)
 *   Plane A → UI0 (front, alpha-keyed)
 *   Window  → UI0 inside a fixed rectangle, replacing Plane A there
 *
 * All tiles are 4bpp. 4 palettes × 16 colors in CRAM.
 *
 * Authentic mode: render only into a centered 320×224 rectangle and
 * leave the pillarbox black. Caller must have cleared both framebuffers
 * to black once at init; the renderer never touches the pillarbox.
 */
#include "genesis.h"
#include "jupiter.h"

/* Pre-resolved tile column: cached entry + decoded tile row pointer */
typedef struct {
    const uint8_t *trow;   /* pointer to this row's tile pixel data */
    uint32_t pal_off;      /* palette offset in CRAM (pal_num * 16) */
    uint16_t tidx;         /* tile index (0 = transparent/empty) */
    uint8_t  fliph;
} tcache_t;

/* Cache one tile column for one plane at the current scanline. */
static inline void cache_tile(tcache_t *tc, const genesis_plane_t *p,
                               uint32_t tile_col, uint32_t world_y)
{
    if (!p || !p->enabled) { tc->tidx = 0; return; }

    uint32_t row = (world_y >> 3) & (p->map_h - 1);
    uint16_t entry = p->map[row * p->map_w + tile_col];
    tc->tidx  = GEN_GET_TILE(entry);
    tc->fliph = GEN_GET_FLIPH(entry);

    if (tc->tidx == 0) return;

    uint32_t flipv = GEN_GET_FLIPV(entry);
    uint32_t fy = flipv ? (7 - (world_y & 7)) : (world_y & 7);

    tc->trow    = p->tiles + tc->tidx * 32 + fy * 4;
    tc->pal_off = GEN_GET_PAL(entry) * 16;
}

/* Decode one 4bpp pixel from a cached tile column. */
static inline uint8_t tc_pixel(const tcache_t *tc, uint32_t fine_x)
{
    if (tc->tidx == 0) return 0;
    uint32_t fx = tc->fliph ? (7 - fine_x) : fine_x;
    uint8_t b = tc->trow[fx >> 1];
    return (fx & 1) ? (b & 0x0F) : (b >> 4);
}

/* Render one scroll plane to the opaque VI0 framebuffer within a rect.
 * If plane->line_hscroll is set, that per-scanline offset is added to
 * scroll_x, enabling Genesis line-scroll effects. */
static void render_plane_fb(uint32_t *fb, uint32_t pitch,
                             uint32_t x0, uint32_t y0,
                             uint32_t rw, uint32_t rh,
                             uint32_t backdrop,
                             const genesis_plane_t *p)
{
    if (!p || !p->enabled) {
        for (uint32_t py = y0; py < y0 + rh; py++) {
            uint32_t *row = fb + py * pitch;
            for (uint32_t px = x0; px < x0 + rw; px++) row[px] = backdrop;
        }
        return;
    }

    uint32_t mw = p->map_w - 1;

    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t lpy = py - y0;
        int32_t sx = p->scroll_x;
        if (p->line_hscroll) sx += p->line_hscroll[lpy];
        uint32_t wy = (uint32_t)((int32_t)lpy + p->scroll_y);
        uint32_t *row = fb + py * pitch;

        tcache_t tc = {0};
        uint32_t cur = ~0u;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t wx = (uint32_t)((int32_t)(px - x0) + sx);
            uint32_t col = (wx >> 3) & mw;
            if (col != cur) { cache_tile(&tc, p, col, wy); cur = col; }

            if (tc.tidx) {
                uint8_t ci = tc_pixel(&tc, wx & 7);
                if (ci) { row[px] = p->cram[tc.pal_off + ci]; continue; }
            }
            row[px] = backdrop;
        }
    }
}

/* Render one scroll plane to the alpha-keyed UI0 overlay within a rect. */
static void render_plane_ovl(uint32_t *ovl, uint32_t pitch,
                              uint32_t x0, uint32_t y0,
                              uint32_t rw, uint32_t rh,
                              const genesis_plane_t *p)
{
    if (!p || !p->enabled) {
        for (uint32_t py = y0; py < y0 + rh; py++) {
            uint32_t *row = ovl + py * pitch;
            for (uint32_t px = x0; px < x0 + rw; px++) row[px] = 0;
        }
        return;
    }

    uint32_t mw = p->map_w - 1;

    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t lpy = py - y0;
        int32_t sx = p->scroll_x;
        if (p->line_hscroll) sx += p->line_hscroll[lpy];
        uint32_t wy = (uint32_t)((int32_t)lpy + p->scroll_y);
        uint32_t *row = ovl + py * pitch;

        tcache_t tc = {0};
        uint32_t cur = ~0u;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t wx = (uint32_t)((int32_t)(px - x0) + sx);
            uint32_t col = (wx >> 3) & mw;
            if (col != cur) { cache_tile(&tc, p, col, wy); cur = col; }

            if (tc.tidx) {
                uint8_t ci = tc_pixel(&tc, wx & 7);
                if (ci) { row[px] = p->cram[tc.pal_off + ci]; continue; }
            }
            row[px] = 0;
        }
    }
}

/* Render one sprite onto the overlay. Clipped to the render rect.
 * Sprite coords are relative to the render rect's (0,0). Tiles are in
 * the shared tiles array, laid out column-major (Genesis convention). */
static void render_sprite(uint32_t *ovl, uint32_t pitch,
                           uint32_t rx0, uint32_t ry0,
                           uint32_t rw, uint32_t rh,
                           const genesis_sprite_t *s,
                           const uint8_t *tiles,
                           const uint32_t *cram)
{
    if (!s || !s->enabled || !s->w || !s->h) return;

    int32_t sw = (int32_t)s->w * 8;
    int32_t sh = (int32_t)s->h * 8;

    /* Clip sprite to render rect (all in rect-local coords) */
    int32_t lx0 = s->x, ly0 = s->y;
    int32_t lx1 = lx0 + sw, ly1 = ly0 + sh;
    if (lx0 < 0) lx0 = 0;
    if (ly0 < 0) ly0 = 0;
    if (lx1 > (int32_t)rw) lx1 = rw;
    if (ly1 > (int32_t)rh) ly1 = rh;
    if (lx0 >= lx1 || ly0 >= ly1) return;

    uint32_t pal_off = s->pal * 16;

    for (int32_t ly = ly0; ly < ly1; ly++) {
        int32_t sprite_y = ly - s->y;
        int32_t actual_y = s->flipv ? (sh - 1 - sprite_y) : sprite_y;
        uint32_t tile_y = (uint32_t)(actual_y >> 3);
        uint32_t fine_y = (uint32_t)(actual_y & 7);

        uint32_t *row = ovl + (ry0 + ly) * pitch + rx0;

        const uint8_t *cur_trow = 0;
        int32_t cur_tile_x = -1;

        for (int32_t lx = lx0; lx < lx1; lx++) {
            int32_t sprite_x = lx - s->x;
            int32_t actual_x = s->fliph ? (sw - 1 - sprite_x) : sprite_x;
            int32_t tile_x = actual_x >> 3;

            if (tile_x != cur_tile_x) {
                /* Column-major: tile index = base + tx*h + ty */
                uint16_t tile_idx = s->tile + (uint32_t)tile_x * s->h + tile_y;
                cur_trow = tiles + tile_idx * 32 + fine_y * 4;
                cur_tile_x = tile_x;
            }

            uint32_t fine_x = (uint32_t)(actual_x & 7);
            uint8_t b = cur_trow[fine_x >> 1];
            uint8_t ci = (fine_x & 1) ? (b & 0x0F) : (b >> 4);

            if (ci) row[lx] = cram[pal_off + ci];
        }
    }
}

/* Draw the window plane on top of the overlay. The window's coords are
 * relative to the render rect's top-left (not the physical framebuffer). */
static void render_window(uint32_t *ovl, uint32_t pitch,
                           uint32_t x0, uint32_t y0,
                           uint32_t rw, uint32_t rh,
                           const genesis_window_t *w,
                           const genesis_plane_t *plane_a)
{
    if (!w || !w->map || !plane_a || !plane_a->enabled) return;

    /* Clip window to the render rect */
    uint32_t wx0 = w->x, wy0 = w->y;
    uint32_t wx1 = w->x + w->w; if (wx1 > rw) wx1 = rw;
    uint32_t wy1 = w->y + w->h; if (wy1 > rh) wy1 = rh;
    if (wx0 >= wx1 || wy0 >= wy1) return;

    uint32_t mw = w->map_w - 1;
    uint32_t mh = w->map_h - 1;

    for (uint32_t ly = wy0; ly < wy1; ly++) {
        uint32_t local_y = ly - wy0;
        uint32_t ty = (local_y >> 3) & mh;
        uint32_t fy = local_y & 7;
        uint32_t *row = ovl + (y0 + ly) * pitch;

        tcache_t tc = {0};
        uint32_t cur = ~0u;

        for (uint32_t lx = wx0; lx < wx1; lx++) {
            uint32_t local_x = lx - wx0;
            uint32_t col = (local_x >> 3) & mw;

            if (col != cur) {
                uint16_t entry = w->map[ty * w->map_w + col];
                tc.tidx  = GEN_GET_TILE(entry);
                tc.fliph = GEN_GET_FLIPH(entry);
                if (tc.tidx) {
                    uint32_t flipv = GEN_GET_FLIPV(entry);
                    uint32_t ry = flipv ? (7 - fy) : fy;
                    tc.trow    = plane_a->tiles + tc.tidx * 32 + ry * 4;
                    tc.pal_off = GEN_GET_PAL(entry) * 16;
                }
                cur = col;
            }

            if (tc.tidx) {
                uint8_t ci = tc_pixel(&tc, local_x & 7);
                if (ci) { row[x0 + lx] = plane_a->cram[tc.pal_off + ci]; continue; }
            }
            row[x0 + lx] = 0;
        }
    }
}

void genesis_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                    uint32_t backdrop,
                    const genesis_plane_t *plane_a,
                    const genesis_plane_t *plane_b,
                    const genesis_window_t *window,
                    const genesis_sprite_t *sprites, uint32_t num_sprites,
                    int authentic)
{
    uint32_t x0 = 0, y0 = 0, rw = fb_w, rh = fb_h;
    if (authentic) {
        rw = GEN_NATIVE_W;
        rh = GEN_NATIVE_H;
        x0 = (fb_w - rw) / 2;
        y0 = (fb_h - rh) / 2;
    }
    render_plane_fb (fb,  fb_w, x0, y0, rw, rh, backdrop, plane_b);
    render_plane_ovl(ovl, fb_w, x0, y0, rw, rh,           plane_a);
    render_window   (ovl, fb_w, x0, y0, rw, rh, window,   plane_a);

    /* Sprites on top of Plane A + Window, using Plane A's tiles/CRAM */
    if (sprites && num_sprites && plane_a && plane_a->enabled) {
        for (uint32_t i = 0; i < num_sprites; i++)
            render_sprite(ovl, fb_w, x0, y0, rw, rh,
                          &sprites[i], plane_a->tiles, plane_a->cram);
    }
}
