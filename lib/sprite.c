/*
 * Jupiter SDK — sprite.c
 * Sprite blitting with screen clipping.
 *
 * Architecture: C owns the clipping math and row dispatch.
 * NEON assembly (sprite_neon.S) owns the pixel loops.
 *
 * Three blitters:
 *   sprite_blit()       — alpha-key transparency (binary on/off)
 *   sprite_blit_flip()  — alpha-key, horizontally mirrored
 *   sprite_blit_blend() — premultiplied alpha blend (translucency)
 *
 * Sprite format for blit/flip: ARGB8888 alpha-key.
 *   0x00?????? = transparent  (alpha byte zero → skipped)
 *   0xFFrrggbb = opaque       (any non-zero alpha → written)
 *
 * Sprite format for blend: premultiplied ARGB8888.
 *   RGB channels pre-multiplied by alpha at asset time.
 *   Blend: result = src + dst × (1 - src_alpha)
 */
#include "jupiter.h"

/* ---- ASM inner loops (sprite_neon.S) ---- */
extern void _sprite_row_keyed(const uint32_t *src, uint32_t *dst,
                               uint32_t count);
extern void _sprite_row_keyed_flip(const uint32_t *src_end, uint32_t *dst,
                                    uint32_t count);
extern void _sprite_row_blend(const uint32_t *src, uint32_t *dst,
                               uint32_t count);
extern void _sprite_row_scaled(const uint32_t *src_row, uint32_t *dst,
                                uint32_t count, uint32_t u_fp, uint32_t du_fp);
extern void _sprite_row_indexed(const uint8_t *src_row, uint32_t *dst,
                                 const uint32_t *palette, uint32_t count);
extern void _sprite_row_rotscale(const uint32_t *src, uint32_t *dst,
                                  uint32_t count, int32_t u_fp, int32_t v_fp,
                                  int32_t du, int32_t dv,
                                  uint32_t src_w, uint32_t src_h);


/* ---- Alpha-key blit: clipped, no flip ---- */

void sprite_blit(volatile uint32_t *dst, uint32_t dst_pitch,
                 const uint32_t *src, uint32_t src_w, uint32_t src_h,
                 int dx, int dy)
{
    int sx0 = 0, sy0 = 0;
    int sx1 = (int)src_w, sy1 = (int)src_h;

    if (dx < 0)                { sx0 = -dx;           dx = 0; }
    if (dy < 0)                { sy0 = -dy;           dy = 0; }
    if (dx + sx1 > (int)LCD_W) sx1 = (int)LCD_W - dx;
    if (dy + sy1 > (int)LCD_H) sy1 = (int)LCD_H - dy;

    if (sx0 >= sx1 || sy0 >= sy1) return;

    int vis_w = sx1 - sx0;

    for (int r = sy0; r < sy1; r++) {
        const uint32_t *srow = src + r * src_w + sx0;
        uint32_t *drow = (uint32_t *)dst + (dy + r - sy0) * dst_pitch + dx;
        _sprite_row_keyed(srow, drow, (uint32_t)vis_w);
    }
}


/* ---- Alpha-key blit with horizontal flip ---- */

void sprite_blit_flip(volatile uint32_t *dst, uint32_t dst_pitch,
                      const uint32_t *src, uint32_t src_w, uint32_t src_h,
                      int dx, int dy)
{
    int sx0 = 0, sy0 = 0;
    int sx1 = (int)src_w, sy1 = (int)src_h;

    if (dx < 0)                { sx0 = -dx;           dx = 0; }
    if (dy < 0)                { sy0 = -dy;           dy = 0; }
    if (dx + sx1 > (int)LCD_W) sx1 = (int)LCD_W - dx;
    if (dy + sy1 > (int)LCD_H) sy1 = (int)LCD_H - dy;

    if (sx0 >= sx1 || sy0 >= sy1) return;

    int vis_w = sx1 - sx0;

    for (int r = sy0; r < sy1; r++) {
        /* Point to the rightmost visible pixel in this source row.
         * The ASM inner loop reads backward from here. */
        const uint32_t *src_end = src + r * src_w + ((int)src_w - 1 - sx0);
        uint32_t *drow = (uint32_t *)dst + (dy + r - sy0) * dst_pitch + dx;
        _sprite_row_keyed_flip(src_end, drow, (uint32_t)vis_w);
    }
}


