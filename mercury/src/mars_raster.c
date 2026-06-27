/*
 * Mercury — Flat-shaded triangle rasterizer
 *
 * Scanline-based, integer-only math, no division in inner loop.
 * Designed for Cortex-M0+ at 133MHz rendering Star Fox-class geometry.
 */

#include <string.h>
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
void j32x_raster_tri(j32x_vertex_t v0, j32x_vertex_t v1, j32x_vertex_t v2,
                      uint8_t color)
{
    /* Sort by Y ascending */
    j32x_vertex_t tmp;
    if (v0.y > v1.y) { tmp = v0; v0 = v1; v1 = tmp; }
    if (v0.y > v2.y) { tmp = v0; v0 = v2; v2 = tmp; }
    if (v1.y > v2.y) { tmp = v1; v1 = v2; v2 = tmp; }

    int y0 = v0.y, y1 = v1.y, y2 = v2.y;
    if (y0 == y2) return; /* degenerate */

    /* 16.16 fixed-point edge stepping */
    int32_t x_long;     /* x along v0→v2 (the long edge) */
    int32_t x_short;    /* x along v0→v1 then v1→v2 */
    int32_t dx_long, dx_short;

    /* Top half: v0 → v1 (short), v0 → v2 (long) */
    int dy_long = y2 - y0;
    int dy_top  = y1 - y0;

    dx_long = ((v2.x - v0.x) << 16) / dy_long;

    x_long  = v0.x << 16;

    if (dy_top > 0) {
        dx_short = ((v1.x - v0.x) << 16) / dy_top;
        x_short  = v0.x << 16;

        for (int y = y0; y < y1; y++) {
            int xa = x_long >> 16;
            int xb = x_short >> 16;
            j32x_raster_hline(xa, xb, y, color);
            x_long  += dx_long;
            x_short += dx_short;
        }
    } else {
        x_long = v0.x << 16;
        x_long += dx_long * dy_top; /* skip to v1.y */
    }

    /* Bottom half: v1 → v2 (short), continuing v0 → v2 (long) */
    int dy_bot = y2 - y1;
    if (dy_bot > 0) {
        dx_short = ((v2.x - v1.x) << 16) / dy_bot;
        x_short  = v1.x << 16;

        for (int y = y1; y < y2; y++) {
            int xa = x_long >> 16;
            int xb = x_short >> 16;
            j32x_raster_hline(xa, xb, y, color);
            x_long  += dx_long;
            x_short += dx_short;
        }
    }

    g_state.tri_count++;
}

void j32x_swap(void)
{
    uint8_t tmp = g_state.front;
    g_state.front = g_state.back;
    g_state.back = tmp;
    __dmb();  /* ensure Core 1 sees the swap */
    g_state.tri_count = 0;
}
