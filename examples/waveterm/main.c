/*
 * Jupiter SDK — WaveTerm (PPG Wave 2.3 / Behringer Wave editor)
 *
 * Faithful recreation of the classic PPG WaveTerm C UI: monochrome
 * green-on-black phosphor terminal with asterisk-bracketed page
 * titles, two-column data layout, and ten function-key labels above
 * ten numbered keys at the bottom of the screen.
 *
 * Reference: Hermann Seib's WaveTerm C gallery
 * (https://www.hermannseib.com/waveterm/gallery.shtml) and the
 * actual hardware photos.
 *
 * Pages (mirrors real WaveTerm page numbering):
 *   PAGE 0  System / Communication Management
 *   PAGE 1  Voice Programs
 *   PAGE 2  Wavetable Display & Edit
 *   PAGE 3  Transient Sound Tool (sampling)
 *   PAGE 5  Sequence Editor
 *   PAGE 9  File Search / Disk
 *
 * N64 controller mapping (matches the bottom F-key row 1..0):
 *   F1=ESCAPE  (B)
 *   F2=EXECUTE (A)
 *   F3=LEFT    (D-pad ←)
 *   F4=RIGHT   (D-pad →)
 *   F5=DOWN    (D-pad ↓)
 *   F6=UP      (D-pad ↑)
 *   F7=SET     (Z trigger)
 *   F8=DELETE  (C-down)
 *   F9=SCREEN  (R shoulder = next page; L = previous)
 *   F0=HELP    (C-up)
 *
 * Build: make GAME=examples/waveterm/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>

/* ================================================================
 *  5x7 font (column-encoded). Same glyph set as the other editors.
 * ================================================================ */
#define FNT_W 5
#define FNT_H 7
typedef struct { char ch; uint8_t cols[FNT_W]; } glyph_t;

static const glyph_t font[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'A', {0x7E,0x09,0x09,0x09,0x7E}}, {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}}, {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}}, {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}}, {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}}, {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}}, {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}}, {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}}, {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}}, {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}}, {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}}, {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x7F,0x20,0x18,0x20,0x7F}}, {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}}, {'Z', {0x61,0x51,0x49,0x45,0x43}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}}, {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}}, {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}}, {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}}, {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}}, {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'-', {0x08,0x08,0x08,0x08,0x08}}, {':', {0x00,0x36,0x36,0x00,0x00}},
    {'.', {0x00,0x60,0x60,0x00,0x00}}, {',', {0x00,0x50,0x30,0x00,0x00}},
    {'/', {0x20,0x10,0x08,0x04,0x02}}, {'+', {0x08,0x08,0x3E,0x08,0x08}},
    {'_', {0x40,0x40,0x40,0x40,0x40}}, {'(', {0x00,0x1C,0x22,0x41,0x00}},
    {')', {0x00,0x41,0x22,0x1C,0x00}}, {'[', {0x00,0x7F,0x41,0x41,0x00}},
    {']', {0x00,0x41,0x41,0x7F,0x00}}, {'<', {0x08,0x14,0x22,0x41,0x00}},
    {'>', {0x00,0x41,0x22,0x14,0x08}}, {'=', {0x14,0x14,0x14,0x14,0x14}},
    {'*', {0x14,0x08,0x3E,0x08,0x14}}, {'#', {0x14,0x7F,0x14,0x7F,0x14}},
    {'!', {0x00,0x00,0x5F,0x00,0x00}}, {'?', {0x02,0x01,0x51,0x09,0x06}},
    {'|', {0x00,0x00,0x7F,0x00,0x00}}, {'%', {0x23,0x13,0x08,0x64,0x62}},
    {'@', {0x32,0x49,0x79,0x41,0x3E}}, {'\'',{0x00,0x05,0x03,0x00,0x00}},
};
#define FONT_N (sizeof(font)/sizeof(font[0]))

static const uint8_t *glyph_for(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (uint32_t i = 0; i < FONT_N; i++)
        if (font[i].ch == c) return font[i].cols;
    return font[0].cols;
}

