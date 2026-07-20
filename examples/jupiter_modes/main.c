/*
 * Jupiter Modes Tour — every hardware-native display mode, in order
 *
 * Auto-cycles (~6 s per mode, N64 A/Right to skip, Left to go back):
 *
 *   MODE 0 FLAT      overlay vanishes: only VI0 is scanned
 *   MODE 1 BG+OBJ    the orbiting sprites return (UI0 alpha blend)
 *   MODE 2 TRIPLANE  VI1 hardware window drops in (live pattern)
 *   MODE 3 GHOST     whole overlay breathes via hw global alpha
 *   MODE 4 CINEMA    VI0 scans the Cedar decoder's NV12 directly
 *   MODE 5 SPLIT     two hardware viewports: dusk on top, a second
 *                    playfield below, one overlay across both
 *   MODE 6 RASTER    hstimer scanline ISR strobes overlay alpha —
 *                    hardware venetian blinds, zero redraws
 *   MODE 7 AFFINE    NEON rotozoom sampling + HARDWARE per-band
 *                    lineshift sway (checkerboards stay checkerboards)
 *
 * Everything on screen besides the Mode 7 rotozoom and the label text
 * is drawn ONCE — the tour itself is pure DE2 register work.
 *
 * Build: make GAME=examples/jupiter_modes/main.c
 */
#include "jupiter.h"
#include "hstimer.h"
#include "input.h"
#include "pmu.h"

#include "../cedar_jpeg/test_frame.h"

/* ================================================================
 * 5x7 font (A-Z minus QXZ, 0-9, + -) for the labels
 * ================================================================ */
typedef struct { char ch; const char *rows[7]; } glyph_t;

static const glyph_t font[] = {
    {'A',{".###.","#...#","#...#","#####","#...#","#...#","#...#"}},
    {'B',{"####.","#...#","#...#","####.","#...#","#...#","####."}},
    {'C',{".####","#....","#....","#....","#....","#....",".####"}},
    {'D',{"####.","#...#","#...#","#...#","#...#","#...#","####."}},
    {'E',{"#####","#....","#....","####.","#....","#....","#####"}},
    {'F',{"#####","#....","#....","####.","#....","#....","#...."}},
    {'G',{".###.","#....","#....","#.###","#...#","#...#",".###."}},
    {'H',{"#...#","#...#","#...#","#####","#...#","#...#","#...#"}},
    {'I',{"#####","..#..","..#..","..#..","..#..","..#..","#####"}},
    {'J',{".####","...#.","...#.","...#.","...#.","#..#.",".##.."}},
    {'K',{"#...#","#..#.","#.#..","##...","#.#..","#..#.","#...#"}},
    {'L',{"#....","#....","#....","#....","#....","#....","#####"}},
    {'M',{"#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#"}},
    {'N',{"#...#","##..#","#.#.#","#..##","#...#","#...#","#...#"}},
    {'O',{".###.","#...#","#...#","#...#","#...#","#...#",".###."}},
    {'P',{"####.","#...#","#...#","####.","#....","#....","#...."}},
    {'R',{"####.","#...#","#...#","####.","#.#..","#..#.","#...#"}},
    {'S',{".####","#....","#....",".###.","....#","....#","####."}},
    {'T',{"#####","..#..","..#..","..#..","..#..","..#..","..#.."}},
    {'U',{"#...#","#...#","#...#","#...#","#...#","#...#",".###."}},
    {'V',{"#...#","#...#","#...#","#...#",".#.#.",".#.#.","..#.."}},
    {'W',{"#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},
    {'Y',{"#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."}},
    {'0',{".###.","#...#","#..##","#.#.#","##..#","#...#",".###."}},
    {'1',{"..#..",".##..","..#..","..#..","..#..","..#..","#####"}},
    {'2',{".###.","#...#","....#","..##.",".#...","#....","#####"}},
    {'3',{"####.","....#","....#",".###.","....#","....#","####."}},
    {'4',{"#...#","#...#","#...#","#####","....#","....#","....#"}},
    {'5',{"#####","#....","#....","####.","....#","....#","####."}},
    {'6',{".###.","#....","#....","####.","#...#","#...#",".###."}},
    {'7',{"#####","....#","...#.","..#..",".#...",".#...",".#..."}},
    {'+',{".....","..#..","..#..","#####","..#..","..#..","....."}},
    {'-',{".....",".....",".....","#####",".....",".....","....."}},
};

static void draw_text(volatile uint32_t *buf, uint32_t pitch,
                      const char *s, int x, int y, int scale, uint32_t color)
{
    for (; *s; s++, x += 6 * scale) {
        if (*s == ' ') continue;
        const glyph_t *g = 0;
        for (unsigned i = 0; i < sizeof(font) / sizeof(font[0]); i++)
            if (font[i].ch == *s) { g = &font[i]; break; }
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g->rows[r][c] == '#')
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++)
                            buf[(y + r * scale + sy) * pitch +
                                (x + c * scale + sx)] = color;
    }
}

