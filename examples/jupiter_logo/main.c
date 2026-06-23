/*
 * Jupiter SDK — Jupiter Logo Animation
 *
 * Procedural planet Jupiter with orbiting light, cloud bands, Great Red
 * Spot, specular highlight, limb glow, and twinkling star field.
 *
 * Full 105px radius. No per-pixel divisions. No per-frame clear — planet
 * silhouette is fixed, every sphere pixel overwritten each frame. This
 * eliminates tearing without vsync: a mid-scan swap just shows two
 * slightly different lighting angles, which is invisible.
 *
 * ~55ms render, cinematic ~18fps. Butter smooth.
 */
#include "jupiter.h"

#define FP 10
#define FP_ONE (1 << FP)

static uint32_t isqrt(uint32_t n)
{
    uint32_t r = 0, b = 1U << 30;
    while (b > n) b >>= 2;
    while (b) {
        if (n >= r + b) { n -= r + b; r = (r >> 1) + b; }
        else { r >>= 1; }
        b >>= 2;
    }
    return r;
}

#define SIN_N    256
#define SIN_MASK 255
#define SIN_FP   12
static int32_t sin_lut[SIN_N], cos_lut[SIN_N];

static void init_sincos(void)
{
    for (int i = 0; i < SIN_N; i++) {
        int j = i & 127;
        int32_t v = (4 * j * (128 - j)) >> (14 - SIN_FP);
        sin_lut[i] = (i >= 128) ? -v : v;
    }
    for (int i = 0; i < SIN_N; i++)
        cos_lut[i] = sin_lut[(i + 64) & SIN_MASK];
}

/* ---- Full resolution geometry ---- */
#define PR     105
#define PR2    (PR * PR)
#define PD     (PR * 2 + 1)
#define PCX    (LCD_W / 2)
#define PCY    (LCD_H / 2 - 10)

#define NORM_SHIFT 20
#define NORM_INV   ((1 << NORM_SHIFT) / (PR << (SIN_FP - FP)))

static uint16_t nz_lut[PD * PD];
static uint16_t disc_half[PD];
static uint32_t lon_step[PD];

static void precompute_geometry(void)
{
    for (int dy = -PR; dy <= PR; dy++) {
        int dy2 = dy * dy;
        int rem = PR2 - dy2;
        uint16_t hw = (rem > 0) ? (uint16_t)isqrt((uint32_t)rem) : 0;
        disc_half[dy + PR] = hw;
        lon_step[dy + PR] = (hw > 0) ? (256U << 16) / (uint32_t)(hw * 2) : 0;

        for (int dx = -PR; dx <= PR; dx++) {
            int d2 = dx * dx + dy2;
            nz_lut[(dy + PR) * PD + (dx + PR)] =
                (d2 <= PR2) ? (uint16_t)isqrt((uint32_t)(PR2 - d2)) : 0;
        }
    }
}

#define LAT_STEPS 64
static uint32_t band_lut[LAT_STEPS];

static void precompute_bands(void)
{
    for (int i = 0; i < LAT_STEPS; i++) {
        int32_t alat = (i * FP_ONE) / LAT_STEPS;
        uint32_t c;
        if      (alat < FP_ONE *  8 / 100) c = 0x00EBD7AF;
        else if (alat < FP_ONE * 16 / 100) c = 0x00AA7346;
        else if (alat < FP_ONE * 24 / 100) c = 0x00E1CDA5;
        else if (alat < FP_ONE * 35 / 100) c = 0x00A07850;
        else if (alat < FP_ONE * 50 / 100) c = 0x00D7C8AA;
        else if (alat < FP_ONE * 62 / 100) c = 0x00AF916E;
        else if (alat < FP_ONE * 78 / 100) c = 0x00BEB4A5;
        else                                c = 0x009B9BAA;
        band_lut[i] = c;
    }
}

/* ---- Space ---- */
static uint32_t space_colors[LCD_H];

static void precompute_space(void)
{
    for (uint32_t y = 0; y < LCD_H; y++) {
        uint32_t t = (y * 256) / LCD_H;
        space_colors[y] = ((0x01 + (t * 0x03 >> 8)) << 16) |
                          ((0x01 + (t * 0x02 >> 8)) << 8)  |
                           (0x04 + (t * 0x08 >> 8));
    }
}