static void draw_text(uint32_t *fb, const char *s, int x, int y, int size, uint32_t color)
{
    while (*s) {
        const uint8_t *g = glyph_for(*s);
        for (int col = 0; col < FNT_W; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < FNT_H; row++) {
                if (!(bits & (1 << row))) continue;
                for (int dy = 0; dy < size; dy++) {
                    int py = y + row * size + dy;
                    if (py < 0 || py >= (int)LCD_H) continue;
                    uint32_t *line = fb + py * LCD_W;
                    for (int dx = 0; dx < size; dx++) {
                        int px = x + col * size + dx;
                        if (px < 0 || px >= (int)LCD_W) continue;
                        line[px] = color;
                    }
                }
            }
        }
        x += (FNT_W + 1) * size;
        s++;
    }
}

static int text_width(const char *s, int size)
{
    int n = 0; while (*s++) n++;
    return n * (FNT_W + 1) * size;
}

static void fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color)
{
    for (int py = y; py < y + h && py < (int)LCD_H; py++) {
        if (py < 0) continue;
        uint32_t *row = fb + py * LCD_W;
        for (int px = x; px < x + w && px < (int)LCD_W; px++) {
            if (px < 0) continue;
            row[px] = color;
        }
    }
}

static void int_to_str(int v, char *out)
{
    int n = 0; int neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    char tmp[8]; int len = 0;
    if (v == 0) tmp[len++] = '0';
    while (v > 0) { tmp[len++] = (char)('0' + v % 10); v /= 10; }
    if (neg) out[n++] = '-';
    while (len > 0) out[n++] = tmp[--len];
    out[n] = 0;
}

/* ================================================================
 *  Phosphor green palette — 4-step intensity for that CRT look.
 * ================================================================ */
#define COL_PHOS_BG     0xFF000800    /* near-black, slight green tint */
#define COL_PHOS_DIM    0xFF206030    /* dim green (separator dashes) */
#define COL_PHOS_MID    0xFF40A040    /* mid green (labels) */
#define COL_PHOS_HI     0xFF60E060    /* bright green (data/values) */
#define COL_PHOS_HOT    0xFFA0FF80    /* hot (cursor / selection) */
#define COL_PHOS_AMBER  0xFFE0C040    /* amber (warning highlights) */

/* CRT scanline post-pass: dim every odd row to ~62% to fake the
 * standard CRT raster look. */
static void apply_scanlines(uint32_t *fb)
{
    for (int y = 1; y < (int)LCD_H; y += 2) {
        uint32_t *row = fb + y * LCD_W;
        for (int x = 0; x < (int)LCD_W; x++) {
            uint32_t p = row[x];
            uint32_t a = p & 0xFF000000;
            uint32_t r = ((p >> 16) & 0xFF) * 5 >> 3;
            uint32_t g = ((p >>  8) & 0xFF) * 5 >> 3;
            uint32_t b = ( p        & 0xFF) * 5 >> 3;
            row[x] = a | (r << 16) | (g << 8) | b;
        }
    }
}

/* ================================================================
 *  Pages (mirrors real WaveTerm page numbering)
 * ================================================================ */
enum {
    PAGE_SYSTEM = 0,   /* page 0 — Communication Management */
    PAGE_VOICE,        /* page 1 — Voice Programs */
    PAGE_WAVETABLE,    /* page 2 — Wavetable Display & Edit */
    PAGE_SAMPLE,       /* page 3 — Transient Sound Tool */
    PAGE_SEQUENCE,     /* page 5 — Sequence Editor */
    PAGE_FILE,         /* page 9 — File Search */
    PAGE_COUNT,
};

static const int      page_numbers[PAGE_COUNT] = { 0, 1, 2, 3, 5, 9 };
static const char *page_titles[PAGE_COUNT] = {
    "COMMUNICATION MANAGEMENT",
    "VOICE PROGRAMS",
    "WAVETABLE DISPLAY",
    "TRANSIENT SOUND TOOL",
    "SEQUENCE EDITOR",
    "FILE SEARCH",
};

