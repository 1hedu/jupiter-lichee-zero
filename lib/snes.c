/*
 * Jupiter SDK — SNES-style background renderer
 *
 * Tile-column compositor: per scanline, cache tilemap entries for
 * all BGs at each tile column, then resolve priority for 8 pixels
 * using cached entries. 16× fewer tilemap lookups than naive.
 */
#include "snes.h"
#include "jupiter.h"

/* Pre-resolved tile column: cached entry + decoded tile row pointer */
typedef struct {
    const uint8_t *trow;    /* pointer to this row's tile pixel data */
    uint32_t pal_off;       /* palette offset for this tile */
    uint8_t  tidx;          /* tile index (0=transparent) */
    uint8_t  flipx;
    uint8_t  pri;           /* priority bit */
    uint8_t  bpp;
} tcache_t;

/* Resolve one tile column for one BG */
static inline void cache_tile(tcache_t *tc, const snes_bg_t *bg,
                               uint32_t tile_col, uint32_t fine_y)
{
    if (!bg || !bg->enabled) { tc->tidx = 0; return; }

    uint16_t entry = bg->map[((fine_y >> 3) % bg->map_h) * bg->map_w + tile_col];
    tc->tidx  = SNES_GET_TILE(entry);
    tc->pri   = SNES_GET_PRIO(entry);
    tc->flipx = SNES_GET_FLIPX(entry);
    tc->bpp   = bg->bpp;

    if (tc->tidx == 0) return;

    uint32_t flipy = SNES_GET_FLIPY(entry);
    uint32_t fy = flipy ? (7 - (fine_y & 7)) : (fine_y & 7);

    uint32_t trb, tsz, cpp;
    if (bg->bpp == 8)      { trb = 8; tsz = 64; cpp = 256; }
    else if (bg->bpp == 4) { trb = 4; tsz = 32; cpp = 16;  }
    else                   { trb = 2; tsz = 16; cpp = 4;   }

    tc->trow = bg->tiles + tc->tidx * tsz + fy * trb;
    tc->pal_off = SNES_GET_PAL(entry) * cpp;
}

/* Decode one pixel from cached tile column */
static inline uint8_t tc_pixel(const tcache_t *tc, uint32_t fine_x)
{
    if (tc->tidx == 0) return 0;
    uint32_t fx = tc->flipx ? (7 - fine_x) : fine_x;

    if (tc->bpp == 8) return tc->trow[fx];
    if (tc->bpp == 4) {
        uint8_t b = tc->trow[fx >> 1];
        return (fx & 1) ? (b & 0x0F) : (b >> 4);
    }
    uint8_t b = tc->trow[fx >> 2];
    return (b >> ((3 - (fx & 3)) * 2)) & 3;
}

/* ================================================================
 *  Single-BG rect renderer (VI0 back layer, opaque with backdrop)
 * ================================================================ */

static void render_bg_rect(uint32_t *fb, uint32_t pitch,
                            uint32_t x0, uint32_t y0,
                            uint32_t rw, uint32_t rh,
                            const snes_bg_t *bg, uint32_t backdrop)
{
    if (!bg || !bg->enabled) {
        for (uint32_t py = y0; py < y0 + rh; py++) {
            uint32_t *row = fb + py * pitch;
            for (uint32_t px = x0; px < x0 + rw; px++) row[px] = backdrop;
        }
        return;
    }

    int32_t sx = bg->scroll_x, sy = bg->scroll_y;

    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t wy = (uint32_t)((int32_t)(py - y0) + sy);
        uint32_t *row = fb + py * pitch;

        tcache_t tc={0};
        uint32_t cur_tc = ~0u;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t wx = (uint32_t)((int32_t)(px - x0) + sx);
            uint32_t tile_col = (wx >> 3) % bg->map_w;

            if (tile_col != cur_tc) {
                cache_tile(&tc, bg, tile_col, wy);
                cur_tc = tile_col;
            }

            if (tc.tidx == 0) { row[px] = backdrop; continue; }

            uint8_t ci = tc_pixel(&tc, wx & 7);
            row[px] = ci ? bg->palette[tc.pal_off + ci] : backdrop;
        }
    }
}

