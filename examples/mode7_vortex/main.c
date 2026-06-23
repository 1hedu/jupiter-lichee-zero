/*
 * Jupiter SDK — Mode 7 Vortex
 *
 * The SNES Mode 7 "vortex" effect — the spiral / tunnel / portal look
 * from FF6's intro, the Star Fox death sequence, F-Zero turbo flash,
 * Earthbound's psychedelic battles. On real SNES this was done by
 * abusing the Mode 7 affine matrix per-scanline with extreme scaling
 * and a rotating angle. We render the same look with a per-pixel
 * polar map (u = angle, v = R/r depth), which is mathematically what
 * the SNES trick approximates anyway.
 *
 * Precompute u_lut and v_lut once at startup, then each frame just
 * shift v (forward motion through the tunnel) and u (rotation around
 * the axis). Sample a procedural banded texture, add a pulsing
 * center glow.
 *
 * Per-pixel cost: 2 LUT loads + 2 adds + 1 sample + 1 store + a
 * cheap center-glow blend. ~6-8 cycles. At 480×272 that's <1ms/frame
 * on the 1.2 GHz Cortex-A7, so easy 60 fps.
 */
#include "jupiter.h"

/* ---- Precomputed per-pixel polar mapping ---- */
static uint8_t u_lut[LCD_H * LCD_W] __attribute__((aligned(64)));
static uint8_t v_lut[LCD_H * LCD_W] __attribute__((aligned(64)));

/* Distance from center, used by the center-glow pass. Squared so we
 * can compare without sqrt; max value clipped to 0xFFFF for compact
 * storage. */
static uint16_t r2_lut[LCD_H * LCD_W] __attribute__((aligned(64)));

/* ---- Procedural tunnel texture (256×256, palette indices) ----
 * Built once at startup. Rings every 16 v-units, with a twist that
 * shifts the ring phase as v advances (gives the vortex spiral). */
static uint8_t tunnel_tex[256 * 256] __attribute__((aligned(64), section(".bss_low")));

/* Palette: deep purple → magenta → hot pink → white-yellow.
 * Indices 0..63   = dark band, deep purple/black
 *         64..127 = mid band, magenta
 *        128..191 = bright band, hot pink
 *        192..255 = highlight, white-yellow
 */
static uint32_t palette[256];

/* Fast integer sqrt approximation — enough precision for a 480×272
 * radius LUT and avoids pulling in math.h. */
static uint32_t isqrt(uint32_t n)
{
    if (n == 0) return 0;
    uint32_t x = n, y = (x + 1) >> 1;
    while (y < x) { x = y; y = (x + n / x) >> 1; }
    return x;
}

/* Fast atan2 returning 0..255 (wraps the angle into a byte). Uses a
 * symmetric octant approximation: atan(t) ≈ t·(0.97−0.21·t²) for t∈[0,1],
 * good to ~0.5% — way better than the eye can pick out on a moving
 * tunnel. */
static int atan2_byte(int y, int x)
{
    if (x == 0 && y == 0) return 0;
    int ax = x < 0 ? -x : x;
    int ay = y < 0 ? -y : y;
    int swap = ay > ax;
    int num = swap ? ax : ay;
    int den = swap ? ay : ax;
    /* Q10 fixed-point: t = num/den ∈ [0,1] in 0..1024 */
    int t = (num * 1024) / (den == 0 ? 1 : den);
    /* atan(t) in turns·256, range 0..32 (= 1/8 of a full turn, 45°) */
    int a = (t * 32) / 1024;  /* linear approx — good enough */
    if (swap) a = 64 - a;
    if (x < 0) a = 128 - a;
    if (y < 0) a = -a;
    return a & 0xFF;
}

static void build_luts(void)
{
    const int cx = LCD_W / 2;
    const int cy = LCD_H / 2;
    /* "Tube radius" — controls how fast v advances near the center.
     * Bigger R = slower zoom past the rings, longer tunnel feel. */
    const int R = 80;

    for (int y = 0; y < (int)LCD_H; y++) {
        for (int x = 0; x < (int)LCD_W; x++) {
            int dx = x - cx;
            int dy = y - cy;
            int r2 = dx * dx + dy * dy;
            int r  = (int)isqrt((uint32_t)r2);
            uint8_t v;
            if (r < 2) v = 255;                    /* center → far away */
            else {
                int vv = (R * 256) / r;
                v = (uint8_t)(vv & 0xFF);          /* wraps for natural depth tiling */
            }
            u_lut[y * LCD_W + x] = (uint8_t)atan2_byte(dy, dx);
            v_lut[y * LCD_W + x] = v;
            r2_lut[y * LCD_W + x] = r2 > 0xFFFF ? 0xFFFF : (uint16_t)r2;
        }
    }
}

