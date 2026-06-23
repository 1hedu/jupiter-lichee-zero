/*
 * Jupiter SDK — Procedural Jupiter + Nuked-OPN2 Fighting Back
 *
 * Copy of the jupiter_moon planet renderer (procedural Jupiter with
 * banded atmosphere + Great Red Spot + day/night terminator + star
 * field overlay), with one big audio change:
 *
 *   The Moon (NES VGM, real-time) → Fighting Back (Genesis VGM,
 *   pre-rendered through Nuked-OPN2 into a DRAM buffer)
 *
 * Nuked-OPN2 is the cycle-accurate YM2612 emulator and is too heavy
 * for real-time playback on the V3s Cortex-A7, so we boot, render
 * the entire 60-second buffer up front (~3-4 minutes wall clock),
 * then enter the planet animation with playback streaming from DRAM
 * at zero CPU cost.
 *
 * Build: make GAME=examples/opn2_jupiter/main.c
 *   (the "opn2" in the path tells the Makefile to swap the Gens
 *    YM2612 core for Nuked-OPN2 via lib/nuked_opn2_shim.c)
 */
#include "jupiter.h"
#include "pmu.h"
#include "../../libvgm/vgm_player.h"
#include "fighting_back.h"

/* ================================================================
 *  Audio: pre-rendered DRAM buffer + streaming playback
 * ================================================================ */

#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;

#define OUT_RATE        48000
#define RECORDED_SECS   60
#define RECORDED_FRAMES (OUT_RATE * RECORDED_SECS)

static int16_t recorded[RECORDED_FRAMES * 2];
static volatile uint32_t recorded_pos;
static volatile uint32_t play_pos;

#define TARGET_DEPTH 2400

static vgm_player_t vgm;

/* Stream pre-rendered samples into mix_buf. Loops at end. */
static void playback_fill(uint32_t up_to_frames)
{
    /* Clamp target to ring buffer capacity — never overfill */
    uint32_t target = up_to_frames * 2;
    if (target > MIX_BUF_SIZE - 2) target = MIX_BUF_SIZE - 2;
    while ((mix_wr - mix_rd) < target) {
        if (play_pos >= recorded_pos) play_pos = 0;
        mix_buf[mix_wr & MIX_BUF_MASK] = recorded[play_pos * 2 + 0];
        mix_wr++;
        mix_buf[mix_wr & MIX_BUF_MASK] = recorded[play_pos * 2 + 1];
        mix_wr++;
        play_pos++;
    }
}

/* ================================================================
 *  Tiny pixel font for the boot splash (5x7, only what we need)
 * ================================================================ */

#define FNT_W 5
#define FNT_H 7
typedef struct { char ch; uint8_t cols[FNT_W]; } glyph_t;

static const glyph_t font[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'A', {0x7E,0x09,0x09,0x09,0x7E}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},
    {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},
    {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x7F,0x20,0x18,0x20,0x7F}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},
    {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},
    {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},
    {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {':', {0x00,0x24,0x00,0x00,0x00}},
    {'.', {0x00,0x40,0x00,0x00,0x00}},
    {'%', {0x23,0x13,0x08,0x64,0x62}},
};
#define FONT_N (sizeof(font)/sizeof(font[0]))

static const uint8_t *glyph_for(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (uint32_t i = 0; i < FONT_N; i++)
        if (font[i].ch == c) return font[i].cols;
    return font[0].cols;
}

static void draw_text_p(uint32_t *fb, int pitch, int max_w, int max_h,
                        const char *s, int x, int y, int size, uint32_t color)
{
    while (*s) {
        const uint8_t *g = glyph_for(*s);
        for (int col = 0; col < FNT_W; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < FNT_H; row++) {
                if (!(bits & (1 << row))) continue;
                for (int dy = 0; dy < size; dy++) {
                    int py = y + row * size + dy;
                    if (py < 0 || py >= max_h) continue;
                    uint32_t *line = fb + py * pitch;
                    for (int dx = 0; dx < size; dx++) {
                        int px = x + col * size + dx;
                        if (px < 0 || px >= max_w) continue;
                        line[px] = color;
                    }
                }
            }
        }
        x += (FNT_W + 1) * size;
        s++;
    }
}