/* ---- Premultiplied alpha blend ---- */

void sprite_blit_blend(volatile uint32_t *dst, uint32_t dst_pitch,
                       const uint32_t *src, uint32_t src_w, uint32_t src_h,
                       int dx, int dy)
{
    int sx0 = 0, sy0 = 0;
    int sx1 = (int)src_w, sy1 = (int)src_h;

    if (dx < 0)                { sx0 = -dx;           dx = 0; }
    if (dy < 0)                { sy0 = -dy;           dy = 0; }
    if (dx + sx1 > (int)LCD_W) sx1 = (int)LCD_W - dx;
    if (dy + sy1 > (int)LCD_H) sy1 = (int)LCD_H - dy;

    if (sx0 >= sx1 || sy0 >= sy1) return;

    int vis_w = sx1 - sx0;

    for (int r = sy0; r < sy1; r++) {
        const uint32_t *srow = src + r * src_w + sx0;
        uint32_t *drow = (uint32_t *)dst + (dy + r - sy0) * dst_pitch + dx;
        _sprite_row_blend(srow, drow, (uint32_t)vis_w);
    }
}


/* ---- Nearest-neighbor scaled blit: alpha-key ---- */

void sprite_blit_scaled(volatile uint32_t *dst, uint32_t dst_pitch,
                        const uint32_t *src, uint32_t src_w, uint32_t src_h,
                        int dx, int dy,
                        uint32_t dst_w, uint32_t dst_h)
{
    if (dst_w == 0 || dst_h == 0) return;

    /* UV step in 16.16 fixed point.
     * u_step = source pixels per destination pixel. */
    uint32_t du = ((src_w << 16) + (dst_w >> 1)) / dst_w;  /* rounded */
    uint32_t dv = ((src_h << 16) + (dst_h >> 1)) / dst_h;

    /* Destination-space clipping, mapped back to UV start offsets.
     * If the left edge is off-screen by N dest pixels, we skip
     * N×du source texels. Same for top edge with V. */
    uint32_t u_start = 0;
    uint32_t v_start = 0;
    int vis_x0 = dx;
    int vis_y0 = dy;
    int vis_x1 = dx + (int)dst_w;
    int vis_y1 = dy + (int)dst_h;

    /* Left clip */
    if (vis_x0 < 0) {
        u_start = (uint32_t)(-vis_x0) * du;
        vis_x0 = 0;
    }
    /* Top clip */
    if (vis_y0 < 0) {
        v_start = (uint32_t)(-vis_y0) * dv;
        vis_y0 = 0;
    }
    /* Right clip */
    if (vis_x1 > (int)LCD_W) vis_x1 = (int)LCD_W;
    /* Bottom clip */
    if (vis_y1 > (int)LCD_H) vis_y1 = (int)LCD_H;

    int vis_w = vis_x1 - vis_x0;
    int vis_h = vis_y1 - vis_y0;
    if (vis_w <= 0 || vis_h <= 0) return;

    uint32_t v_fp = v_start;

    for (int r = 0; r < vis_h; r++) {
        /* Source row: nearest-neighbor V sampling */
        uint32_t sy = v_fp >> 16;
        if (sy >= src_h) break;   /* safety clamp */

        const uint32_t *srow = src + sy * src_w;
        uint32_t *drow = (uint32_t *)dst + (vis_y0 + r) * dst_pitch + vis_x0;

        _sprite_row_scaled(srow, drow, (uint32_t)vis_w, u_start, du);
        v_fp += dv;
    }
}


/* ---- Indexed palette sprite blit: 8-bit source + LUT ---- */