/* ================================================================
 *  Mode compositors — split across DE2 hardware layers
 *
 *  Bottom BG → VI0 (fb):  render_bg_rect, opaque with backdrop
 *  Upper BGs → UI0 (ovl): overlay rect compositors, 0 where transparent
 *  Hardware blender composites UI0 over VI0 at zero CPU cost.
 *
 *  Each mode wrapper accepts `int authentic`. When set, the renderer
 *  draws into a centered SNES_NATIVE_W × SNES_NATIVE_H rectangle and
 *  leaves the pillarbox untouched (caller must pre-clear both buffers
 *  to black once). Scroll coords are relative to the rect's (0,0).
 * ================================================================ */

/* Compute centered render rect based on authentic flag */
static inline void compute_rect(int authentic, uint32_t fb_w, uint32_t fb_h,
                                 uint32_t *x0, uint32_t *y0,
                                 uint32_t *rw, uint32_t *rh)
{
    if (authentic) {
        *rw = SNES_NATIVE_W; *rh = SNES_NATIVE_H;
        *x0 = (fb_w - *rw) / 2;
        *y0 = (fb_h - *rh) / 2;
    } else {
        *x0 = 0; *y0 = 0;
        *rw = fb_w; *rh = fb_h;
    }
}

/* ---- Overlay compositors (UI0) — rect-aware ---- */

/* 1-BG overlay */
static void render_ovl1_rect(uint32_t *ovl, uint32_t pitch,
                              uint32_t x0, uint32_t y0,
                              uint32_t rw, uint32_t rh,
                              const snes_bg_t *bg1)
{
    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t wy = (uint32_t)((int32_t)(py - y0) + bg1->scroll_y);
        uint32_t *row = ovl + py * pitch;
        tcache_t t1={0}; uint32_t ct1 = ~0u;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t wx = (uint32_t)((int32_t)(px - x0) + bg1->scroll_x);
            uint32_t tc = (wx >> 3) % bg1->map_w;
            if (tc != ct1) { cache_tile(&t1, bg1, tc, wy); ct1 = tc; }

            if (t1.tidx) {
                uint8_t ci = tc_pixel(&t1, wx & 7);
                if (ci) { row[px] = bg1->palette[t1.pal_off + ci]; continue; }
            }
            row[px] = 0;
        }
    }
}

/* 2-BG overlay: front-to-back, first opaque wins */
static void render_ovl2_rect(uint32_t *ovl, uint32_t pitch,
                              uint32_t x0, uint32_t y0,
                              uint32_t rw, uint32_t rh,
                              const snes_bg_t *bg1, const snes_bg_t *bg2)
{
    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t lpy = py - y0;
        uint32_t wy1 = (uint32_t)((int32_t)lpy + bg1->scroll_y);
        uint32_t wy2 = (uint32_t)((int32_t)lpy + bg2->scroll_y);
        uint32_t *row = ovl + py * pitch;

        tcache_t t1={0}, t2={0};
        uint32_t ct1 = ~0u, ct2 = ~0u;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t lpx = px - x0;
            uint32_t tc;
            tc = ((uint32_t)((int32_t)lpx + bg1->scroll_x) >> 3) % bg1->map_w;
            if (tc != ct1) { cache_tile(&t1, bg1, tc, wy1); ct1 = tc; }
            tc = ((uint32_t)((int32_t)lpx + bg2->scroll_x) >> 3) % bg2->map_w;
            if (tc != ct2) { cache_tile(&t2, bg2, tc, wy2); ct2 = tc; }

            uint32_t fx1 = (uint32_t)((int32_t)lpx + bg1->scroll_x) & 7;
            uint32_t fx2 = (uint32_t)((int32_t)lpx + bg2->scroll_x) & 7;
            uint8_t ci;

            #define TRY(t, bg, fx) \
                if ((t).tidx) { \
                    ci = tc_pixel(&(t), (fx)); \
                    if (ci) { row[px] = (bg)->palette[(t).pal_off + ci]; goto done; } \
                }
            TRY(t1, bg1, fx1)
            TRY(t2, bg2, fx2)
            row[px] = 0;
            done:;
            #undef TRY
        }
    }
}