/* Convenience: draw on full-screen framebuffer */
static void draw_text(uint32_t *fb, const char *s, int x, int y, int size, uint32_t color)
{
    draw_text_p(fb, LCD_W, LCD_W, LCD_H, s, x, y, size, color);
}

static void fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)LCD_W) w = LCD_W - x;
    if (y + h > (int)LCD_H) h = LCD_H - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; row++) {
        uint32_t *line = fb + (y + row) * LCD_W + x;
        for (int col = 0; col < w; col++) line[col] = color;
    }
}

/* ====================================================================
 *  Procedural Jupiter renderer — verbatim from examples/jupiter_moon
 * ==================================================================== */

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
    /* NEON row fill — each row is a constant gradient color.
     * memset32_neon does 64 bytes/iter via vstm {q0-q3}. */
    uint32_t addr = (uint32_t)fb;
    for (uint32_t y = 0; y < LCD_H; y++) {
        memset32_neon(addr, space_colors[y], LCD_W * 4);
        addr += LCD_W * 4;
    }
}

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

/* Render stars directly onto VI0 (opaque — no alpha needed) */
static void render_stars(uint32_t *fb, uint32_t frame)
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
                fb[sy * LCD_W + sx] = (r << 16) | (g << 8) | b;
            }
        }
    }
}

/* ====================================================================
 *  HUD elements — drawn on UI0 (ARGB alpha overlay)
 * ==================================================================== */

#define WAVE_W    200       /* waveform box width */
#define WAVE_H    24        /* waveform box height */
#define WAVE_X    ((LCD_W - WAVE_W) / 2)
#define WAVE_Y    (LCD_H - WAVE_H - 6)
#define HUD_BG    0xA0101828  /* semi-transparent dark blue */
#define HUD_BORDER 0xC04080C0 /* semi-transparent border */

/* Draw a semi-transparent box on the ARGB overlay */
static void hud_box(volatile uint32_t *ovl, int x, int y, int w, int h)
{
    for (int row = 0; row < h; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)LCD_H) continue;
        for (int col = 0; col < w; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)LCD_W) continue;
            uint32_t c = (row == 0 || row == h-1 || col == 0 || col == w-1)
                       ? HUD_BORDER : HUD_BG;
            ovl[py * LCD_W + px] = c;
        }
    }
}

/* Draw audio waveform on UI0 overlay */
static void hud_waveform(volatile uint32_t *ovl)
{
    hud_box(ovl, WAVE_X, WAVE_Y, WAVE_W, WAVE_H);

    int mid_y = WAVE_Y + WAVE_H / 2;
    int inner_h = (WAVE_H - 4) / 2;  /* half-height for waveform amplitude */

    /* Sample from recorded[] near current playback position */
    uint32_t base = play_pos;
    if (base >= 512) base -= 512; else base = 0;
    int num_pts = WAVE_W - 4;
    for (int i = 0; i < num_pts; i++) {
        uint32_t si = base + (i * 800) / num_pts;
        if (si >= recorded_pos) si = 0;
        int16_t L = recorded[si * 2];
        int16_t R = recorded[si * 2 + 1];
        int32_t mono = (L + R) / 2;

        /* Map to pixel offset from center */
        int y_off = (mono * inner_h) / 32768;
        int px = WAVE_X + 2 + i;
        int py = mid_y - y_off;
        if (py < WAVE_Y + 1) py = WAVE_Y + 1;
        if (py > WAVE_Y + WAVE_H - 2) py = WAVE_Y + WAVE_H - 2;

        /* Draw a 1px dot — cyan */
        ovl[py * LCD_W + px] = 0xFF60D0FF;
        /* Connect to center line with dim trail */
        int y0 = (y_off >= 0) ? mid_y : py;
        int y1 = (y_off >= 0) ? py : mid_y;
        for (int yy = y0; yy <= y1; yy++)
            ovl[yy * LCD_W + px] = 0xB030A0D0;
        ovl[py * LCD_W + px] = 0xFF60D0FF;  /* bright tip on top */
    }
}