void sprite_blit_indexed(volatile uint32_t *dst, uint32_t dst_pitch,
                         const uint8_t *src, uint32_t src_w, uint32_t src_h,
                         const uint32_t *palette,
                         int dx, int dy)
{
    int sx0 = 0, sy0 = 0;
    int sx1 = (int)src_w, sy1 = (int)src_h;

    if (dx < 0)                { sx0 = -dx;           dx = 0; }
    if (dy < 0)                { sy0 = -dy;           dy = 0; }
    if (dx + sx1 > (int)LCD_W) sx1 = (int)LCD_W - dx;
    if (dy + sy1 > (int)LCD_H) sy1 = (int)LCD_H - dy;

    if (sx0 >= sx1 || sy0 >= sy1) return;

    int vis_w = sx1 - sx0;

    for (int r = sy0; r < sy1; r++) {
        const uint8_t *srow = src + r * src_w + sx0;
        uint32_t *drow = (uint32_t *)dst + (dy + r - sy0) * dst_pitch + dx;
        _sprite_row_indexed(srow, drow, palette, (uint32_t)vis_w);
    }
}


/* ---- Built-in sin/cos table for rotscale (256 entries, 16.16 FP) ---- */

/* Parabolic sine approximation: same math as the examples' LUT,
 * but stored at 16.16 precision for UV transforms. sin[0]=0,
 * sin[64]=+65536 (1.0), sin[128]=0, sin[192]=-65536 (-1.0).
 * cos[i] = sin[(i+64) & 255].
 * Computed once at first use. */
static int32_t _rs_sin[256];
static int32_t _rs_cos[256];
static int _rs_lut_ready = 0;

static void _rs_init_lut(void)
{
    if (_rs_lut_ready) return;
    for (int i = 0; i < 256; i++) {
        int j = i & 127;
        int32_t val = (4 * j * (128 - j));  /* 0..16384 parabola */
        val = (val << 16) / 16384;           /* normalize to 16.16 */
        if (i >= 128) val = -val;
        _rs_sin[i] = val;
    }
    for (int i = 0; i < 256; i++)
        _rs_cos[i] = _rs_sin[(i + 64) & 255];
    _rs_lut_ready = 1;
}


/* ---- Rotation + scale sprite blit ---- */