/* 3-BG overlay: front-to-back, first opaque wins */
static void render_ovl3_rect(uint32_t *ovl, uint32_t pitch,
                              uint32_t x0, uint32_t y0,
                              uint32_t rw, uint32_t rh,
                              const snes_bg_t *bg1, const snes_bg_t *bg2,
                              const snes_bg_t *bg3)
{
    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t lpy = py - y0;
        uint32_t wy1 = (uint32_t)((int32_t)lpy + bg1->scroll_y);
        uint32_t wy2 = (uint32_t)((int32_t)lpy + bg2->scroll_y);
        uint32_t wy3 = (uint32_t)((int32_t)lpy + bg3->scroll_y);
        uint32_t *row = ovl + py * pitch;

        tcache_t t1={0}, t2={0}, t3={0};
        uint32_t ct1 = ~0u, ct2 = ~0u, ct3 = ~0u;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t lpx = px - x0;
            uint32_t tc;
            tc = ((uint32_t)((int32_t)lpx + bg1->scroll_x) >> 3) % bg1->map_w;
            if (tc != ct1) { cache_tile(&t1, bg1, tc, wy1); ct1 = tc; }
            tc = ((uint32_t)((int32_t)lpx + bg2->scroll_x) >> 3) % bg2->map_w;
            if (tc != ct2) { cache_tile(&t2, bg2, tc, wy2); ct2 = tc; }
            tc = ((uint32_t)((int32_t)lpx + bg3->scroll_x) >> 3) % bg3->map_w;
            if (tc != ct3) { cache_tile(&t3, bg3, tc, wy3); ct3 = tc; }

            uint32_t fx1 = (uint32_t)((int32_t)lpx + bg1->scroll_x) & 7;
            uint32_t fx2 = (uint32_t)((int32_t)lpx + bg2->scroll_x) & 7;
            uint32_t fx3 = (uint32_t)((int32_t)lpx + bg3->scroll_x) & 7;
            uint8_t ci;

            #define TRY(t, bg, fx) \
                if ((t).tidx) { \
                    ci = tc_pixel(&(t), (fx)); \
                    if (ci) { row[px] = (bg)->palette[(t).pal_off + ci]; goto done; } \
                }
            TRY(t1, bg1, fx1)
            TRY(t2, bg2, fx2)
            TRY(t3, bg3, fx3)
            row[px] = 0;
            done:;
            #undef TRY
        }
    }
}

/* 1-BG overlay with per-tile-column offset */
static void render_ovl1_ofs_rect(uint32_t *ovl, uint32_t pitch,
                                  uint32_t x0, uint32_t y0,
                                  uint32_t rw, uint32_t rh,
                                  const snes_bg_t *bg1,
                                  const snes_tile_offset_t *ofs)
{
    uint32_t tile_cols = (rw + 7) / 8;

    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t lpy = py - y0;
        uint32_t *row = ovl + py * pitch;
        tcache_t t1={0}; uint32_t ct1 = ~0u;
        int32_t cur_ofs = 0;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t lpx = px - x0;
            uint32_t scol = lpx >> 3;
            if ((lpx & 7) == 0 && ofs && ofs->col_offset && scol < tile_cols)
                cur_ofs = ofs->col_offset[scol];

            int32_t sx1 = bg1->scroll_x + (ofs && !ofs->vertical ? cur_ofs : 0);
            int32_t sy1 = bg1->scroll_y + (ofs && ofs->vertical  ? cur_ofs : 0);
            uint32_t wx1 = (uint32_t)((int32_t)lpx + sx1);
            uint32_t wy1 = (uint32_t)((int32_t)lpy + sy1);

            uint32_t tc = (wx1 >> 3) % bg1->map_w;
            if (tc != ct1 || (lpx & 7) == 0) {
                cache_tile(&t1, bg1, tc, wy1);
                ct1 = tc;
            }

            if (t1.tidx) {
                uint8_t ci = tc_pixel(&t1, wx1 & 7);
                if (ci) { row[px] = bg1->palette[t1.pal_off + ci]; continue; }
            }
            row[px] = 0;
        }
    }
}

