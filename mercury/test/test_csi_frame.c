/*
 * Host test: csi_frame_to_argb() recovers the Pico's framebuffer from a
 * captured 640x480 CSI frame.
 *
 * Builds the frame the way the link produces it — dvi_out.c's 320x240
 * canvas (framebuffer centred, black around it) shown at 2x, carried as
 * CSI-2 RGB888 (B, G, R bytes) or UYVY (BT.601 limited, one U/V pair
 * per 2x-wide canvas pixel) — then checks every recovered pixel, for
 * each Pico resolution SET_RES allows in practice.
 *
 * Built twice by the Makefile: RGB888 (exact match, plus the R/B swap
 * path) and -DMERCURY_CSI_UYVY (within YCbCr rounding).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../v3s_side/mercury_csi_capture.h"

static uint8_t frame[MERCURY_CSI_FRAME_BYTES];

#ifdef MERCURY_CSI_UYVY
static int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
#endif

static void put_pixel(int fx, int fy, uint32_t rgb)
{
    int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    uint8_t *p = frame + fy * MERCURY_CSI_LINE_BYTES;
#ifdef MERCURY_CSI_UYVY
    /* fx is even: write the whole U Y V Y pair for fx, fx+1 */
    int y = clamp8(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
    int u = clamp8(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
    int v = clamp8(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
    p += (fx / 2) * 4;
    p[0] = (uint8_t)u; p[1] = (uint8_t)y; p[2] = (uint8_t)v; p[3] = (uint8_t)y;
#else
    p += fx * 3;
    p[0] = (uint8_t)b; p[1] = (uint8_t)g; p[2] = (uint8_t)r;
    p[3] = (uint8_t)b; p[4] = (uint8_t)g; p[5] = (uint8_t)r;
#endif
}

static int run(uint32_t w, uint32_t h, int swap)
{
    static uint32_t fb[320 * 240], out[320 * 240];
    for (uint32_t i = 0; i < w * h; i++)
        fb[i] = (uint32_t)rand() & 0xFFFFFF;

    /* PicoDVI: 320x240 canvas, fb centred, each pixel a 2x2 block */
    memset(frame, 0x5A, sizeof frame);  /* poison: catch stray reads */
    uint32_t xo = (320 - w) / 2, yo = (240 - h) / 2;
    for (uint32_t cy = 0; cy < 240; cy++)
        for (uint32_t cx = 0; cx < 320; cx++) {
            uint32_t c = 0;
            if (cx >= xo && cx < xo + w && cy >= yo && cy < yo + h)
                c = fb[(cy - yo) * w + (cx - xo)];
            put_pixel(2 * cx, 2 * cy, c);
            put_pixel(2 * cx, 2 * cy + 1, c);
        }

    csi_frame_to_argb(out, frame, w, h, swap);

    int bad = 0, tol = 0;
#ifdef MERCURY_CSI_UYVY
    tol = 4;            /* YCbCr round trip */
#endif
    for (uint32_t i = 0; i < w * h; i++) {
        uint32_t want = fb[i], got = out[i];
        if (swap)
            want = (want & 0x00FF00) | ((want >> 16) & 255) | ((want & 255) << 16);
        if ((got >> 24) != 0xFF) { bad++; continue; }
        for (int sh = 0; sh <= 16; sh += 8) {
            int d = (int)((got >> sh) & 255) - (int)((want >> sh) & 255);
            if (d < -tol || d > tol) { bad++; break; }
        }
    }
    printf("  %3ux%-3u swap=%d: %s (%d bad)\n", w, h, swap,
           bad ? "FAIL" : "ok", bad);
    return bad != 0;
}

int main(void)
{
    static const uint32_t res[][2] = {
        {320, 224}, {320, 240}, {256, 224}, {160, 144}, {32, 32},
    };
    int fails = 0;
#ifdef MERCURY_CSI_UYVY
    printf("csi_frame_to_argb, UYVY:\n");
#else
    printf("csi_frame_to_argb, RGB888:\n");
#endif
    for (unsigned i = 0; i < sizeof res / sizeof res[0]; i++) {
        fails += run(res[i][0], res[i][1], 0);
#ifndef MERCURY_CSI_UYVY
        fails += run(res[i][0], res[i][1], 1);
#endif
    }
    printf(fails ? "FAILED\n" : "all passed\n");
    return fails != 0;
}