/* ================================================================
 *  Chrome — header bar, function-key row at bottom
 * ================================================================ */

/* Top header line:  ************ PPG WAVE-TERM SYSTEM **********
 * spans the full screen width, asterisks fill any leftover space. */
static void render_header_band(uint32_t *fb)
{
    const char *title = " PPG WAVE-TERM SYSTEM ";
    int title_w = text_width(title, 1);
    int avail = LCD_W - 12;
    int side_w = (avail - title_w) / 2;
    int stars_per_side = side_w / 6;
    char buf[80]; int n = 0;
    for (int i = 0; i < stars_per_side && n < (int)sizeof(buf) - 1; i++) buf[n++] = '*';
    buf[n] = 0;
    draw_text(fb, buf, 6, 4, 1, COL_PHOS_HI);
    draw_text(fb, title, 6 + side_w, 4, 1, COL_PHOS_HI);
    draw_text(fb, buf, 6 + side_w + title_w, 4, 1, COL_PHOS_HI);
}

/* Below header: version, page indicator, page title. */
static void render_meta_row(uint32_t *fb, int page)
{
    char pbuf[16]; int_to_str(page_numbers[page], pbuf);
    draw_text(fb, "Version: C  Rev 1.00.0001", 6, 14, 1, COL_PHOS_MID);
    draw_text(fb, "PAGE", 200, 14, 1, COL_PHOS_MID);
    draw_text(fb, pbuf, 232, 14, 1, COL_PHOS_HI);
    draw_text(fb, page_titles[page],
              LCD_W - text_width(page_titles[page], 1) - 6, 14, 1, COL_PHOS_HI);
}

/* Function key labels (row of 10) + numbered keys (1..0 underneath).
 * Sits at the very bottom of the CRT body. */
static const char *fkey_labels[PAGE_COUNT][10] = {
    /* PAGE 0 — Communication Management */
    { "ESCAPE", "RETRY", "COMPON", "DISPLY", "RECORD", "PLAYBK", "MULTI", "GROUP", "BANK", "PRINT" },
    /* PAGE 1 — Voice Programs */
    { "ESCAPE", "EXECUT", "LEFT", "RIGHT", "DOWN", "UP", "SET", "DELETE", "SCREEN", "HELP" },
    /* PAGE 2 — Wavetable */
    { "ESCAPE", "EXECUT", "LEFT", "RIGHT", "DOWN", "UP", "DRAW", "SMOOTH", "WAVE", "HELP" },
    /* PAGE 3 — Transient Sound Tool */
    { "ESCAPE", "EXECUT", "REWIND", "FWD", "DOWN", "UP", "RECORD", "PLAY", "MARK", "HELP" },
    /* PAGE 5 — Sequence */
    { "ESCAPE", "EXECUT", "LEFT", "RIGHT", "STEP-", "STEP+", "SET", "DELETE", "PLAY", "HELP" },
    /* PAGE 9 — File */
    { "ESCAPE", "LOAD", "LEFT", "RIGHT", "DOWN", "UP", "SAVE", "DELETE", "DIR", "HELP" },
};

static void render_fkey_row(uint32_t *fb, int page)
{
    int label_y = LCD_H - 22;
    int num_y   = LCD_H - 12;
    /* Each F-key cell is LCD_W / 10 wide. */
    int cell_w = LCD_W / 10;
    for (int i = 0; i < 10; i++) {
        int x = i * cell_w;
        /* Label centered horizontally in cell */
        const char *lbl = fkey_labels[page][i];
        int lw = text_width(lbl, 1);
        int lx = x + (cell_w - lw) / 2;
        draw_text(fb, lbl, lx, label_y, 1, COL_PHOS_MID);
        /* Number 1..0 centered below */
        char nb[2];
        nb[0] = (i < 9) ? (char)('1' + i) : '0';
        nb[1] = 0;
        int nw = text_width(nb, 1);
        int nx = x + (cell_w - nw) / 2;
        draw_text(fb, nb, nx, num_y, 1, COL_PHOS_HI);
    }
    /* Thin separator line above the label row */
    fill_rect(fb, 4, label_y - 4, LCD_W - 8, 1, COL_PHOS_DIM);
}