/* ---- Mode wrappers ---- */

/* Mode 0: VI0=BG4, UI0=BG1+BG2+BG3 */
void snes_mode0_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       const snes_bg_t *bg3, const snes_bg_t *bg4,
                       int authentic)
{
    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);
    render_bg_rect  (fb,  fb_w, x0, y0, rw, rh, bg4, backdrop);
    render_ovl3_rect(ovl, fb_w, x0, y0, rw, rh, bg1, bg2, bg3);
}

/* Mode 1: VI0=BG3, UI0=BG1+BG2 */
void snes_mode1_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop,
                       const snes_bg_t *bg1, const snes_bg_t *bg2,
                       const snes_bg_t *bg3,
                       int authentic)
{
    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);
    render_bg_rect  (fb,  fb_w, x0, y0, rw, rh, bg3, backdrop);
    render_ovl2_rect(ovl, fb_w, x0, y0, rw, rh, bg1, bg2);
}

/* Mode 2: BG1 (with offset) in front, BG2 behind */
void snes_mode2_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t bd, const snes_bg_t *b1, const snes_bg_t *b2,
                       const snes_tile_offset_t *ofs,
                       int authentic)
{
    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);
    render_bg_rect      (fb,  fb_w, x0, y0, rw, rh, b2, bd);
    render_ovl1_ofs_rect(ovl, fb_w, x0, y0, rw, rh, b1, ofs);
}

/* Mode 3: BG1 in front, BG2 behind */
void snes_mode3_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t bd, const snes_bg_t *b1, const snes_bg_t *b2,
                       int authentic)
{
    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);
    render_bg_rect  (fb,  fb_w, x0, y0, rw, rh, b2, bd);
    render_ovl1_rect(ovl, fb_w, x0, y0, rw, rh, b1);
}

/* Mode 4: BG1 (with offset) in front, BG2 behind */
void snes_mode4_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t bd, const snes_bg_t *b1, const snes_bg_t *b2,
                       const snes_tile_offset_t *ofs,
                       int authentic)
{
    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);
    render_bg_rect      (fb,  fb_w, x0, y0, rw, rh, b2, bd);
    render_ovl1_ofs_rect(ovl, fb_w, x0, y0, rw, rh, b1, ofs);
}

/* Mode 5: BG1 in front, BG2 behind */
void snes_mode5_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t bd, const snes_bg_t *b1, const snes_bg_t *b2,
                       int authentic)
{
    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);
    render_bg_rect  (fb,  fb_w, x0, y0, rw, rh, b2, bd);
    render_ovl1_rect(ovl, fb_w, x0, y0, rw, rh, b1);
}

/* Mode 6: VI0=BG1 with offset, UI0=empty */
void snes_mode6_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       uint32_t backdrop, const snes_bg_t *bg1,
                       const snes_tile_offset_t *ofs,
                       int authentic)
{
    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);

    /* VI0: single BG with per-tile-column offset */
    uint32_t tile_cols = (rw + 7) / 8;
    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t lpy = py - y0;
        uint32_t *row = fb + py * fb_w;
        tcache_t t1={0}; uint32_t ct1=~0u;
        int32_t cur_ofs = 0;

        for (uint32_t px = x0; px < x0 + rw; px++) {
            uint32_t lpx = px - x0;
            uint32_t scol = lpx >> 3;
            if ((lpx & 7) == 0 && ofs && ofs->col_offset && scol < tile_cols)
                cur_ofs = ofs->col_offset[scol];

            int32_t sx1 = bg1->scroll_x + (ofs && !ofs->vertical ? cur_ofs : 0);
            int32_t sy1 = bg1->scroll_y + (ofs && ofs->vertical  ? cur_ofs : 0);
            uint32_t wx1 = (uint32_t)((int32_t)lpx + sx1);
            uint32_t wy1 = (uint32_t)((int32_t)lpy + sy1);

            uint32_t tc = (wx1 >> 3) % bg1->map_w;
            if (tc != ct1 || (lpx & 7) == 0) {
                cache_tile(&t1, bg1, tc, wy1);
                ct1 = tc;
            }
            if (t1.tidx) {
                uint8_t ci = tc_pixel(&t1, wx1 & 7);
                if (ci) { row[px] = bg1->palette[t1.pal_off + ci]; continue; }
            }
            row[px] = backdrop;
        }
    }

    /* UI0: empty (within the rect) */
    for (uint32_t py = y0; py < y0 + rh; py++) {
        uint32_t *row = ovl + py * fb_w;
        for (uint32_t px = x0; px < x0 + rw; px++) row[px] = 0;
    }
}