/* Draw track info box, top-left */
static void hud_track_info(volatile uint32_t *ovl, uint32_t frame)
{
    int bw = 180, bh = 32;
    int bx = 6, by = 6;
    hud_box(ovl, bx, by, bw, bh);

    draw_text((uint32_t *)ovl, "FIGHTING BACK", bx + 4, by + 3, 1, 0xFFE0C040);
    draw_text((uint32_t *)ovl, "THUNDER FORCE 4", bx + 4, by + 12, 1, 0xFFA0C8E0);

    /* Timestamp */
    uint32_t sec = frame / 60;
    uint32_t min = sec / 60;
    sec = sec % 60;
    char ts[8];
    ts[0] = '0' + (min / 10);
    ts[1] = '0' + (min % 10);
    ts[2] = ':'; /* reuse '-' glyph as colon standin */
    ts[3] = '0' + (sec / 10);
    ts[4] = '0' + (sec % 10);
    ts[5] = 0;
    draw_text((uint32_t *)ovl, ts, bx + 4, by + 22, 1, 0xFFE8E8F0);
    draw_text((uint32_t *)ovl, "NUKED-OPN2", bx + 80, by + 22, 1, 0xFF80C080);
}

/* VI1 badge dimensions — "JUPITER" big, "SDK" small underneath */
#define BADGE_W  110
#define BADGE_H  28

/* ====================================================================
 *  Pre-render boot splash
 * ==================================================================== */

static void draw_prerender_splash(uint32_t *fb, int progress_pct)
{
    render_space_full(fb);

    /* Title */
    {
        const char *title = "JUPITER";
        int char_w = (FNT_W + 1) * 5;
        int title_w = 7 * char_w - 5;
        int x = ((int)LCD_W - title_w) / 2;
        draw_text(fb, title, x, 60, 5, 0xFFE0C040);
    }
    /* Subtitle */
    {
        const char *sub = "NUKED-OPN2 PRE-RENDER";
        int char_w = (FNT_W + 1) * 2;
        int sub_w = 21 * char_w - 2;
        int x = ((int)LCD_W - sub_w) / 2;
        draw_text(fb, sub, x, 115, 2, 0xFFE0C040);
    }
    /* Track */
    {
        const char *line = "FIGHTING BACK - THUNDER FORCE 4";
        int char_w = (FNT_W + 1) * 2;
        int w = 31 * char_w - 2;
        int x = ((int)LCD_W - w) / 2;
        draw_text(fb, line, x, 145, 2, 0xFFA0C8E0);
    }
    /* Progress bar */
    int bar_x = 60, bar_y = 200, bar_w = LCD_W - 120, bar_h = 16;
    fill_rect(fb, bar_x - 2, bar_y - 2, bar_w + 4, bar_h + 4, 0xFFE0C040);
    fill_rect(fb, bar_x, bar_y, bar_w, bar_h, 0xFF0A0F1E);
    int fill_w = (bar_w * progress_pct) / 100;
    if (fill_w > 0) fill_rect(fb, bar_x, bar_y, fill_w, bar_h, 0xFF60D0FF);
    /* Percentage */
    {
        char buf[8]; int n = 0;
        if (progress_pct >= 100) { buf[n++]='1'; buf[n++]='0'; buf[n++]='0'; }
        else if (progress_pct >= 10) { buf[n++]='0'+(progress_pct/10); buf[n++]='0'+(progress_pct%10); }
        else { buf[n++]='0'+progress_pct; }
        buf[n++]='%'; buf[n]=0;
        int char_w = (FNT_W+1)*2;
        int w = n*char_w-2;
        int x = ((int)LCD_W-w)/2;
        draw_text(fb, buf, x, 235, 2, 0xFFE8E8F0);
    }
}