/* Section divider — dashes around a centered label. */
static void render_section_divider(uint32_t *fb, int y, const char *label)
{
    int lw = text_width(label, 1);
    int total = LCD_W - 12;
    int dashes_each = (total - lw - 4) / 12;   /* 6 px per dash glyph */
    char buf[80]; int n = 0;
    for (int i = 0; i < dashes_each && n < (int)sizeof(buf) - 1; i++) buf[n++] = '-';
    buf[n] = 0;
    draw_text(fb, buf, 6, y, 1, COL_PHOS_DIM);
    draw_text(fb, label, 6 + dashes_each * 6 + 2, y, 1, COL_PHOS_HI);
    draw_text(fb, buf, 6 + dashes_each * 6 + 2 + lw + 2, y, 1, COL_PHOS_DIM);
}

/* ================================================================
 *  Page 0 — Communication Management (modeled on the gallery shot)
 * ================================================================ */
static int g_cur_component = 0;
static int g_multi_sampling = 1;
static int g_load_hdu = 0;

static void render_page_system(uint32_t *fb)
{
    int y = 28;
    render_section_divider(fb, y, " Identification ");
    y += 12;
    draw_text(fb, "COMPONENT  VERSION", 6, y, 1, COL_PHOS_MID);
    y += 10;
    static const char *components[][2] = {
        {"0 WAV", "6"}, {"1 WAV", "6"}, {"2 EVU", "2"}, {"3 EVU", "1"},
        {"4 PRK", "1"}, {"5 NO LINE", ""}, {"6 NO LINE", ""}, {"7 NO LINE", ""},
    };
    for (int i = 0; i < 8; i++) {
        int is_sel = (i == g_cur_component);
        uint32_t c = is_sel ? COL_PHOS_HOT : COL_PHOS_HI;
        draw_text(fb, components[i][0], 6, y, 1, c);
        draw_text(fb, components[i][1], 80, y, 1, c);
        if (is_sel) draw_text(fb, ">", 0, y, 1, COL_PHOS_HOT);
        y += 9;
    }
    /* Bottom info block */
    y = LCD_H - 50;
    char buf[16];
    int_to_str(g_cur_component, buf);
    draw_text(fb, "SELECTED COMPONENT:", 6, y, 1, COL_PHOS_MID);
    draw_text(fb, buf, 130, y, 1, COL_PHOS_HI);
    y += 9;
    draw_text(fb, "MULTI-SAMPLING:", 6, y, 1, COL_PHOS_MID);
    int_to_str(g_multi_sampling, buf);
    draw_text(fb, buf, 130, y, 1, COL_PHOS_HI);
    y += 9;
    draw_text(fb, "LOAD FROM HDU:", 6, y, 1, COL_PHOS_MID);
    int_to_str(g_load_hdu, buf);
    draw_text(fb, buf, 130, y, 1, COL_PHOS_HI);
    /* Right column: memory banks header */
    y = 50;
    draw_text(fb, "M E M O R Y  -  B A N K S", 220, y, 1, COL_PHOS_HI);
    y += 12;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 8; col++) {
            char cell[6];
            cell[0] = 'T';
            int n = row * 8 + col + 12;
            cell[1] = (char)('0' + n / 100);
            cell[2] = (char)('0' + (n / 10) % 10);
            cell[3] = (char)('0' + n % 10);
            cell[4] = 0;
            draw_text(fb, cell, 220 + col * 30, y + row * 9, 1, COL_PHOS_MID);
        }
    }
}

/* ================================================================
 *  Page 1 — Voice Programs (placeholder content)
 * ================================================================ */