static int text_w(const char *s, int scale)
{ int n = 0; for (; *s; s++) n++; return n * 6 * scale; }

/* ================================================================
 * Static scene: dusk band background (VI0), glow orbs (UI0)
 * ================================================================ */
#define LABEL_H 24                       /* label bar at the bottom  */

static const uint32_t sky_bands[8] = {
    0xFF0B1026, 0xFF141A3A, 0xFF232558, 0xFF3A2E6E,
    0xFF5C3B7E, 0xFF8A4A86, 0xFFC75B79, 0xFFF07A5A,
};

static uint32_t hash2(uint32_t x, uint32_t y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void draw_scene(uint32_t fb_addr)
{
    volatile uint32_t *fb = (volatile uint32_t *)fb_addr;
    uint32_t body_h = LCD_H - LABEL_H;
    for (uint32_t y = 0; y < body_h; y++) {
        uint32_t band = y * 8 / body_h;
        /* chunky 8px-cell dither between bands */
        uint32_t cell = ((y >> 3) + ((band * body_h + body_h / 2) / 8)) & 1;
        uint32_t next = band < 7 ? band + 1 : 7;
        uint32_t c = sky_bands[(y * 16 / body_h) & 1 && cell ? next : band];
        for (uint32_t x = 0; x < LCD_W; x++)
            fb[y * LCD_W + x] = c;
    }
    /* stars in the upper half */
    for (uint32_t y = 8; y < body_h / 2; y += 8)
        for (uint32_t x = 4; x < LCD_W - 4; x += 8)
            if ((hash2(x, y) % 41) == 0)
                fb[y * LCD_W + x] = 0xFFE8ECFF;
    /* ridge silhouette above the label bar */
    for (uint32_t x = 0; x < LCD_W; x++) {
        uint32_t h = 10 + (hash2(x >> 4, 7) % 14);
        for (uint32_t y = body_h - h; y < body_h; y++)
            fb[y * LCD_W + x] = 0xFF191230;
    }
}

/* radial glow orb, same recipe as the parallax firefly */
#define ORB_W 20
#define ORB_H 20
static uint32_t orb_data[ORB_W * ORB_H];

static void gen_orb(uint32_t hot, uint32_t warm)
{
    for (int y = 0; y < ORB_H; y++)
        for (int x = 0; x < ORB_W; x++) {
            int dx = x - 10, dy = y - 10;
            int d2 = dx * dx + dy * dy;
            uint32_t px = 0;
            if (d2 <= 4)       px = hot;
            else if (d2 <= 12) px = warm;
            else if (d2 <= 30) px = 0xA0000000 | (warm & 0xFFFFFF);
            else if (d2 <= 60) px = 0x50000000 | (warm & 0xFFFFFF);
            orb_data[y * ORB_W + x] = px;
        }
}

/* sin LUT, Q8, 256 steps (the parabola trick) */
static int16_t sinq8[256];
static void init_sin(void)
{
    for (int i = 0; i < 128; i++) {
        int v = (4 * i * (128 - i) * 256) / (128 * 128);
        sinq8[i] = (int16_t)v;
        sinq8[128 + i] = (int16_t)-v;
    }
}
#define SIN(a) sinq8[(a) & 255]
#define COS(a) sinq8[((a) + 64) & 255]

/* ================================================================
 * Mode 6 RASTER: hstimer ISR strobes UI0 global alpha per band
 * ================================================================ */
static volatile int raster_on;
static volatile uint32_t raster_band;

static void raster_isr(void)
{
    if (!raster_on) return;
    raster_band++;
    /* alternate opaque / half-ghost overlay every 16 scanlines */
    uint32_t a = (raster_band & 1) ? 0x60 : 0xFF;
    UI_ATTR(0) = UI_EN | UI_FMT_ARGB8888 | UI_AMODE_COMBINED | UI_GALPHA(a);
    REG32(0x01100000 + 0x08) = 1;   /* GLB_DBUFF: latch mid-frame */
}

/* Mode 7 hardware half: per-band X shifts, refreshed every frame */
#define M7_BANDS (LCD_H / 8)
static int16_t m7_shift[M7_BANDS];

static void update_m7_shift(uint32_t frame)
{
    for (int i = 0; i < M7_BANDS; i++)
        m7_shift[i] = (int16_t)((SIN(frame * 3 + i * 12) * 10) >> 8);
}

/* ================================================================
 * Mode 7: NEON affine rotozoom into the VI0 body (checkerboard!)
 * ================================================================ */
#define M7_MAP_BITS 5
#define M7_MAP_W    32
static uint8_t  m7_map[M7_MAP_W * M7_MAP_W];
static uint32_t m7_lut[256];

static void init_m7(void)
{
    for (int y = 0; y < M7_MAP_W; y++)
        for (int x = 0; x < M7_MAP_W; x++)
            m7_map[y * M7_MAP_W + x] = (uint8_t)((x ^ y) & 1);
    m7_lut[0] = 0xFF232558;
    m7_lut[1] = 0xFFC75B79;
}

static void draw_m7(uint32_t fb_addr, uint32_t frame)
{
    uint32_t body_h = LCD_H - LABEL_H;
    int32_t c = COS(frame), s = SIN(frame);
    int32_t zoom = 32 + ((SIN(frame * 2) * 10) >> 8); /* breathe */
    /* texel step per output texel (pixel-doubled: 240 texels/row).
     * Rows advance at HALF the texel step so cells stay square —
     * each texel covers 2 px horizontally but rows are 1 px. */
    int32_t du = (c * zoom) >> 8, dv = (s * zoom) >> 8;
    int32_t rdu = du / 2, rdv = dv / 2;
    for (uint32_t y = 0; y < body_h; y++) {
        int32_t ry = (int32_t)y - (int32_t)body_h / 2;
        int32_t u = (M7_MAP_W << 7) - ry * rdv - 120 * du;
        int32_t v = (M7_MAP_W << 7) + ry * rdu - 120 * dv;
        mode7_scanline((uint32_t *)(fb_addr + y * LCD_PITCH),
                       m7_map, m7_lut, u, v, du, dv,
                       LCD_W / 2, M7_MAP_BITS, M7_MAP_W - 1);
    }
}

/* ================================================================
 * The tour
 * ================================================================ */
typedef struct { int mode; const char *label; } phase_t;

static const phase_t phases[] = {
    { 0, "MODE 0 FLAT - VI0 ONLY" },
    { 1, "MODE 1 BG+OBJ" },
    { 2, "MODE 2 TRIPLANE" },
    { 3, "MODE 3 GHOST FADE" },
    { 4, "MODE 4 CINEMA NV12" },
    { 5, "MODE 5 SPLIT" },
    { 6, "MODE 6 RASTER BLINDS" },
    { 7, "MODE 7 AFFINE" },
};
#define NUM_PHASES (int)(sizeof(phases) / sizeof(phases[0]))
#define PHASE_FRAMES 360

static void draw_label(uint32_t buf, const char *text)
{
    volatile uint32_t *p = (volatile uint32_t *)buf;
    for (uint32_t y = LCD_H - LABEL_H; y < LCD_H; y++)
        for (uint32_t x = 0; x < LCD_W; x++)
            p[y * LCD_W + x] = (y == LCD_H - LABEL_H) ? 0xFF48A090
                                                      : 0xFF10141E;
    draw_text(p, LCD_W, text,
              (LCD_W - text_w(text, 2)) / 2, LCD_H - LABEL_H + 5,
              2, 0xFFF2F2E8);
}

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    irq_init();
    hstimer_init();
    input_init(INPUT_N64);

    uart_puts("\n=== Jupiter Modes Tour ===\n");

    video_init();               /* leaves us in Mode 1 */
    cedar_init();

    /* Decode the probe frame once for Mode 4 (video_init first — it
     * clears the codec buffer region) */
    int cedar_ok = cedar_h264_decode(test_h264, test_h264_size,
                                     64, 64, 36, 10, 0, 0, 1) == 0;
    uart_puts(cedar_ok ? "[tour] cedar frame ready\n"
                       : "[tour] cedar decode failed - mode 4 shows text only\n");

    init_sin();
    init_m7();
    gen_orb(0xFFFFF6D0, 0xFFFFB84E);

    /* Static background scene on FB0 (single-buffered on purpose —
     * only Mode 7 redraws it, and full-frame) */
    draw_scene(FB0_ADDR);
    dcache_clean_fb(FB0_ADDR);
    video_swap(FB0_ADDR);

    /* VI1 pattern buffer (Mode 2 window content, animated cheaply) */
    #define PIP_W 96
    #define PIP_H 64

    irq_global_enable();
    input_settle();   /* no phantom first-poll edge skipping phase 0 */

    int phase = -1, want = 0;
    uint32_t frame = 0, pf = 0;
    uint32_t held_prev = 0;

    while (1) {
        /* ---- phase transitions ---- */
        if (want != phase) {
            /* leave old phase */
            if (phase >= 0 && phases[phase].mode == 4) video_mode4_off();
            if (phase >= 0 && phases[phase].mode == 5) video_mode5_off();
            if (phase >= 0 && phases[phase].mode == 7)
                video_mode7_lineshift_off();
            raster_on = 0;
            hstimer_stop(0);

            phase = want;
            int m = phases[phase].mode;
            uart_puts("[tour] "); uart_puts(phases[phase].label);
            uart_puts("\n");

            /* base config for the phase */
            video_mode(m == 4 || m == 6 ? 1 : m);

            if (m == 5) {
                /* second playfield: warm inverse of the dusk scene —
                 * chunky amber bands + grid, drawn once into FB1 */
                volatile uint32_t *pf = (volatile uint32_t *)FB1_ADDR;
                for (uint32_t y = 0; y < LCD_H / 2; y++)
                    for (uint32_t x = 0; x < LCD_W; x++) {
                        uint32_t c = sky_bands[7 - ((y * 8) / (LCD_H / 2))];
                        if ((x & 31) == 0 || (y & 31) == 0)
                            c = 0xFF48A090;
                        pf[y * LCD_W + x] = c;
                    }
                dcache_clean_fb(FB1_ADDR);
                video_mode5_split(FB0_ADDR, FB1_ADDR);
            }
            if (m == 7) {
                update_m7_shift(0);
                video_mode7_lineshift(m7_shift, M7_BANDS, 8);
            }

            if (m == 0 || m == 7) {
                /* label must live on VI0 — overlay is off/busy */
            }
            if (m == 2) {
                video_vi1_init((LCD_W - PIP_W) / 2, 28, PIP_W, PIP_H);
            }
            if (m == 4 && cedar_ok) {
                video_mode4_nv12(cedar_dec_luma_addr(),
                                 cedar_dec_chroma_addr(),
                                 64, 64, 64,
                                 (LCD_W - 64) / 2, 80);
            }
            if (m == 6) {
                raster_band = 0;
                raster_on = 1;
                hstimer_set_repeating(0, 16, raster_isr);
            }
            if (m != 7) {
                draw_scene(FB0_ADDR);
                draw_label(FB0_ADDR, phases[phase].label);
                dcache_clean_fb(FB0_ADDR);
            }
            pf = 0;
        }

        int m = phases[phase].mode;

        /* ---- per-frame content ---- */

        /* Mode 7: NEON redraws the rotozoom; the hardware lineshift
         * sways the scanout on top (bring-up: sway = LADDR mid-frame
         * latch works; static = it doesn't) */
        if (m == 7) {
            draw_m7(FB0_ADDR, frame);
            draw_label(FB0_ADDR, phases[phase].label);
            dcache_clean_fb(FB0_ADDR);
            update_m7_shift(frame);
        }

        /* overlay: orbs + label (label mirrored here so it survives
         * Mode 4, where VI0 shows the NV12 window instead of FB0) */
        memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
        {
            volatile uint32_t *ovl = (volatile uint32_t *)OVL_ADDR;
            for (int i = 0; i < 3; i++) {
                int a = (int)(frame + i * 85) & 255;
                int ox = LCD_W / 2 - ORB_W / 2 + ((COS(a) * 120) >> 8);
                int oy = 92 - ORB_H / 2 + ((SIN(a) * 44) >> 8);
                sprite_blit(ovl, LCD_W, orb_data, ORB_W, ORB_H, ox, oy);
            }
            draw_text(ovl, LCD_W, "JUPITER MODES",
                      (LCD_W - text_w("JUPITER MODES", 2)) / 2, 8,
                      2, 0xFFF2C14E);
            draw_label(OVL_ADDR, phases[phase].label);
        }
        dcache_clean_range(OVL_ADDR, LCD_W * LCD_H * 4);
        video_set_overlay(OVL_ADDR);

        /* Mode 2: animate the VI1 window pattern */
        if (m == 2) {
            volatile uint32_t *pip = (volatile uint32_t *)SPR_ADDR;
            for (int y = 0; y < PIP_H; y++)
                for (int x = 0; x < PIP_W; x++) {
                    int band = ((x + y + (int)(frame * 2)) >> 3) & 7;
                    pip[y * PIP_W + x] = sky_bands[band];
                }
            dcache_clean_range(SPR_ADDR, PIP_W * PIP_H * 4);
        }

        /* Mode 3: breathe the overlay via hw global alpha */
        if (m == 3)
            video_mode3_alpha((uint8_t)(144 + ((SIN(frame * 2) * 110) >> 8)));

        /* ---- input + auto-advance ---- */
        input_poll();
        uint32_t held = input_held();
        uint32_t pressed = held & ~held_prev;
        held_prev = held;
        if (pressed & (BTN_A | BTN_RIGHT)) want = (phase + 1) % NUM_PHASES;
        if (pressed & BTN_LEFT) want = (phase + NUM_PHASES - 1) % NUM_PHASES;
        if (++pf >= PHASE_FRAMES) want = (phase + 1) % NUM_PHASES;

        frame++;
        video_wait_vblank();
        if (m == 7)
            video_mode7_line_reset(FB0_ADDR);
        if (m == 6) {
            /* Phase-lock the blinds: restart the strobe period at
             * vblank so the bands hold still instead of crawling. */
            raster_band = 0;
            hstimer_set_repeating(0, 16, raster_isr);
        }
    }
    return 0;
}
