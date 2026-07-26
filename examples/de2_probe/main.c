/*
 * DE2 capability probe — three unanswered hardware questions, one flash
 *
 * PHASE A: BLENDER COLORKEY (round 3: window-exclusion sweep)
 *   The DE2 blend block carries CK_CTL/CK_CFG/CK_MAX/CK_MIN registers
 *   that Linux never drives and no public doc explains for the V3s.
 *   If they work, an opaque VI layer gets "magic pink" punch-through —
 *   hardware colorkey transparency, the classic 90s trick, and the
 *   missing piece for dual full-screen playfields.
 *   VI0 scans an animated gradient; VI1 sits above it, full screen,
 *   filled MAGENTA with white bars and text. The probe steps through
 *   12 CK_CTL/CK_CFG interpretations (A / Right = next, Left = prev),
 *   printing each to UART.
 *     -> Gradient shows through the magenta = COLORKEY WORKS at that
 *        config number. Note the number, tell the SDK.
 *     -> Magenta stays solid on all 12 = keying is fused/absent.
 *
 * PHASE B: VI CHANNEL SUB-WINDOWS (config 24) — CONFIRMED on
 * silicon: all four slots render (round 1 bench).
 *   Each VI channel has four overlay slots (ATTR/MBSIZE/COOR/PITCH/
 *   LADDR with a 0x30 stride). Linux only ever uses slot 0. The probe
 *   enables all four with different solid colors at staggered
 *   positions.
 *     -> FOUR squares (red, green, blue, white) = quad hardware
 *        windows per channel. Huge.
 *     -> ONE red square = slot 0 only, the rest are fused/ignored.
 *
 * PHASE C: WINDOW ANIMATION (config 25) — CONFIRMED smooth on
 * silicon: size+position tween tear-free (round 1 bench).
 *   Answers the original question: can a VI window's position AND
 *   dimensions animate per frame? The probe re-programs VI1's
 *   MBSIZE/OVL_SIZE/INSIZE/OFFSET every vblank — a breathing,
 *   orbiting box.
 *     -> Smooth pulse = yes, animate freely.
 *     -> Tearing/flicker/hang = dimensions want vblank-latch care.
 *
 * Build: make GAME=examples/de2_probe/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"

/* ---- tiny font (subset, 5x7) ---- */
typedef struct { char ch; const char *rows[7]; } glyph_t;
static const glyph_t font[] = {
    {'A',{".###.","#...#","#...#","#####","#...#","#...#","#...#"}},
    {'B',{"####.","#...#","#...#","####.","#...#","#...#","####."}},
    {'C',{".####","#....","#....","#....","#....","#....",".####"}},
    {'D',{"####.","#...#","#...#","#...#","#...#","#...#","####."}},
    {'E',{"#####","#....","#....","####.","#....","#....","#####"}},
    {'F',{"#####","#....","#....","####.","#....","#....","#...."}},
    {'G',{".###.","#....","#....","#.###","#...#","#...#",".###."}},
    {'I',{"#####","..#..","..#..","..#..","..#..","..#..","#####"}},
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
    {'W',{"#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},
    {'X',{"#...#",".#.#.","..#..","..#..","..#..",".#.#.","#...#"}},
    {'Y',{"#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."}},
    {'-',{".....",".....",".....","#####",".....",".....","....."}},
    {'=',{".....",".....","#####",".....","#####",".....","....."}},
    {'0',{".###.","#...#","#..##","#.#.#","##..#","#...#",".###."}},
    {'1',{"..#..",".##..","..#..","..#..","..#..","..#..","#####"}},
    {'2',{".###.","#...#","....#","..##.",".#...","#....","#####"}},
    {'3',{"####.","....#","....#",".###.","....#","....#","####."}},
    {'4',{"#...#","#...#","#...#","#####","....#","....#","....#"}},
    {'5',{"#####","#....","#....","####.","....#","....#","####."}},
    {'6',{".###.","#....","#....","####.","#...#","#...#",".###."}},
    {'7',{"#####","....#","...#.","..#..",".#...",".#...",".#..."}},
    {'8',{".###.","#...#","#...#",".###.","#...#","#...#",".###."}},
    {'9',{".###.","#...#","#...#",".####","....#","....#",".###."}},
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

/* ---- colorkey sweep table ----
 * ROUND 1 SILICON RESULTS (Lichee Pi Zero bench): configs with
 * CTL=0x03 / 0x07 REACT — the comparator is live, matching magenta
 * against MIN=MAX exactly — but with the polarity INVERTED from the
 * magic-pink convention: pixels MATCHING the key kept the top layer,
 * everything else (white bars, ink text) went transparent to the
 * VI0 gradient. So under enable bits 0+1 the key defines the top
 * layer's OPAQUE set. Round 1 only tried the direction-guess bits
 * (8+) with the single enable bit that does nothing on its own; it
 * never combined them with the working 0x03/0x07 enables.
 *
 * ROUND 2 (configs 12+): sweep the direction bits ON TOP of the
 * proven-reactive enables, hunting the match->transparent mode
 * (magenta punches through = magic pink found). */
#define KEY_COLOR 0x00FF00FF   /* magenta, no alpha bits */
/* ROUND 2 RESULT: bits 8-10 of CTL do nothing (all dir-bit combos
 * behaved identically to plain 0x03/0x07) — there is no direction
 * field there. Working model after two rounds:
 *   - CTL bit per PIPE: a key activates between adjacent pipes when
 *     both endpoints' bits are set (0x03 = key between P0 and P1).
 *   - Fixed semantics: TOP pipe pixel inside [MIN,MAX] -> top shown;
 *     outside -> bottom shows through. (The "inverse key".)
 * ROUND 3 exploits that as-is: WINDOW EXCLUSION. Key window
 * [0x000000, 0xFFFFFE] contains every color EXCEPT blue=0xFF —
 * magenta (FF00FF) falls outside -> punches to the gradient, while
 * white bars (B=E8) and ink text (B=1E) stay. Magic pink from the
 * observed semantics, no polarity flip needed: art simply avoids
 * pure-0xFF blue (invisible restriction). Plus channel-mapping and
 * invert-bit probes for CFG. */
static const struct { uint32_t ctl, cfg, min, max; } ck_sweep[] = {
    /* 0-2: round-1 regression baseline */
    { 0x01, 0x00000007, KEY_COLOR,  KEY_COLOR  },  /* null control     */
    { 0x03, 0x00000007, KEY_COLOR,  KEY_COLOR  },  /* inverse-key hit  */
    { 0x07, 0x00000007, KEY_COLOR,  KEY_COLOR  },  /* inverse-key hit  */
    /* 3-5: window exclusion — 3 is THE magic-pink candidate */
    { 0x03, 0x00000007, 0x00000000, 0x00FFFFFE },  /* exclude B=FF: WIN if magenta punches */
    { 0x03, 0x00000007, 0x00000000, 0x00FEFFFF },  /* exclude R=FF: should also punch      */
    { 0x03, 0x00000007, 0x00000000, 0x00FFFEFF },  /* exclude G=FF: CONTROL - no punch     */
    /* 6-8: CFG channel-map probes (single bit, exact-magenta key) */
    { 0x03, 0x00000001, KEY_COLOR,  KEY_COLOR  },
    { 0x03, 0x00000002, KEY_COLOR,  KEY_COLOR  },
    { 0x03, 0x00000004, KEY_COLOR,  KEY_COLOR  },
    /* 9-12: CFG invert-bit guesses (a set bit above the channel
     * trio flipping match->transparent would beat the window trick) */
    { 0x03, 0x0000000F, KEY_COLOR,  KEY_COLOR  },
    { 0x03, 0x00000070, KEY_COLOR,  KEY_COLOR  },
    { 0x03, 0x00000077, KEY_COLOR,  KEY_COLOR  },
    { 0x03, 0x00000700, KEY_COLOR,  KEY_COLOR  },
};
#define NUM_CK   ((int)(sizeof(ck_sweep) / sizeof(ck_sweep[0])))
#define PH_SUBWIN (NUM_CK)      /* after the CK sweep */
#define PH_ANIM   (NUM_CK + 1)
#define NUM_PH    (NUM_CK + 2)

/* VI1 full-screen buffer lives in the SPR slot (fits: 510KB < 512KB) */
#define VI1_BUF SPR_ADDR

static void draw_key_pattern(void)
{
    volatile uint32_t *p = (volatile uint32_t *)VI1_BUF;
    for (uint32_t y = 0; y < LCD_H; y++)
        for (uint32_t x = 0; x < LCD_W; x++) {
            uint32_t c = 0xFF000000 | KEY_COLOR;      /* magenta field */
            if (((x + y) & 63) < 8) c = 0xFFF2F2E8;   /* white bars    */
            p[y * LCD_W + x] = c;
        }
    draw_text(p, LCD_W, "MAGENTA IS KEYED", 120, 40, 2, 0xFF10141E);
    draw_text(p, LCD_W, "SEE GRADIENT = WIN", 108, 200, 2, 0xFF10141E);
    dcache_clean_fb(VI1_BUF);
}

static void apply_ck(int i)
{
    /* route: P0=VI0, P1=VI1 (adjacent, keys pair adjacent pipes),
     * P2=UI0 label layer */
    VI1_ATTR(0)       = VI_ATTR_EN | VI_FMT_XRGB8888;
    VI1_MBSIZE(0)     = WH(LCD_W, LCD_H);
    VI1_COOR(0)       = 0;
    VI1_PITCH0(0)     = LCD_PITCH;
    VI1_TOP_LADDR0(0) = VI1_BUF;
    VI1_OVL_SIZE(0)   = WH(LCD_W, LCD_H);
    BLD_FCOLOR(1)     = 0xFF000000;
    BLD_INSIZE(1)     = WH(LCD_W, LCD_H);
    BLD_OFFSET(1)     = 0;
    BLD_MODE(1)       = BLEND_DEF;
    BLD_ROUTE    = ROUTE_P(0, 0) | ROUTE_P(1, 1) | ROUTE_P(2, 2);
    BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_EN(2) | PIPE_FC(0);

    for (int k = 0; k < 3; k++) {
        BLD_CK_MAX(k) = ck_sweep[i].max;
        BLD_CK_MIN(k) = ck_sweep[i].min;
    }
    BLD_CK_CTL = ck_sweep[i].ctl;
    BLD_CK_CFG = ck_sweep[i].cfg;
    MIX_GLB_DBUF = DBUF_EN;

    uart_puts("[probe] CK cfg "); uart_putdec((uint32_t)i);
    uart_puts(": CTL="); uart_puthex(ck_sweep[i].ctl);
    uart_puts(" CFG="); uart_puthex(ck_sweep[i].cfg);
    uart_puts(" MIN="); uart_puthex(ck_sweep[i].min);
    uart_puts(" MAX="); uart_puthex(ck_sweep[i].max);
    uart_puts("\n");
}

static void ck_off(void)
{
    BLD_CK_CTL = 0;
    BLD_CK_CFG = 0;
    MIX_GLB_DBUF = DBUF_EN;
}

/* ---- sub-window probe ---- */
#define SUB_W 64
#define SUB_H 64
static void apply_subwin(void)
{
    static const uint32_t colors[4] =
        { 0xFFE04848, 0xFF48C060, 0xFF4878E0, 0xFFF2F2E8 };
    /* four 64x64 solid buffers packed after the key pattern buffer */
    for (int i = 0; i < 4; i++) {
        volatile uint32_t *b =
            (volatile uint32_t *)(VI1_BUF + 0x60000 + i * SUB_W * SUB_H * 4);
        for (int j = 0; j < SUB_W * SUB_H; j++) b[j] = colors[i];
        dcache_clean_range(VI1_BUF + 0x60000 + i * SUB_W * SUB_H * 4,
                           SUB_W * SUB_H * 4);
    }

    for (int i = 0; i < 4; i++) {
        uint32_t x = 40 + (uint32_t)i * 100, y = 60 + (uint32_t)i * 30;
        VI1_ATTR(i)       = VI_ATTR_EN | VI_FMT_XRGB8888;
        VI1_MBSIZE(i)     = WH(SUB_W, SUB_H);
        VI1_COOR(i)       = (y << 16) | x;   /* position within channel */
        VI1_PITCH0(i)     = SUB_W * 4;
        VI1_TOP_LADDR0(i) = VI1_BUF + 0x60000 + (uint32_t)i * SUB_W * SUB_H * 4;
    }
    VI1_OVL_SIZE(0) = WH(LCD_W, LCD_H);      /* channel canvas          */
    BLD_INSIZE(1)   = WH(LCD_W, LCD_H);
    BLD_OFFSET(1)   = 0;
    BLD_ROUTE    = ROUTE_P(0, 0) | ROUTE_P(1, 1) | ROUTE_P(2, 2);
    BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_EN(2) | PIPE_FC(0);
    MIX_GLB_DBUF = DBUF_EN;

    uart_puts("[probe] SUBWIN: 4 slots enabled "
              "(red/green/blue/white staggered)\n");
}

static void subwin_off(void)
{
    for (int i = 1; i < 4; i++) VI1_ATTR(i) = 0;
    MIX_GLB_DBUF = DBUF_EN;
}

/* ---- animation probe ---- */
static void apply_anim_frame(uint32_t frame, const int16_t *sinq8)
{
    #define SINQ(a) sinq8[(a) & 255]
    uint32_t w = (uint32_t)(120 + ((SINQ(frame * 2) * 60) >> 8));
    uint32_t h = (uint32_t)(80 + ((SINQ(frame * 2 + 64) * 40) >> 8));
    uint32_t x = (uint32_t)(LCD_W / 2 - w / 2 + ((SINQ(frame) * 100) >> 8));
    uint32_t y = (uint32_t)(LCD_H / 2 - h / 2 + ((SINQ(frame * 3) * 40) >> 8));
    VI1_ATTR(0)       = VI_ATTR_EN | VI_FMT_XRGB8888;
    VI1_MBSIZE(0)     = WH(w, h);
    VI1_COOR(0)       = 0;
    VI1_PITCH0(0)     = LCD_PITCH;           /* window into key pattern */
    VI1_TOP_LADDR0(0) = VI1_BUF;
    VI1_OVL_SIZE(0)   = WH(w, h);
    BLD_INSIZE(1)     = WH(w, h);
    BLD_OFFSET(1)     = (y << 16) | x;
    MIX_GLB_DBUF = DBUF_EN;
}

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    input_init(INPUT_N64);

    uart_puts("\n=== DE2 capability probe ===\n");
    uart_puts("A/Right = next config, Left = prev. Configs:\n");
    uart_puts("  0-12 colorkey round 3 (3 = magic-pink candidate), then sub-windows, animation\n\n");

    video_init();
    video_mode(2);   /* VI0 + VI1 + UI0 routing baseline */

    /* sin LUT for the gradient animation + anim phase */
    static int16_t sinq8[256];
    for (int i = 0; i < 128; i++) {
        int v = (4 * i * (128 - i) * 256) / (128 * 128);
        sinq8[i] = (int16_t)v;
        sinq8[128 + i] = (int16_t)-v;
    }

    draw_key_pattern();
    input_settle();   /* a phantom first-poll A would skip config 0 */

    int phase = -1, want = 0;
    uint32_t frame = 0, held_prev = 0;

    while (1) {
        if (want != phase) {
            if (phase >= 0 && phase < NUM_CK) ck_off();
            if (phase == PH_SUBWIN) subwin_off();
            phase = want;

            /* label on the UI0 overlay */
            memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
            volatile uint32_t *ovl = (volatile uint32_t *)OVL_ADDR;
            for (uint32_t y = 0; y < 22; y++)
                for (uint32_t x = 0; x < LCD_W; x++)
                    ovl[y * LCD_W + x] = 0xE010141E;
            if (phase < NUM_CK) {
                char lbl[] = "CK CFG 00";
                lbl[7] = (char)('0' + phase / 10);
                lbl[8] = (char)('0' + phase % 10);
                draw_text(ovl, LCD_W, lbl, 8, 4, 2, 0xFFF2C14E);
                apply_ck(phase);
            } else if (phase == PH_SUBWIN) {
                draw_text(ovl, LCD_W, "SUBWINDOWS 4", 8, 4, 2, 0xFFF2C14E);
                apply_subwin();
            } else {
                draw_text(ovl, LCD_W, "ANIM WINDOW", 8, 4, 2, 0xFFF2C14E);
                uart_puts("[probe] ANIM: breathing window — watch for "
                          "tearing\n");
            }
            dcache_clean_range(OVL_ADDR, LCD_W * LCD_H * 4);
            video_set_overlay(OVL_ADDR);
        }

        /* VI0: animated gradient so punch-through is unmistakable */
        {
            volatile uint32_t *fb = (volatile uint32_t *)FB0_ADDR;
            uint32_t ph = frame & 255;
            for (uint32_t y = 0; y < LCD_H; y += 2) {
                uint32_t g = (y * 255 / LCD_H + ph) & 255;
                uint32_t c = 0xFF000000 | (g << 16) | ((255 - g) << 8) | 0x40;
                for (uint32_t x = 0; x < LCD_W; x++) {
                    fb[y * LCD_W + x] = c;
                    fb[(y + 1) * LCD_W + x] = c;
                }
            }
            dcache_clean_fb(FB0_ADDR);
        }

        if (phase == PH_ANIM)
            apply_anim_frame(frame, sinq8);

        input_poll();
        uint32_t held = input_held();
        uint32_t pressed = held & ~held_prev;
        held_prev = held;
        if (pressed & (BTN_A | BTN_RIGHT)) want = (phase + 1) % NUM_PH;
        if (pressed & BTN_LEFT) want = (phase + NUM_PH - 1) % NUM_PH;

        frame++;
        video_wait_vblank();
    }
    return 0;
}