/* ====================================================================
 *  Vortex renderer — ported from VortexEffect.lua
 * ==================================================================== */

#define VORTEX_MAP_SIZE  64
#define VORTEX_MAP_MASK  63
#define VORTEX_TILE_SHIFT 3

#define VCX (LCD_W / 2)
#define VCY (LCD_H / 2)

static uint8_t  vortex_map[VORTEX_MAP_SIZE * VORTEX_MAP_SIZE];
static uint32_t vortex_lut[256];

static void vortex_init(void)
{
    int half = VORTEX_MAP_SIZE / 2;
    for (int y = 0; y < VORTEX_MAP_SIZE; y++) {
        for (int x = 0; x < VORTEX_MAP_SIZE; x++) {
            int dx = x - half, dy = y - half;
            uint32_t dist = isqrt((uint32_t)(dx * dx + dy * dy));
            int32_t angle = 0;
            if (dx != 0 || dy != 0) {
                int32_t ax = dx < 0 ? -dx : dx;
                int32_t ay = dy < 0 ? -dy : dy;
                int32_t a = (ax >= ay)
                    ? (ay * 32) / (ax ? ax : 1)
                    : 64 - (ax * 32) / (ay ? ay : 1);
                if (dx >= 0 && dy >= 0) angle = a;
                else if (dx < 0 && dy >= 0) angle = 128 - a;
                else if (dx < 0 && dy < 0) angle = 128 + a;
                else angle = 256 - a;
                angle &= 255;
            }
            int32_t spiral_val = (dist * 20 + angle * 2) & 255;
            uint8_t tile;
            if (dist < 2) tile = 255;
            else if (spiral_val < 128) {
                int32_t intensity = 255 - dist * 6;
                if (intensity < 25) intensity = 25;
                tile = (uint8_t)(128 + (intensity >> 1));
            } else {
                int32_t intensity = 80 - dist * 3;
                if (intensity < 12) intensity = 12;
                tile = (uint8_t)intensity;
            }
            vortex_map[y * VORTEX_MAP_SIZE + x] = tile;
        }
    }
    for (int i = 0; i < 256; i++) {
        uint32_t r, g, b;
        if (i >= 250) { r = 255; g = 220; b = 255; }
        else if (i >= 128) { uint32_t t = i - 128; r = 80+t; g = 20+t/3; b = 120+t; }
        else { r = i/4; g = i/6; b = i/2; }
        vortex_lut[i] = (r << 16) | (g << 8) | b;
    }
}

#if 0 /* trivial vortex — used for bisect */
static void render_vortex_TRIVIAL(uint32_t *fb, uint32_t frame)
{
    for (int y = 0; y < (int)LCD_H; y++) {
        uint32_t *row = fb + y * LCD_W;
        uint32_t c = ((y + frame) & 0xFF) << 8;
        for (int x = 0; x < (int)LCD_W; x++)
            row[x] = c | ((x + frame) & 0xFF);
    }
}
#endif