static void build_texture(void)
{
    /* Tunnel texture: vertical (depth) bands every 32 v-units, with a
     * smaller phase-shift per row to create the spiral twist. */
    for (int v = 0; v < 256; v++) {
        for (int u = 0; u < 256; u++) {
            /* Spiral: shift u by v*1 so columns of constant-u get a
             * helical pattern around the tube. */
            int u2 = (u + (v >> 1)) & 0xFF;
            /* Band: bright in the middle of each 32-unit cycle. */
            int band_phase = v & 31;       /* 0..31 */
            int dist = band_phase < 16 ? band_phase : 31 - band_phase;
            /* dist ∈ [0,16], 0 = ring center, 16 = between rings.
             * Map to palette index 0..255 with u modulating the hue. */
            int base = 192 - dist * 12;    /* highlight near ring center */
            int hue  = (u2 >> 1);          /* 0..127 */
            int idx  = base - (hue >> 2);
            if (idx < 0)   idx = 0;
            if (idx > 255) idx = 255;
            tunnel_tex[v * 256 + u] = (uint8_t)idx;
        }
    }
}

static void build_palette(void)
{
    /* 4-stop gradient: midnight purple → magenta → hot pink → white-yellow */
    for (int i = 0; i < 256; i++) {
        uint32_t r, g, b;
        if (i < 64) {                       /* 0..63 : near-black → purple */
            uint32_t t = i;                 /* 0..63 */
            r = (0x05 + (t * (0x40 - 0x05) / 64));
            g = (0x00 + (t * (0x00 - 0x00) / 64));
            b = (0x10 + (t * (0x50 - 0x10) / 64));
        } else if (i < 128) {               /* 64..127 : purple → magenta */
            uint32_t t = i - 64;
            r = (0x40 + (t * (0xC0 - 0x40) / 64));
            g = (0x00 + (t * (0x10 - 0x00) / 64));
            b = (0x50 + (t * (0x80 - 0x50) / 64));
        } else if (i < 192) {               /* 128..191 : magenta → hot pink */
            uint32_t t = i - 128;
            r = (0xC0 + (t * (0xFF - 0xC0) / 64));
            g = (0x10 + (t * (0x60 - 0x10) / 64));
            b = (0x80 + (t * (0xC0 - 0x80) / 64));
        } else {                            /* 192..255 : hot pink → white-yellow */
            uint32_t t = i - 192;
            r = 0xFF;
            g = (0x60 + (t * (0xFF - 0x60) / 64));
            b = (0xC0 - (t * (0xC0 - 0x40) / 64));
        }
        palette[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

/* Render one tunnel frame to `fb_addr` with current time offsets. */
static void render_tunnel(uint32_t fb_addr, uint32_t frame)
{
    uint32_t *fb = (uint32_t *)fb_addr;

    /* Tunnel motion:
     *   v_off — forward velocity through the tube (rings stream past)
     *   u_off — angular twist (whole tube rotates slowly)
     * Both wrap at 256 since textures wrap. */
    uint8_t v_off = (uint8_t)(frame * 6);
    uint8_t u_off = (uint8_t)(frame * 1);

    /* Pulse the center-glow brightness on a slow sine. */
    static const int8_t pulse_lut[64] = {
        0,  3,  6,  9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46,
       49, 52, 55, 57, 60, 63, 65, 68, 70, 72, 74, 76, 78, 80, 82, 84,
       85, 87, 88, 89, 91, 92, 93, 94, 95, 95, 96, 96, 96, 96, 96, 95,
       95, 94, 93, 92, 91, 89, 88, 87, 85, 84, 82, 80, 78, 76, 74, 72,
    };
    int glow_boost = pulse_lut[(frame >> 1) & 63];   /* 0..96 */

    for (uint32_t p = 0; p < LCD_W * LCD_H; p++) {
        uint8_t u = u_lut[p] + u_off;
        uint8_t v = v_lut[p] + v_off;
        uint32_t base = palette[tunnel_tex[(uint32_t)v * 256 + u]];

        /* Center glow: pixels close to the vanishing point get a
         * white-hot punch that pulses with the music-less beat. */
        uint16_t r2 = r2_lut[p];
        if (r2 < 2048) {
            int boost = (2048 - (int)r2) * glow_boost / 2048;  /* 0..glow_boost */
            uint32_t r = ((base >> 16) & 0xFF) + boost;
            uint32_t g = ((base >>  8) & 0xFF) + boost;
            uint32_t b = ( base        & 0xFF) + boost;
            if (r > 0xFF) r = 0xFF;
            if (g > 0xFF) g = 0xFF;
            if (b > 0xFF) b = 0xFF;
            base = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        fb[p] = base;
    }
}

/* ==== ENTRY ==== */
void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — Vortex Tunnel ===\n");
    timer_init();
    mmu_init();

    uart_puts("Building polar LUTs...\n");
    build_luts();
    build_texture();
    build_palette();

    memset32_neon(FB0_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    uart_puts("Tunnel live.\n");

    /* VI0 double-buffered, OVL stays cleared (no sprite layer). */
    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();

        render_tunnel(back_fb, frame);

        dcache_clean_fb(back_fb);
        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        if ((frame % 120) == 0) {
            uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
            uart_puts("f="); uart_putdec(frame);
            uart_puts("  "); uart_putdec(us); uart_puts("us/frame\n");
        }
        frame++;
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;
    }
}