static void render_space_full(uint32_t *fb)
{
    for (uint32_t y = 0; y < LCD_H; y++) {
        uint32_t c = space_colors[y];
        uint32_t *row = fb + y * LCD_W;
        for (uint32_t x = 0; x < LCD_W; x++) row[x] = c;
    }
}

/* ---- Planet: full res, zero per-pixel divisions, no clear ---- */
static void render_planet(uint32_t *fb, uint32_t frame)
{
    int32_t la = frame;
    int32_t lx = cos_lut[la & SIN_MASK];
    int32_t lz = sin_lut[la & SIN_MASK];
    int32_t ly = sin_lut[(la / 3) & SIN_MASK] >> 2;
    int32_t lon_offset_fp16 = (int32_t)(frame / 2) << 16;

    int y0 = PCY - PR;
    int y1 = PCY + PR;
    if (y0 < 0) y0 = 0;
    if (y1 >= (int)LCD_H) y1 = LCD_H - 1;

    for (int y = y0; y <= y1; y++) {
        int dy = y - PCY;
        int dyi = dy + PR;
        uint32_t *row = fb + y * LCD_W;
        const uint16_t *nz_row = &nz_lut[dyi * PD + PR];
        int32_t hw = (int32_t)disc_half[dyi];
        if (hw == 0) continue;

        int32_t lat_fp = (dy * FP_ONE) / PR;
        int32_t alat = lat_fp < 0 ? -lat_fp : lat_fp;
        int idx = (alat * LAT_STEPS) >> FP;
        if (idx >= LAT_STEPS) idx = LAT_STEPS - 1;
        uint32_t base_band = band_lut[idx];

        int grs_ok = (lat_fp < -FP_ONE * 20 / 100 && lat_fp > -FP_ONE * 34 / 100);
        int32_t grs_dlat = lat_fp + FP_ONE * 27 / 100;
        int32_t grs_ey = (grs_dlat * grs_dlat * 16) / (FP_ONE * FP_ONE / 256);

        uint32_t lstep = lon_step[dyi];
        int32_t dy_ly = dy * ly;

        int x0 = PCX - PR;
        int x1 = PCX + PR;
        if (x0 < 0) x0 = 0;
        if (x1 >= (int)LCD_W) x1 = LCD_W - 1;

        for (int x = x0; x <= x1; x++) {
            int dx = x - PCX;
            int32_t nz = (int32_t)nz_row[dx];
            if (nz == 0) continue;

            uint32_t lon_accum = (uint32_t)(dx + hw) * lstep + (uint32_t)lon_offset_fp16;
            int32_t lon = (int32_t)((lon_accum >> 16) & 0xFF);

            int32_t dot = dx * lx + dy_ly + nz * lz;
            int32_t diff = (dot * NORM_INV) >> NORM_SHIFT;

            uint32_t cr = (base_band >> 16) & 0xFF;
            uint32_t cg = (base_band >> 8) & 0xFF;
            uint32_t cb = base_band & 0xFF;

            if (grs_ok) {
                int32_t dlon = ((lon - 80 + 128) & 0xFF) - 128;
                if (dlon > -18 && dlon < 18) {
                    int32_t ed = dlon * dlon + grs_ey;
                    if (ed < 200) {
                        if      (ed < 80)  { cr=180; cg=65;  cb=40; }
                        else if (ed < 140) { cr=195; cg=80;  cb=45; }
                        else               { cr=210; cg=105; cb=60; }
                    }
                }
            }

            int32_t lit;
            if (diff > 0) {
                lit = 64 + ((FP_ONE - 64) * diff >> FP);
                if (lit > FP_ONE) lit = FP_ONE;
            } else {
                lit = 64;
            }
            cr = cr * lit >> FP;
            cg = cg * lit >> FP;
            cb = cb * lit >> FP;

            if (diff > FP_ONE / 4) {
                int32_t s = diff - FP_ONE / 4;
                s = (s * s) >> FP;
                s = (s * s) >> FP;
                cr += s * 200 >> FP;
                cg += s * 180 >> FP;
                cb += s * 140 >> FP;
            }

            if (nz < PR / 3) {
                int32_t glow = PR / 3 - nz;
                cb += glow * 3 / 2;
                cg += glow * 3 / 4;
            }

            if (cr > 255) cr = 255;
            if (cg > 255) cg = 255;
            if (cb > 255) cb = 255;

            row[x] = (cr << 16) | (cg << 8) | cb;
        }
    }
}