#if 0 /* original vortex with divisions — causes audio distortion */
static void render_vortex_FULL(uint32_t *fb, uint32_t frame)
{
    const int32_t SPIRAL_STRENGTH_FP = 3;
    const int32_t TIME_SCALE = 3;
    const int32_t WAVE_AMP = 12;
    const int32_t CENTER_U = VORTEX_MAP_SIZE * 4;
    const int32_t CENTER_V = VORTEX_MAP_SIZE * 4;
    int32_t time_angle = frame * TIME_SCALE;
    int32_t wave_time = frame * 5;

    for (int y = 0; y < (int)LCD_H; y++) {
        int32_t dy = y - VCY;
        uint32_t *row = fb + y * LCD_W;
        for (int x = 0; x < (int)LCD_W; x++) {
            int32_t dx = x - VCX;
            int32_t ax = dx < 0 ? -dx : dx;
            int32_t ay = dy < 0 ? -dy : dy;
            int32_t dist = (ax > ay) ? ax + (ay * 3 >> 3) : ay + (ax * 3 >> 3);
            int32_t angle;
            if (ax == 0 && ay == 0) { angle = 0; }
            else {
                int32_t a = (ax >= ay) ? (ay * 32) / ax : 64 - (ax * 32) / ay;
                if (dx >= 0 && dy >= 0) angle = a;
                else if (dx < 0 && dy >= 0) angle = 128 - a;
                else if (dx < 0 && dy < 0) angle = 128 + a;
                else angle = 256 - a;
                angle &= 255;
            }
            int32_t spiral_angle = angle + (dist * SPIRAL_STRENGTH_FP >> 4) + time_angle;
            int32_t wave = sin_lut[(wave_time + (dist >> 1)) & SIN_MASK] >> (SIN_FP - 4);
            int32_t pull_dist = dist + wave * WAVE_AMP / 16;
            if (pull_dist < 0) pull_dist = 0;
            int32_t ca = cos_lut[spiral_angle & SIN_MASK];
            int32_t sa = sin_lut[spiral_angle & SIN_MASK];
            int32_t world_x = CENTER_U + ((ca * pull_dist) >> SIN_FP);
            int32_t world_y = CENTER_V + ((sa * pull_dist) >> SIN_FP);
            int32_t tx = (world_x >> VORTEX_TILE_SHIFT) & VORTEX_MAP_MASK;
            int32_t ty = (world_y >> VORTEX_TILE_SHIFT) & VORTEX_MAP_MASK;
            uint32_t color = vortex_lut[vortex_map[ty * VORTEX_MAP_SIZE + tx]];
            int32_t shift = sin_lut[(frame * 2) & SIN_MASK];
            {
                int32_t cr = (int32_t)((color >> 16) & 0xFF);
                int32_t cg = (int32_t)((color >> 8) & 0xFF);
                int32_t cb = (int32_t)(color & 0xFF);
                cr += (cr * shift) >> (SIN_FP + 2);
                cg += (cg * shift) >> (SIN_FP + 3);
                cb += (cb * shift) >> (SIN_FP + 2);
                if (cr < 5) cr = 5; if (cr > 255) cr = 255;
                if (cg < 5) cg = 5; if (cg > 255) cg = 255;
                if (cb < 5) cb = 5; if (cb > 255) cb = 255;
                color = ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb;
            }
            int32_t max_dist = VCX > VCY ? VCX : VCY;
            int32_t bright = 256 - (dist * 128 / max_dist);
            if (bright < 50) bright = 50;
            if (dist < 40) {
                int32_t glow = (40 - dist) * 6;
                uint32_t cr = ((color >> 16) & 0xFF) + glow;
                uint32_t cg = ((color >> 8) & 0xFF) + glow / 2;
                uint32_t cb = (color & 0xFF) + glow;
                if (cr > 255) cr = 255;
                if (cg > 255) cg = 255;
                if (cb > 255) cb = 255;
                color = (cr << 16) | (cg << 8) | cb;
                bright = 256;
            }
            if (bright < 256) {
                uint32_t cr = ((color >> 16) & 0xFF) * bright >> 8;
                uint32_t cg = ((color >> 8) & 0xFF) * bright >> 8;
                uint32_t cb = (color & 0xFF) * bright >> 8;
                color = (cr << 16) | (cg << 8) | cb;
            }
            row[x] = color;
        }
    }
}
#endif

