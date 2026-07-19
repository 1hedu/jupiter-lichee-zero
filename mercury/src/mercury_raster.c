/*
 * Mercury — Flat-shaded triangle rasterizer
 *
 * Scanline-based, integer-only math, no division in inner loop.
 * Designed for Cortex-M0+ at 133MHz rendering Star Fox-class geometry.
 */

#include <string.h>
#include "hardware/sync.h"   /* __dmb() */
#include "jupiter32x.h"

j32x_state_t g_state;

static inline uint8_t *backbuf(void) { return g_state.fb[g_state.back]; }
static inline uint16_t fb_w(void) { return g_state.width ? g_state.width : J32X_WIDTH; }
static inline uint16_t fb_h(void) { return g_state.height ? g_state.height : J32X_HEIGHT; }

void j32x_raster_clear(uint8_t color)
{
    memset(backbuf(), color, fb_w() * fb_h());
}

void j32x_raster_hline(int x0, int x1, int y, uint8_t color)
{
    uint16_t w = fb_w(), h = fb_h();
    if (y < 0 || y >= h) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (x0 > x1) return;

    uint8_t *row = &backbuf()[y * w];
    memset(&row[x0], color, x1 - x0 + 1);
}

void j32x_raster_line(j32x_vertex_t v0, j32x_vertex_t v1, uint8_t color)
{
    uint16_t w = fb_w(), h = fb_h();
    /* clamp so a garbled list can't walk Bresenham for 65k steps */
    if (v0.x < J32X_COORD_MIN) v0.x = J32X_COORD_MIN;
    if (v0.x > J32X_COORD_MAX) v0.x = J32X_COORD_MAX;
    if (v0.y < J32X_COORD_MIN) v0.y = J32X_COORD_MIN;
    if (v0.y > J32X_COORD_MAX) v0.y = J32X_COORD_MAX;
    if (v1.x < J32X_COORD_MIN) v1.x = J32X_COORD_MIN;
    if (v1.x > J32X_COORD_MAX) v1.x = J32X_COORD_MAX;
    if (v1.y < J32X_COORD_MIN) v1.y = J32X_COORD_MIN;
    if (v1.y > J32X_COORD_MAX) v1.y = J32X_COORD_MAX;
    int dx = v1.x - v0.x;
    int dy = v1.y - v0.y;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int x = v0.x, y = v0.y;
    int err = dx - dy;

    for (;;) {
        if (x >= 0 && x < w && y >= 0 && y < h)
            backbuf()[y * w + x] = color;
        if (x == v1.x && y == v1.y) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
}

/*
 * Flat-shaded triangle — scanline rasterizer
 *
 * Sort vertices by Y, walk left/right edges with 16.16 fixed-point
 * x-stepping, fill horizontal spans. Zero divisions in the inner loop.
 */
/* Clamp a coordinate so 16.16 edge math can't overflow int32 and a
 * garbled display list can't send the scanline walk to y=±32767. */
static inline int16_t cclamp(int16_t v)
{
    if (v < J32X_COORD_MIN) return J32X_COORD_MIN;
    if (v > J32X_COORD_MAX) return J32X_COORD_MAX;
    return v;
}

void j32x_raster_tri(j32x_vertex_t v0, j32x_vertex_t v1, j32x_vertex_t v2,
                      uint8_t color)
{
    v0.x = cclamp(v0.x); v0.y = cclamp(v0.y);
    v1.x = cclamp(v1.x); v1.y = cclamp(v1.y);
    v2.x = cclamp(v2.x); v2.y = cclamp(v2.y);

    /* Sort by Y ascending */
    j32x_vertex_t tmp;
    if (v0.y > v1.y) { tmp = v0; v0 = v1; v1 = tmp; }
    if (v0.y > v2.y) { tmp = v0; v0 = v2; v2 = tmp; }
    if (v1.y > v2.y) { tmp = v1; v1 = v2; v2 = tmp; }

    int y0 = v0.y, y1 = v1.y, y2 = v2.y;
    int h = fb_h();
    if (y0 == y2) return;            /* degenerate  */
    if (y2 <= 0 || y0 >= h) return;  /* fully offscreen */

    /* 16.16 fixed-point edge stepping (coords are clamped to ±2048,
     * so (dx << 16) stays well inside int32) */
    int32_t x_long;     /* x along v0→v2 (the long edge) */
    int32_t x_short;    /* x along v0→v1 then v1→v2 */
    int32_t dx_long, dx_short;

    int dy_long = y2 - y0;
    int dy_top  = y1 - y0;

    dx_long = (int32_t)(v2.x - v0.x) * 65536 / dy_long;

    /* Both halves re-derive the long edge from the v0 base so clipping
     * one half can never leave the edge misplaced for the other
     * (repeated += of dx equals the multiply exactly). */

    /* Top half: v0 → v1 (short), v0 → v2 (long), rows clipped to [0,h) */
    if (dy_top > 0) {
        dx_short = (int32_t)(v1.x - v0.x) * 65536 / dy_top;

        int ys = y0 < 0 ? 0 : y0;
        int ye = y1 > h ? h : y1;
        x_long  = (int32_t)v0.x * 65536 + dx_long  * (ys - y0);
        x_short = (int32_t)v0.x * 65536 + dx_short * (ys - y0);

        for (int y = ys; y < ye; y++) {
            j32x_raster_hline(x_long >> 16, x_short >> 16, y, color);
            x_long  += dx_long;
            x_short += dx_short;
        }
    }

    /* Bottom half: v1 → v2 (short), continuing v0 → v2 (long) */
    int dy_bot = y2 - y1;
    if (dy_bot > 0) {
        dx_short = (int32_t)(v2.x - v1.x) * 65536 / dy_bot;

        int ys = y1 < 0 ? 0 : y1;
        int ye = y2 > h ? h : y2;
        x_long  = (int32_t)v0.x * 65536 + dx_long  * (ys - y0);
        x_short = (int32_t)v1.x * 65536 + dx_short * (ys - y1);

        for (int y = ys; y < ye; y++) {
            j32x_raster_hline(x_long >> 16, x_short >> 16, y, color);
            x_long  += dx_long;
            x_short += dx_short;
        }
    }

    g_state.tri_count++;
}

void j32x_swap(void)
{
    /* Don't flip front/back here: core 1 may be mid-scanout of the
     * front buffer, and an instant flip would hand it our half-drawn
     * back buffer (tear/garbage for a frame). Instead raise a latch
     * that the scanout loop consumes at its next frame boundary, and
     * wait for it — this is also the renderer's natural frame pacing
     * (blocks at most one scanout frame). */
    __dmb();                     /* back-buffer writes visible first */
    g_state.swap_pending = 1;
    while (g_state.swap_pending)
        ;                        /* core 1 flips front/back + clears */
    g_state.tri_count = 0;
}

/* Called by the scanout loop (core 1) at each frame boundary. */
void j32x_swap_apply(void)
{
    if (!g_state.swap_pending)
        return;
    uint8_t tmp = g_state.front;
    g_state.front = g_state.back;
    g_state.back = tmp;
    __dmb();
    g_state.swap_pending = 0;
}