/* ================================================================== */
/* SNES 4bpp pixel decode                                               */
/* ================================================================== */
static inline uint8_t snes_4bpp_pixel(const uint8_t *tile, int row, int col)
{
    int shift = 7 - col;
    uint8_t b0 = (tile[row * 2]      >> shift) & 1;
    uint8_t b1 = (tile[row * 2 + 1]  >> shift) & 1;
    uint8_t b2 = (tile[row * 2 + 16] >> shift) & 1;
    uint8_t b3 = (tile[row * 2 + 17] >> shift) & 1;
    return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* ================================================================== */
/* SNES Sprite Renderer                                                 */
/* ================================================================== */
void snes_render_sprites(uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                         const uint8_t *sprite_chr,
                         const uint32_t *sprite_pal,
                         const snes_sprite_t *sprites, uint32_t num_sprites,
                         int authentic)
{
    if (!sprites || !sprite_chr || !sprite_pal || num_sprites == 0) return;

    uint32_t rw, rh, x0, y0;
    if (authentic) {
        rw = SNES_NATIVE_W; rh = SNES_NATIVE_H;
        x0 = (fb_w - rw) / 2; y0 = (fb_h - rh) / 2;
    } else {
        rw = fb_w; rh = fb_h; x0 = 0; y0 = 0;
    }

    uint8_t line_count[SNES_NATIVE_H];
    for (uint32_t i = 0; i < SNES_NATIVE_H; i++) line_count[i] = 0;

    /* Render in reverse order (lower index = higher priority, drawn last) */
    for (int s = (int)num_sprites - 1; s >= 0; s--) {
        const snes_sprite_t *sp = &sprites[s];
        if (!sp->enabled || !sp->w || !sp->h) continue;

        int32_t spw = (int32_t)sp->w * 8;
        int32_t sph = (int32_t)sp->h * 8;

        for (int32_t py = 0; py < sph; py++) {
            int32_t screen_y = sp->y + py;
            if (screen_y < 0 || screen_y >= (int32_t)rh) continue;
            if (line_count[screen_y] >= SNES_SPRITES_PER_LINE) continue;

            int32_t actual_y = sp->flipv ? (sph - 1 - py) : py;
            int32_t tile_y = actual_y >> 3;
            int32_t fine_y = actual_y & 7;
            int drew = 0;

            for (int32_t px = 0; px < spw; px++) {
                int32_t screen_x = sp->x + px;
                if (screen_x < 0 || screen_x >= (int32_t)rw) continue;

                int32_t actual_x = sp->fliph ? (spw - 1 - px) : px;
                int32_t tile_x = actual_x >> 3;

                /* Column-major: tile = base + tx * h + ty */
                uint16_t tile_idx = sp->tile + (uint32_t)tile_x * sp->h + tile_y;
                const uint8_t *tile = sprite_chr + tile_idx * 32;

                uint8_t ci = snes_4bpp_pixel(tile, fine_y, actual_x & 7);
                if (ci == 0) continue;

                uint32_t color = sprite_pal[sp->pal * 16 + ci];
                ovl[(y0 + screen_y) * fb_w + (x0 + screen_x)] = color;
                drew = 1;
            }
            if (drew) line_count[screen_y]++;
        }
    }
}

/* ================================================================== */
/* Mode 7 — affine ground projection                                    */
/* ================================================================== */

#define M7_FP_SIN     12
#define M7_FP_UV      8
#define M7_SIN_N      256
#define M7_SIN_MASK   255

static int32_t s_m7_sin[M7_SIN_N];
static int32_t s_m7_cos[M7_SIN_N];
static int     s_m7_lut_built = 0;

static void m7_build_lut_once(void)
{
    if (s_m7_lut_built) return;
    s_m7_lut_built = 1;
    for (int i = 0; i < M7_SIN_N; i++) {
        int j = i & 127;
        int32_t val = (4 * j * (128 - j)) >> (14 - M7_FP_SIN);
        if (i >= 128) val = -val;
        s_m7_sin[i] = val;
    }
    for (int i = 0; i < M7_SIN_N; i++)
        s_m7_cos[i] = s_m7_sin[(i + 64) & M7_SIN_MASK];
}

const int32_t *snes_sin_lut(void) { m7_build_lut_once(); return s_m7_sin; }
const int32_t *snes_cos_lut(void) { m7_build_lut_once(); return s_m7_cos; }

void snes_mode7_render(uint32_t *fb, uint32_t *ovl, uint32_t fb_w, uint32_t fb_h,
                       const snes_mode7_t *m7,
                       int authentic)
{
    (void)ovl;
    m7_build_lut_once();

    uint32_t x0, y0, rw, rh;
    compute_rect(authentic, fb_w, fb_h, &x0, &y0, &rw, &rh);

    /* Skip the sky region entirely — caller paints it. Floor starts at
     * horizon (relative to the render rect). */
    uint32_t horizon = m7->horizon < rh ? m7->horizon : rh;
    uint32_t floor_h = rh - horizon;
    uint32_t m7_h    = floor_h / 2;   /* half-vertical-res, line-doubled */

    int32_t half_w  = (int32_t)rw / 2;
    int32_t cam_x   = m7->cam_x;
    int32_t cam_y   = m7->cam_y;
    int32_t base_a  = m7->angle;
    int32_t twist   = m7->twist;
    int32_t space_z = m7->space_z ? m7->space_z : 8000;

    for (uint32_t sy = 0; sy < m7_h; sy++) {
        int32_t p   = (int32_t)(sy * 2 + 1);
        int32_t lam = space_z / p;

        /* Per-scanline angle = base + (lam >> 2) * twist. twist=0 →
         * flat camera (rigid affine rotation). twist=1 → classic SNES
         * vortex swirl. Higher values give wilder spirals. */
        int32_t angle = (base_a + ((lam >> 2) * twist)) & M7_SIN_MASK;
        int32_t ca = s_m7_cos[angle];
        int32_t sa = s_m7_sin[angle];

        int32_t du = ((-sa) * lam / (int32_t)rw) >> (M7_FP_SIN - M7_FP_UV);
        int32_t dv = (( ca) * lam / (int32_t)rw) >> (M7_FP_SIN - M7_FP_UV);
        int32_t cx = cam_x + ((ca * lam) >> M7_FP_SIN);
        int32_t cy = cam_y + ((sa * lam) >> M7_FP_SIN);
        int32_t u  = (cx << M7_FP_UV) - du * half_w;
        int32_t v  = (cy << M7_FP_UV) - dv * half_w;

        uint32_t screen_y = y0 + horizon + sy * 2;
        if (screen_y + 1 >= y0 + rh) break;

        uint32_t *row = fb + screen_y * fb_w + x0;
        mode7_scanline(row, m7->map, m7->palette, u, v, du, dv,
                       rw, m7->map_w_bits, m7->map_mask);
        /* Vertical line-doubling via NEON memcpy — ~1.5 µs/row vs
         * ~35 µs for a scalar copy. */
        memcpy_neon(row + fb_w, row, rw * 4);
    }
}