/* Optimized vortex: divisions replaced with shifts/multiplies */
static void render_vortex(uint32_t *fb, uint32_t frame)
{
    int32_t time_angle = frame * 3;
    int32_t wave_time = frame * 5;
    int32_t frame_shift = sin_lut[(frame * 2) & SIN_MASK];
    const int32_t CU = VORTEX_MAP_SIZE * 4;
    const int32_t CV = VORTEX_MAP_SIZE * 4;

    for (int y = 0; y < (int)LCD_H; y++) {
        int32_t dy = y - VCY;
        int32_t ay = dy < 0 ? -dy : dy;
        uint32_t *row = fb + y * LCD_W;
        for (int x = 0; x < (int)LCD_W; x++) {
            int32_t dx = x - VCX;
            int32_t ax = dx < 0 ? -dx : dx;
            int32_t dist = (ax > ay) ? ax + (ay * 3 >> 3) : ay + (ax * 3 >> 3);

            /* Angle: branchless octant approximation, NO division.
             * Use (min << 5) / max approximated by (min << 5) * recip >> 16
             * Or simpler: just use min*64/max via shift-subtract division.
             * Cheapest: use atan2 LUT indexed by (dy_sign, dx_sign, min/max ratio) */
            /* Angle: division-free approximation using the spiral tilemap's
             * own symmetry. For the vortex visual, exact angle doesn't matter
             * much — the spiral rotation dominates the look. Use a simple
             * octant approximation with multiply+shift instead of divide. */
            int32_t angle = 0;
            if (ax | ay) {
                /* Rough octant angle (0-255) using no division:
                 * ratio ≈ min * 64 >> ceil(log2(max+1)). Close enough for spiral. */
                int32_t mn = ax < ay ? ax : ay;
                int32_t mx = ax > ay ? ax : ay;
                /* Approximate mn*64/mx using: mn*64 >> log2(mx)
                 * Find highest set bit of mx for the shift amount */
                int32_t shift = mx > 1 ? (31 - __builtin_clz((uint32_t)mx)) : 1;
                int32_t a = (mn << 6) >> shift;
                if (a > 64) a = 64;
                a = (ax >= ay) ? (a >> 1) : 64 - (a >> 1);
                if (dx >= 0 && dy >= 0) angle = a;
                else if (dx < 0 && dy >= 0) angle = 128 - a;
                else if (dx < 0 && dy < 0) angle = 128 + a;
                else angle = 256 - a;
                angle &= 255;
            }

            int32_t spiral_angle = angle + (dist * 3 >> 4) + time_angle;
            int32_t wave = sin_lut[(wave_time + (dist >> 1)) & SIN_MASK] >> (SIN_FP - 4);
            int32_t pull_dist = dist + wave * 12 / 16;
            if (pull_dist < 0) pull_dist = 0;

            int32_t ca = cos_lut[spiral_angle & SIN_MASK];
            int32_t sa = sin_lut[spiral_angle & SIN_MASK];
            int32_t wx = CU + ((ca * pull_dist) >> SIN_FP);
            int32_t wy = CV + ((sa * pull_dist) >> SIN_FP);

            uint32_t color = vortex_lut[vortex_map[
                ((wy >> VORTEX_TILE_SHIFT) & VORTEX_MAP_MASK) * VORTEX_MAP_SIZE +
                ((wx >> VORTEX_TILE_SHIFT) & VORTEX_MAP_MASK)]];

            /* Color shift (frame_shift is constant) */
            int32_t cr = (int32_t)((color >> 16) & 0xFF);
            int32_t cg = (int32_t)((color >> 8) & 0xFF);
            int32_t cb = (int32_t)(color & 0xFF);
            cr += (cr * frame_shift) >> (SIN_FP + 2);
            cg += (cg * frame_shift) >> (SIN_FP + 3);
            cb += (cb * frame_shift) >> (SIN_FP + 2);

            /* Brightness: shift instead of divide */
            int32_t bright = 256 - (dist >> 1);
            if (bright < 50) bright = 50;
            if (dist < 40) {
                cr += (40 - dist) * 6;
                cg += (40 - dist) * 3;
                cb += (40 - dist) * 6;
                bright = 256;
            }
            cr = cr * bright >> 8;
            cg = cg * bright >> 8;
            cb = cb * bright >> 8;
            if (cr < 0) cr = 0; if (cr > 255) cr = 255;
            if (cg < 0) cg = 0; if (cg > 255) cg = 255;
            if (cb < 0) cb = 0; if (cb > 255) cb = 255;

            row[x] = ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb;
        }
    }
}