static int g_cur_prog = 0;

static void render_page_voice(uint32_t *fb)
{
    int y = 28;
    render_section_divider(fb, y, " VOICE PROGRAM PARAMETERS ");
    y += 12;
    char buf[16]; int_to_str(g_cur_prog + 1, buf);
    draw_text(fb, "PROGRAM #", 6, y, 1, COL_PHOS_MID);
    draw_text(fb, buf, 70, y, 1, COL_PHOS_HOT);
    draw_text(fb, "PPG BRASS 1", 110, y, 1, COL_PHOS_HI);
    y += 14;
    const char *lines[] = {
        "OSC 1   WAVETABLE  4    POS  12    DETUNE  +0",
        "OSC 2   WAVETABLE  9    POS  32    DETUNE  -7",
        "        SEMI 1     +0   SEMI 2    -12",
        "",
        "MIX     OSC1  80    OSC2  60    NOISE  0    SUB  0",
        "",
        "VCF     CUTOFF  90  RES 12  ENV +24  KEYF 100%",
        "",
        "ENV 1   A  4    D  30   S  60    R  50    (filter)",
        "ENV 2   A  1    D  5    S  100   R  24    (amp)",
        "ENV 3   A  0    D  8    S  70    R  24    (wave)",
        "",
        "LFO     RATE  50   DEPTH  8   DEST  pitch",
        "VOICE   poly 8     glide 0    bend +/- 2",
    };
    for (uint32_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++) {
        draw_text(fb, lines[i], 6, y, 1, COL_PHOS_MID);
        y += 10;
    }
}

/* ================================================================
 *  Page 2 — Wavetable Display
 * ================================================================ */
static int g_cur_wavetable = 4;
static int g_cur_wave_idx  = 23;

static void render_page_wavetable(uint32_t *fb)
{
    int y = 28;
    char buf[16];
    render_section_divider(fb, y, " WAVETABLE EDIT ");
    y += 12;
    draw_text(fb, "TABLE", 6, y, 1, COL_PHOS_MID);
    int_to_str(g_cur_wavetable, buf);
    draw_text(fb, buf, 38, y, 1, COL_PHOS_HOT);
    draw_text(fb, "/ 32", 56, y, 1, COL_PHOS_DIM);
    draw_text(fb, "WAVE", 110, y, 1, COL_PHOS_MID);
    int_to_str(g_cur_wave_idx, buf);
    draw_text(fb, buf, 142, y, 1, COL_PHOS_HOT);
    draw_text(fb, "/ 64", 160, y, 1, COL_PHOS_DIM);
    y += 14;
    /* Wave display box */
    int wx = 8, ww = LCD_W - 16, wh = 130;
    fill_rect(fb, wx, y, ww, 1, COL_PHOS_DIM);
    fill_rect(fb, wx, y + wh, ww, 1, COL_PHOS_DIM);
    fill_rect(fb, wx, y, 1, wh, COL_PHOS_DIM);
    fill_rect(fb, wx + ww - 1, y, 1, wh, COL_PHOS_DIM);
    /* Procedural waveform — sine + 3rd harmonic */
    static const int sin_tbl[64] = {
          0,  12,  24,  37,  48,  59,  70,  79,
         87,  94, 100, 104, 107, 109, 110, 109,
        107, 104, 100,  94,  87,  79,  70,  59,
         48,  37,  24,  12,   0, -12, -24, -37,
        -48, -59, -70, -79, -87, -94,-100,-104,
       -107,-109,-110,-109,-107,-104,-100, -94,
        -87, -79, -70, -59, -48, -37, -24, -12,
          0,  12,  24,  37,  48,  59,  70,  79,
    };
    int cy = y + wh / 2;
    int amp = wh / 2 - 6;
    for (int x = 2; x < ww - 2; x++) {
        int i  = (x * 64) / (ww - 4);
        int i3 = ((x + g_cur_wave_idx) * 192) / (ww - 4);
        int v = sin_tbl[i & 63] / 2 + sin_tbl[i3 & 63] / 4;
        int py = cy - (v * amp) / 110;
        if (py >= y + 1 && py < y + wh - 1)
            fb[py * LCD_W + (wx + x)] = COL_PHOS_HI;
    }
    y += wh + 6;
    draw_text(fb, "POS 23 / 64    HARM 1.0    OFFSET +0",
              6, y, 1, COL_PHOS_MID);
}