void sprite_blit_rotscale(volatile uint32_t *dst, uint32_t dst_pitch,
                          const uint32_t *src, uint32_t src_w, uint32_t src_h,
                          int cx, int cy,
                          uint8_t angle, uint32_t scale_fp)
{
    _rs_init_lut();

    int32_t ca = _rs_cos[angle];    /* 16.16 fixed point */
    int32_t sa = _rs_sin[angle];

    /* Scale the cos/sin by the scale factor.
     * scale_fp is 16.16: 0x10000 = 1.0, 0x8000 = 0.5, 0x20000 = 2.0.
     * After multiply: (ca * scale) >> 16 gives scaled cos in 16.16. */
    int32_t sca = (int32_t)(((int64_t)ca * scale_fp) >> 16);
    int32_t ssa = (int32_t)(((int64_t)sa * scale_fp) >> 16);

    /* Compute bounding box of the rotated sprite in dest space.
     * The 4 source corners map to:
     *   dest_x = cx + (src_x - src_w/2) * cos - (src_y - src_h/2) * sin
     *   dest_y = cy + (src_x - src_w/2) * sin + (src_y - src_h/2) * cos
     *
     * We check all 4 corners and find the min/max to get the bbox. */
    int32_t half_w = (int32_t)src_w / 2;
    int32_t half_h = (int32_t)src_h / 2;

    /* 4 corners at (±half_w, ±half_h), mapped through forward transform */
    int32_t c0x = (-half_w * sca - (-half_h) * ssa) >> 16;
    int32_t c1x = ( half_w * sca - (-half_h) * ssa) >> 16;
    int32_t c2x = (-half_w * sca - ( half_h) * ssa) >> 16;
    int32_t c3x = ( half_w * sca - ( half_h) * ssa) >> 16;
    int32_t c0y = (-half_w * ssa + (-half_h) * sca) >> 16;
    int32_t c1y = ( half_w * ssa + (-half_h) * sca) >> 16;
    int32_t c2y = (-half_w * ssa + ( half_h) * sca) >> 16;
    int32_t c3y = ( half_w * ssa + ( half_h) * sca) >> 16;

    /* Forward bbox: min/max of corners */
    int32_t min_x = c0x, max_x = c0x;
    if (c1x < min_x) min_x = c1x;
    if (c1x > max_x) max_x = c1x;
    if (c2x < min_x) min_x = c2x;
    if (c2x > max_x) max_x = c2x;
    if (c3x < min_x) min_x = c3x;
    if (c3x > max_x) max_x = c3x;
    int32_t min_y = c0y, max_y = c0y;
    if (c1y < min_y) min_y = c1y;
    if (c1y > max_y) max_y = c1y;
    if (c2y < min_y) min_y = c2y;
    if (c2y > max_y) max_y = c2y;
    if (c3y < min_y) min_y = c3y;
    if (c3y > max_y) max_y = c3y;

    int dest_x0 = cx + (int)min_x - 1;  /* -1 for safety margin */
    int dest_y0 = cy + (int)min_y - 1;
    int dest_x1 = cx + (int)max_x + 2;
    int dest_y1 = cy + (int)max_y + 2;

    /* Clip to screen */
    if (dest_x0 < 0) dest_x0 = 0;
    if (dest_y0 < 0) dest_y0 = 0;
    if (dest_x1 > (int)LCD_W) dest_x1 = (int)LCD_W;
    if (dest_y1 > (int)LCD_H) dest_y1 = (int)LCD_H;

    int vis_w = dest_x1 - dest_x0;
    int vis_h = dest_y1 - dest_y0;
    if (vis_w <= 0 || vis_h <= 0) return;

    /* Inverse transform: map dest pixel back to source.
     *
     * For rotation by angle θ at scale s:
     *   src_u = center_u + (dest_dx * cos(θ) + dest_dy * sin(θ)) / s
     *   src_v = center_v + (-dest_dx * sin(θ) + dest_dy * cos(θ)) / s
     *
     * Per-pixel UV steps:
     *   du/dx =  cos(θ) / s
     *   dv/dx = -sin(θ) / s
     *   du/dy =  sin(θ) / s
     *   dv/dy =  cos(θ) / s
     *
     * To stay 32-bit: compute (ca << 8) / (scale_fp >> 8).
     * ca is 16.16, max |65536|. Shifted left 8 = max 16M. Fits int32.
     * scale_fp >> 8 loses 8 bits of fraction — negligible for sprites.
     * Result is 16.16 fixed point.
     */
    int32_t scale8 = (int32_t)(scale_fp >> 8);
    if (scale8 == 0) return;

    int32_t du_x =  (ca << 8) / scale8;
    int32_t dv_x = -(sa << 8) / scale8;
    int32_t du_y =  (sa << 8) / scale8;
    int32_t dv_y =  (ca << 8) / scale8;

    /* UV at top-left corner of dest bbox, mapped to source space.
     * Using the same shift trick to stay 32-bit:
     *   dx_off * (ca >> 8) is at most ~480 * 256 = 122880
     *   Two terms summed ≤ 245760.  × 256 = ~63M. Fits int32. */
    int32_t dx_off = dest_x0 - cx;
    int32_t dy_off = dest_y0 - cy;

    int32_t ca8 = ca >> 8;
    int32_t sa8 = sa >> 8;
    int32_t u_row = (dx_off * ca8 + dy_off * sa8) * 256 / scale8
                    + (half_w << 16);
    int32_t v_row = (-dx_off * sa8 + dy_off * ca8) * 256 / scale8
                    + (half_h << 16);

    for (int r = 0; r < vis_h; r++) {
        uint32_t *drow = (uint32_t *)dst + (dest_y0 + r) * dst_pitch + dest_x0;

        _sprite_row_rotscale(src, drow, (uint32_t)vis_w,
                             u_row, v_row, du_x, dv_x,
                             src_w, src_h);

        u_row += du_y;
        v_row += dv_y;
    }
}


/* ---- Clear a sprite-sized rect (for erasing before redraw) ---- */

void sprite_clear(volatile uint32_t *dst, uint32_t dst_pitch,
                  uint32_t src_w, uint32_t src_h,
                  int dx, int dy)
{
    int x0 = dx, y0 = dy;
    int x1 = dx + (int)src_w, y1 = dy + (int)src_h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)LCD_W) x1 = (int)LCD_W;
    if (y1 > (int)LCD_H) y1 = (int)LCD_H;
    if (x0 >= x1 || y0 >= y1) return;

    int w = x1 - x0;
    for (int r = y0; r < y1; r++) {
        volatile uint32_t *row = dst + r * dst_pitch + x0;
        for (int c = 0; c < w; c++)
            row[c] = 0x00000000;
    }
}