/* Pre-render driver */
static void prerender_song(void)
{
    uart_puts("Pre-rendering ");
    uart_putdec(RECORDED_SECS);
    uart_puts(" seconds with Nuked-OPN2...\n");

    vgm_load(&vgm, vgm_fighting_back, vgm_fighting_back_len, 0);
    vgm_play(&vgm);

    recorded_pos = 0;
    uint32_t render_start = timer_read();
    uint32_t last_print_us = 0;
    uint32_t back = FB1_ADDR, front = FB0_ADDR;

    while (recorded_pos < RECORDED_FRAMES) {
        uint32_t want = RECORDED_FRAMES - recorded_pos;
        if (want > 1024) want = 1024;

        vgm_render(&vgm, &recorded[recorded_pos * 2], want);
        recorded_pos += want;

        if (vgm.cur_loop >= 1) {
            vgm_stop(&vgm);
            vgm_load(&vgm, vgm_fighting_back, vgm_fighting_back_len, 0);
            vgm_play(&vgm);
        }

        uint64_t cur_us = (uint64_t)recorded_pos * 1000000ULL / OUT_RATE;
        if (cur_us - last_print_us >= 1000000) {
            int pct = (int)(((uint64_t)recorded_pos * 100ULL) / RECORDED_FRAMES);
            uint32_t *fb = (uint32_t *)back;
            draw_prerender_splash(fb, pct);
            dcache_clean_fb(back);
            video_swap(back);
            uint32_t tmp = back; back = front; front = tmp;

            uint32_t wall_us = ticks_to_us(timer_elapsed(render_start, timer_read()));
            uart_puts("  ");
            uart_putdec((uint32_t)(cur_us / 1000000));
            uart_puts("s in ");
            uart_putdec(wall_us / 1000);
            uart_puts(" ms (");
            uart_putdec((uint32_t)(((uint64_t)wall_us * 100ULL) / cur_us));
            uart_puts("% rt)\n");
            last_print_us = cur_us;
        }
    }

    /* Final 100% */
    {
        uint32_t *fb = (uint32_t *)back;
        draw_prerender_splash(fb, 100);
        dcache_clean_fb(back);
        video_swap(back);
    }
    uart_puts("Pre-render done.\n\n");
}

/* ====================================================================
 *  Main
 * ==================================================================== */