/* ================================================================
 *  Page 3 — Transient Sound Tool
 * ================================================================ */
static void render_page_sample(uint32_t *fb)
{
    int y = 28;
    render_section_divider(fb, y, " TRANSIENT SOUND TOOL ");
    y += 12;
    draw_text(fb, "SYSTEM COMPONENT: 0   LINE  0   BANK: 0   FCT: 0/02",
              6, y, 1, COL_PHOS_MID);
    y += 12;
    draw_text(fb, "MANUAL: RECORD", 6, y, 1, COL_PHOS_HI);
    y += 9;
    draw_text(fb, "TIME WAVE TYPE: 0   SAMPLE RATE: 41.61 kHz",
              6, y, 1, COL_PHOS_MID);
    y += 14;
    /* Sampled-waveform display */
    int wx = 8, ww = LCD_W - 16, wh = 120;
    fill_rect(fb, wx, y, ww, 1, COL_PHOS_DIM);
    fill_rect(fb, wx, y + wh, ww, 1, COL_PHOS_DIM);
    int cy = y + wh / 2;
    /* Procedural noise-y waveform */
    for (int x = 0; x < ww; x++) {
        int env = (x * x + (x ^ 0x37)) & 0x3F;
        int s = (((x * 17 + 3) ^ (x * 7)) & 0x7F) - 64;
        int v = (s * env) / 64;
        int py = cy - v / 2;
        if (py >= y && py < y + wh)
            fb[py * LCD_W + (wx + x)] = COL_PHOS_HI;
    }
    y += wh + 6;
    draw_text(fb, "COMMENT: NO LINE", 6, y, 1, COL_PHOS_DIM);
}

/* ================================================================
 *  Page 5 — Sequence Editor (placeholder)
 * ================================================================ */
static void render_page_sequence(uint32_t *fb)
{
    int y = 28;
    render_section_divider(fb, y, " SEQUENCE EDITOR ");
    y += 12;
    draw_text(fb, "SEQ 01  16 STEPS  TEMPO 120  TRACK 1/8",
              6, y, 1, COL_PHOS_MID);
    y += 14;
    /* 16-step grid */
    int sx = 8, sy = y, cw = (LCD_W - 16 - 2) / 16, ch = 12;
    for (int s = 0; s < 16; s++) {
        int x = sx + s * cw;
        fill_rect(fb, x, sy, cw - 2, ch, ((s & 3) == 0) ? COL_PHOS_DIM : COL_PHOS_BG);
        if (s == 5) fill_rect(fb, x, sy, cw - 2, ch, COL_PHOS_HOT);   /* fake "active step" */
        char nb[3]; nb[0] = (char)('0' + (s + 1) / 10); nb[1] = (char)('0' + (s + 1) % 10); nb[2] = 0;
        draw_text(fb, nb, x + 2, sy + 3, 1, COL_PHOS_MID);
    }
    y += ch + 8;
    /* Note rows */
    static const char *notes[] = {"NOTE 1: C 3 V100", "NOTE 2: E 3 V100", "NOTE 3: G 3 V100", "NOTE 4: -- ----"};
    for (uint32_t i = 0; i < sizeof(notes)/sizeof(notes[0]); i++) {
        draw_text(fb, notes[i], 6, y, 1, COL_PHOS_MID);
        y += 10;
    }
}

/* ================================================================
 *  Page 9 — File Search
 * ================================================================ */
static int g_cur_file = 0;