/* ---- Stars ---- */
static const struct {
    int x, y; uint8_t mag, twinkle;
} real_stars[] = {
    { 45,30,4,3},{ 78,55,3,5},{120,20,5,2},{ 35,90,2,7},{ 95,75,3,4},
    {150,45,2,6},{ 12,140,3,3},{ 60,165,2,8},{110,130,4,2},{ 25,200,3,5},
    { 85,230,2,4},{145,195,3,6},{ 50,250,2,7},{100,260,3,3},{ 20,110,5,2},
    {  8,55,3,5},{ 55,120,2,4},{ 30,160,3,6},{ 70,145,2,3},{380,25,3,4},
    {420,60,4,3},{460,35,2,7},{395,95,2,5},{440,110,3,2},{470,80,3,6},
    {350,40,2,8},{410,150,2,4},{450,180,3,5},{370,200,4,3},{430,220,2,6},
    {465,195,2,4},{385,250,3,7},{445,260,2,3},{200,8,2,5},{280,15,3,4},
    {320,5,2,6},{175,265,2,7},{260,255,3,3},{310,268,2,5},{160,15,2,4},
    {330,20,2,6},{ 15,10,3,3},{475,5,2,5},{ 10,260,2,4},{475,265,3,7},
    {  5,180,2,3},{478,150,2,5},
};
#define NUM_STARS (sizeof(real_stars) / sizeof(real_stars[0]))

static void render_stars(volatile uint32_t *ovl, uint32_t frame)
{
    for (uint32_t i = 0; i < NUM_STARS; i++) {
        int cx = real_stars[i].x, cy = real_stars[i].y;
        int mag = real_stars[i].mag;
        int32_t twinkle = sin_lut[(frame * real_stars[i].twinkle) & SIN_MASK];
        int32_t bright = 0x40 + mag * 0x30 + (twinkle >> 5);
        if (bright < 0x10) bright = 0x10;
        if (bright > 0xFF) bright = 0xFF;

        int radius = (mag >= 4) ? 2 : (mag >= 3) ? 1 : 0;

        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int sx = cx + dx, sy = cy + dy;
                if (sx < 0 || sx >= (int)LCD_W || sy < 0 || sy >= (int)LCD_H) continue;
                int px = sx - PCX, py = sy - PCY;
                if (px * px + py * py < (PR + 4) * (PR + 4)) continue;

                int d = dx * dx + dy * dy;
                int32_t f = (d == 0) ? bright : (d <= 1) ? bright*3/4 :
                            (d <= 2) ? bright/2 : bright/4;
                if (f < 0x08) continue;

                uint32_t r, g, b;
                if (mag >= 4) { r=f; g=f*245/256; b=f*220/256; }
                else          { r=f*220/256; g=f*230/256; b=f; }
                uint32_t a = (f > 0x80) ? 0xFF : (uint32_t)(f * 2);
                ovl[sy * LCD_W + sx] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/* ==== ENTRY ==== */
void main(void)
{
    uart_puts("\n\n=== Jupiter ===\n");
    timer_init();
    init_sincos();
    precompute_space();
    precompute_bands();

    uart_puts("Precomputing geometry...\n");
    precompute_geometry();

    memset32_neon(FB0_ADDR, 0, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0, LCD_W * LCD_H * 4);

    video_init();
    mmu_init();

    render_space_full((uint32_t *)FB0_ADDR);
    render_space_full((uint32_t *)FB1_ADDR);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(FB1_ADDR);

    uart_puts("Entering orbit...\n\n");

    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();

        uint32_t *fb = (uint32_t *)back_fb;
        render_planet(fb, frame);
        uint32_t t_planet = timer_read();

        memset32_neon(back_ovl, 0, LCD_W * LCD_H * 4);
        volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;
        render_stars(ovl, frame);

        dcache_clean_fb(back_fb);
        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);
        video_swap(back_fb);
        video_set_overlay(back_ovl);

        uint32_t tmp;
        tmp = back_fb;  back_fb  = front_fb;  front_fb  = tmp;
        tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        uint32_t us_planet = ticks_to_us(timer_elapsed(t0, t_planet));
        uint32_t us_total  = ticks_to_us(timer_elapsed(t0, timer_read()));

        if ((frame % 60) == 0) {
            uart_puts("f=");
            uart_putdec(frame);
            uart_puts(" planet=");
            uart_putdec(us_planet);
            uart_puts(" total=");
            uart_putdec(us_total);
            uart_puts("us\n");
        }
        frame++;
    }
}