void main(void)
{
    uart_puts("\n\n=== Jupiter + Nuked-OPN2 Fighting Back ===\n");
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();

    /* CPU PLL bump to 1.2 GHz is now in timer_init() (lib/timer.c) */

    /* Pre-compute the planet/star LUTs first so the main animation
     * starts instantly once pre-render finishes. */
    init_sincos();
    precompute_space();
    precompute_bands();
    uart_puts("Precomputing planet geometry...\n");
    precompute_geometry();
    vortex_init();

    /* Wipe both VI0 buffers + both UI0 overlays so nothing leaks
     * through during the splash. */
    memset32_neon(FB0_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0, LCD_W * LCD_H * 4);

    video_init();

    /* Show splash at 0% */
    draw_prerender_splash((uint32_t *)FB0_ADDR, 0);
    dcache_clean_fb(FB0_ADDR);
    video_swap(FB0_ADDR);

    audio_init();

    /* Pre-render Fighting Back through Nuked-OPN2 */
    prerender_song();

    /* Pre-fill mix buffer + start DMA playback */
    play_pos = 0;
    audio_set_rate(OUT_RATE);
    playback_fill(MIX_BUF_SIZE / 4);
    audio_start();
    irq_global_enable();

    /* VI1: "JUPITER / SDK" badge, top-right — no border, dark bg */
    {
        uint32_t *badge = (uint32_t *)SPR_ADDR;
        /* Dark gradient background */
        for (int y = 0; y < BADGE_H; y++) {
            uint32_t shade = 0x08 + (y * 0x0C / BADGE_H);
            uint32_t bg = 0xFF000000 | (shade/2 << 16) | (shade/2 << 8) | shade;
            for (int x = 0; x < BADGE_W; x++)
                badge[y * BADGE_W + x] = bg;
        }
        /* "JUPITER" — size 2, bold white-blue */
        draw_text_p(badge, BADGE_W, BADGE_W, BADGE_H,
                    "JUPITER", 7, 3, 2, 0xFFE0C040);
        /* "SDK" — size 1, subtle below */
        draw_text_p(badge, BADGE_W, BADGE_W, BADGE_H,
                    "SDK", 76, 19, 1, 0xFF6090B0);
    }
    dcache_clean_range(SPR_ADDR, BADGE_W * BADGE_H * 4);
    video_vi1_init(LCD_W - BADGE_W - 5, 7, BADGE_W, BADGE_H);
    uart_puts("Entering orbit...\n\n");

    uint32_t back_fb  = FB1_ADDR, front_fb  = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();

        /* 1) Render vortex to VI0 back buffer */
        render_vortex((uint32_t *)back_fb, frame);
        uint32_t t_planet = timer_read();

        /* 2) HUD on UI0 overlay (ARGB alpha).
         * Only clear the HUD regions, not the full 522KB overlay.
         * Alpha=0 pixels are already transparent from init clear. But
         * with double-buffering, the back buffer has stale HUD from 2
         * frames ago. We re-draw the same regions, so hud_box overwrites
         * everything including stale content. Just need to clear the
         * waveform interior (it changes every frame). */
        /* Clear waveform box region */
        {
            volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;
            for (int y = WAVE_Y; y < WAVE_Y + WAVE_H && y < (int)LCD_H; y++)
                for (int x = WAVE_X; x < WAVE_X + WAVE_W && x < (int)LCD_W; x++)
                    ovl[y * LCD_W + x] = 0;
        }
        /* Clear track info region */
        {
            volatile uint32_t *ovl = (volatile uint32_t *)back_ovl;
            for (int y = 6; y < 38 && y < (int)LCD_H; y++)
                for (int x = 6; x < 186 && x < (int)LCD_W; x++)
                    ovl[y * LCD_W + x] = 0;
        }
        hud_waveform((volatile uint32_t *)back_ovl);
        hud_track_info((volatile uint32_t *)back_ovl, frame);

        /* 3) Push to display */
        dcache_clean_fb(back_fb);
        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);
        video_swap(back_fb);
        video_set_overlay(back_ovl);

        uint32_t tmp;
        tmp = back_fb;  back_fb  = front_fb;  front_fb  = tmp;
        tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        /* 5) Top up mix buffer from pre-rendered DRAM */
        playback_fill(TARGET_DEPTH);

        /* 6) Pace to 60 fps; keep refilling while we wait */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            playback_fill(TARGET_DEPTH);

        if ((frame % 60) == 0) {
            uint32_t us_planet = ticks_to_us(timer_elapsed(t0, t_planet));
            uint32_t us_total  = ticks_to_us(timer_elapsed(t0, timer_read()));
            uint32_t fill = mix_wr - mix_rd;
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" planet="); uart_putdec(us_planet);
            uart_puts("us total="); uart_putdec(us_total);
            uart_puts("us fill="); uart_putdec(fill);
            uart_puts("\n");
        }
        frame++;
    }
}