static void render_page_file(uint32_t *fb)
{
    int y = 28;
    render_section_divider(fb, y, " FILE SEARCH ");
    y += 12;
    draw_text(fb, "DEV: HDU 0   FREE: 184 KB", 6, y, 1, COL_PHOS_MID);
    y += 12;
    static const char *files[] = {
        "WT_BRASS.WAV    1024", "WT_STRINGS.WAV  2048", "WT_LEAD.WAV    1024",
        "SMP_KICK.PCM     256", "SMP_SNARE.PCM    256", "SEQ_TRACK1.SEQ   512",
        "PROG_BANK.PRG    768", "SYS_GLOBAL.SYS   128",
    };
    for (uint32_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        int is_sel = ((int)i == g_cur_file);
        uint32_t c = is_sel ? COL_PHOS_HOT : COL_PHOS_HI;
        if (is_sel) draw_text(fb, ">", 0, y, 1, COL_PHOS_HOT);
        draw_text(fb, files[i], 8, y, 1, c);
        y += 10;
    }
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void)
{
    uart_puts("\n\n=== WaveTerm (PPG Wave / Behringer Wave editor) ===\n");
    timer_init();
    mmu_init();
    pmu_init();
    video_init();
    input_init(INPUT_N64);

    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    int active_page = PAGE_SYSTEM;
    uint32_t back_fb  = FB1_ADDR;
    uint32_t front_fb = FB0_ADDR;

    while (1) {
        uint32_t t0 = timer_read();
        (void)input_poll();
        uint32_t pressed = input_pressed();

        /* Page switching: L = previous, R = next */
        if (pressed & BTN_L) active_page = (active_page + PAGE_COUNT - 1) % PAGE_COUNT;
        if (pressed & BTN_R) active_page = (active_page + 1) % PAGE_COUNT;

        /* Per-page nav */
        switch (active_page) {
        case PAGE_SYSTEM:
            if (pressed & BTN_UP)   g_cur_component = (g_cur_component + 8 - 1) % 8;
            if (pressed & BTN_DOWN) g_cur_component = (g_cur_component + 1) % 8;
            break;
        case PAGE_WAVETABLE:
            if (pressed & BTN_LEFT)  { if (g_cur_wave_idx > 0)  g_cur_wave_idx--; }
            if (pressed & BTN_RIGHT) { if (g_cur_wave_idx < 63) g_cur_wave_idx++; }
            if (pressed & BTN_UP)    { if (g_cur_wavetable > 0)  g_cur_wavetable--; }
            if (pressed & BTN_DOWN)  { if (g_cur_wavetable < 31) g_cur_wavetable++; }
            break;
        case PAGE_VOICE:
            if (pressed & BTN_UP)   { if (g_cur_prog > 0)  g_cur_prog--; }
            if (pressed & BTN_DOWN) { if (g_cur_prog < 99) g_cur_prog++; }
            break;
        case PAGE_FILE:
            if (pressed & BTN_UP)   g_cur_file = (g_cur_file + 8 - 1) % 8;
            if (pressed & BTN_DOWN) g_cur_file = (g_cur_file + 1) % 8;
            break;
        default: break;
        }

        uint32_t *fb = (uint32_t *)back_fb;
        fill_rect(fb, 0, 0, LCD_W, LCD_H, COL_PHOS_BG);

        render_header_band(fb);
        render_meta_row(fb, active_page);

        switch (active_page) {
        case PAGE_SYSTEM:    render_page_system(fb); break;
        case PAGE_VOICE:     render_page_voice(fb); break;
        case PAGE_WAVETABLE: render_page_wavetable(fb); break;
        case PAGE_SAMPLE:    render_page_sample(fb); break;
        case PAGE_SEQUENCE:  render_page_sequence(fb); break;
        case PAGE_FILE:      render_page_file(fb); break;
        }

        render_fkey_row(fb, active_page);
        apply_scanlines(fb);

        dcache_clean_fb(back_fb);
        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;
    }
    return 0;
}
