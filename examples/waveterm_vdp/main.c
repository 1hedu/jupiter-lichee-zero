/*
 * Jupiter SDK — WaveTerm VDP (Genesis-style port prototype)
 *
 * Same WaveTerm editor UI as examples/waveterm/, re-implemented
 * using the Genesis VDP renderer (lib/genesis.c) so it can eventually
 * be ported to actual Sega Genesis hardware.
 *
 * Constraints honored (Genesis VDP):
 *   - All graphics are 4bpp 8×8 tiles (32 bytes per tile)
 *   - One CRAM palette = 16 colors (we use phosphor-green shades)
 *   - Native 320×224 H40 resolution, pillarboxed on the 480×272 LCD
 *   - Plane B = solid CRT background (single repeating tile)
 *   - Plane A = text content (rebuilt into nametable on state change)
 *   - No per-pixel ARGB writes anywhere — pure tile + nametable
 *
 * What's intentionally missing vs the bitmap WaveTerm:
 *   - No CRT post-process (user explicitly asked for none)
 *   - No procedural waveform display on Page 2/3 (would need dynamic
 *     tile generation + upload per frame; left as future work)
 *   - Field positioning quantized to 8-pixel character cells
 *
 * Build: make GAME=examples/waveterm_vdp/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "genesis.h"
#include <string.h>

/* ================================================================
 *  Tile + palette layout
 *
 *  Tile 0       — fully transparent (Plane A) / solid BG (we use a
 *                 separate tile for that)
 *  Tile 1       — solid CRT background (filled with color index 1)
 *  Tile 2..3    — chrome accents (asterisk-like, divider dash)
 *  Tile 16..127 — ASCII font (' '..'~') = 96 glyphs
 *
 *  Palette (16 colors, one CRAM palette):
 *    0  transparent (Plane A) / used as backdrop/dark by Plane B
 *    1  near-black (CRT BG, slight green tint)
 *    2  dim green   (separators)
 *    3  mid green   (labels)
 *    4  bright green (data values)
 *    5  hot green   (cursor / selection)
 *    6  amber       (warning highlights)
 *    7..15  reserved for future use
 * ================================================================ */
#define TILE_BLANK       0
#define TILE_BG          1
#define TILE_SCOPE_BASE  16     /* 30 cols × 3 rows = 90 dynamic scope tiles */
#define SCOPE_TW         30
#define SCOPE_TH         3
#define SCOPE_PX_W       (SCOPE_TW * 8)
#define SCOPE_PX_H       (SCOPE_TH * 8)
#define TILE_PACKED_BASE 128    /* 2-char-per-tile packed text starts here   */
#define MAX_PAIR_TILES   320    /* room for ~320 unique char pairs per screen */
#define TILE_COUNT       (TILE_PACKED_BASE + MAX_PAIR_TILES)

#define COL_BG     1
#define COL_DIM    2
#define COL_MID    3
#define COL_HI     4
#define COL_HOT    5
#define COL_AMBER  6

static uint8_t  g_tiles[TILE_COUNT * 32];
static uint32_t g_cram[64];

/* Genesis VDP nametable dimensions MUST be powers of two — the
 * renderer masks `wx & (map_w-1)` to wrap. Real Genesis 64×32 is
 * the standard H40 size; we use that and only populate the first
 * VISIBLE_TW × VISIBLE_TH cells (40 × 28 = the actual 320×224 view).
 * Remaining cells stay as TILE_BLANK and never get scrolled into view. */
#define MAP_W         64
#define MAP_H         32
#define VISIBLE_TW    40
#define VISIBLE_TH    28
static uint16_t g_plane_a[MAP_W * MAP_H];
static uint16_t g_plane_b[MAP_W * MAP_H];

/* ================================================================
 *  Packed 4×8 font — 2 chars per 8×8 tile (Genesis-correct PPG
 *  density). 3-pixel-wide glyphs in 7 rows, placed at (0..2, 0..6)
 *  within a 4×8 cell with 1px right padding and 1px bottom padding.
 *  Two cells per tile = 80 char positions across the 320-px native
 *  screen — the same density a real PPG WaveTerm CRT shows.
 * ================================================================ */
#define FNT_W 3
#define FNT_H 7
typedef struct { char ch; uint8_t cols[FNT_W]; } glyph_t;

static const glyph_t font[] = {
    {' ', {0x00,0x00,0x00}},
    {'A', {0x7E,0x09,0x7E}}, {'B', {0x7F,0x49,0x36}},
    {'C', {0x3E,0x41,0x22}}, {'D', {0x7F,0x41,0x3E}},
    {'E', {0x7F,0x49,0x41}}, {'F', {0x7F,0x09,0x01}},
    {'G', {0x3E,0x49,0x7A}}, {'H', {0x7F,0x08,0x7F}},
    {'I', {0x41,0x7F,0x41}}, {'J', {0x30,0x40,0x3F}},
    {'K', {0x7F,0x08,0x77}}, {'L', {0x7F,0x40,0x40}},
    {'M', {0x7F,0x06,0x7F}}, {'N', {0x7F,0x04,0x7F}},
    {'O', {0x3E,0x41,0x3E}}, {'P', {0x7F,0x09,0x06}},
    {'Q', {0x3E,0x61,0x5E}}, {'R', {0x7F,0x19,0x66}},
    {'S', {0x46,0x49,0x31}}, {'T', {0x01,0x7F,0x01}},
    {'U', {0x3F,0x40,0x3F}}, {'V', {0x1F,0x60,0x1F}},
    {'W', {0x7F,0x30,0x7F}}, {'X', {0x63,0x1C,0x63}},
    {'Y', {0x07,0x78,0x07}}, {'Z', {0x71,0x49,0x47}},
    {'0', {0x3E,0x49,0x3E}}, {'1', {0x42,0x7F,0x40}},
    {'2', {0x62,0x51,0x46}}, {'3', {0x22,0x49,0x36}},
    {'4', {0x1C,0x12,0x7F}}, {'5', {0x47,0x45,0x39}},
    {'6', {0x3C,0x4A,0x30}}, {'7', {0x01,0x71,0x07}},
    {'8', {0x36,0x49,0x36}}, {'9', {0x06,0x29,0x1E}},
    {'-', {0x08,0x08,0x08}}, {':', {0x00,0x36,0x00}},
    {'.', {0x00,0x60,0x00}}, {',', {0x00,0x50,0x30}},
    {'/', {0x60,0x18,0x07}}, {'+', {0x08,0x3E,0x08}},
    {'_', {0x40,0x40,0x40}}, {'(', {0x00,0x3E,0x41}},
    {')', {0x41,0x3E,0x00}}, {'[', {0x7F,0x41,0x00}},
    {']', {0x00,0x41,0x7F}}, {'<', {0x08,0x14,0x22}},
    {'>', {0x22,0x14,0x08}}, {'=', {0x14,0x14,0x14}},
    {'*', {0x14,0x3E,0x14}}, {'#', {0x7F,0x14,0x7F}},
    {'!', {0x00,0x5F,0x00}}, {'?', {0x02,0x51,0x06}},
    {'|', {0x00,0x7F,0x00}}, {'%', {0x61,0x18,0x46}},
    {'@', {0x3E,0x49,0x3E}}, {'\'',{0x00,0x07,0x00}},
};
#define FONT_N (sizeof(font)/sizeof(font[0]))

/* Char → 3-byte-cols lookup. Built once at boot. */
static const uint8_t *g_glyph_cols[128];

static void init_glyph_lookup(void)
{
    for (int i = 0; i < 128; i++) g_glyph_cols[i] = font[0].cols; /* default: space */
    for (uint32_t i = 0; i < FONT_N; i++) {
        g_glyph_cols[(uint8_t)font[i].ch] = font[i].cols;
        /* Lower-case alias */
        if (font[i].ch >= 'A' && font[i].ch <= 'Z')
            g_glyph_cols[(uint8_t)(font[i].ch - 'A' + 'a')] = font[i].cols;
    }
}

/* Compose a tile holding two 3×7 glyphs side by side. Left glyph at
 * cols 0..2 of the tile, right glyph at cols 4..6. Cols 3 and 7 are
 * 1-pixel spacers. tile_idx must accept >255 since the packed cache
 * can produce indices up to TILE_PACKED_BASE + MAX_PAIR_TILES - 1. */
static void compose_pair_tile(unsigned int tile_idx, char left, char right, uint8_t color)
{
    uint8_t px[64];
    memset(px, 0, sizeof(px));
    const uint8_t *L = g_glyph_cols[(uint8_t)left];
    const uint8_t *R = g_glyph_cols[(uint8_t)right];
    for (int col = 0; col < FNT_W; col++) {
        uint8_t lb = L[col], rb = R[col];
        for (int row = 0; row < FNT_H; row++) {
            if (lb & (1 << row)) px[row * 8 + col]       = color;
            if (rb & (1 << row)) px[row * 8 + 4 + col]   = color;
        }
    }
    uint8_t *d = g_tiles + tile_idx * 32;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++)
            d[r * 4 + c] = (uint8_t)((px[r * 8 + c * 2] << 4) |
                                      px[r * 8 + c * 2 + 1]);
}

/* ================================================================
 *  Tile cache + 80-char text grid
 *
 *  The render functions write into g_text[80 × 28] (one char per
 *  cell, x in 0..79). After all text/scope is laid down, we walk
 *  the grid in 2-char pairs and emit packed tiles into the cache,
 *  populating the nametable. The cache resets on every screen
 *  rebuild — only pairs that actually appear consume tile slots.
 * ================================================================ */
/* Char-grid dimensions track the visible area (40 tiles × 28 rows),
 * not the nametable storage. 2 packed chars per tile → 80 chars per
 * visible row. Renderers compose into g_text using char positions
 * 0..79, which always land in the visible region of the nametable. */
#define TEXT_W (VISIBLE_TW * 2)   /* 80 char positions per row */
#define TEXT_H  VISIBLE_TH        /* 28 char rows */
static char g_text[TEXT_W * TEXT_H];

static struct {
    uint16_t key;             /* (left << 8) | right */
    uint8_t  valid;
} g_pair_cache[MAX_PAIR_TILES];
static int g_pair_count = 0;

static void cache_reset(void)
{
    g_pair_count = 0;
    for (int i = 0; i < MAX_PAIR_TILES; i++) g_pair_cache[i].valid = 0;
}

static uint16_t cache_get_tile(char left, char right)
{
    uint16_t key = (uint16_t)(((uint8_t)left << 8) | (uint8_t)right);
    for (int i = 0; i < g_pair_count; i++) {
        if (g_pair_cache[i].valid && g_pair_cache[i].key == key)
            return (uint16_t)(TILE_PACKED_BASE + i);
    }
    if (g_pair_count >= MAX_PAIR_TILES) return TILE_BLANK;
    int i = g_pair_count++;
    g_pair_cache[i].key = key;
    g_pair_cache[i].valid = 1;
    compose_pair_tile((unsigned int)(TILE_PACKED_BASE + i), left, right, COL_HI);
    return (uint16_t)(TILE_PACKED_BASE + i);
}

static void text_clear(void)
{
    for (int i = 0; i < TEXT_W * TEXT_H; i++) g_text[i] = ' ';
}

static void text_put_char(int char_x, int y, char c)
{
    if (char_x < 0 || char_x >= TEXT_W || y < 0 || y >= TEXT_H) return;
    g_text[y * TEXT_W + char_x] = c;
}

static void text_put_str(int char_x, int y, const char *s)
{
    while (*s && char_x < TEXT_W) {
        text_put_char(char_x++, y, *s++);
    }
}

static void text_put_int(int char_x, int y, int v)
{
    char buf[12]; int n = 0, neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    char tmp[12]; int len = 0;
    if (v == 0) tmp[len++] = '0';
    while (v > 0) { tmp[len++] = (char)('0' + v % 10); v /= 10; }
    if (neg) buf[n++] = '-';
    while (len > 0) buf[n++] = tmp[--len];
    buf[n] = 0;
    text_put_str(char_x, y, buf);
}

/* After all text is laid into g_text and the scope tiles are placed,
 * convert the text grid into packed tile nametable entries. */
static void build_nametable_from_text(void)
{
    cache_reset();
    /* Only walk the VISIBLE portion of the nametable. Cells outside
     * the 40×28 visible region were cleared to TILE_BLANK by the
     * preceding map_clear call and stay that way — they exist solely
     * so the nametable can satisfy the VDP's power-of-2 mask. */
    for (int ty = 0; ty < VISIBLE_TH; ty++) {
        for (int tx = 0; tx < VISIBLE_TW; tx++) {
            uint16_t existing = g_plane_a[ty * MAP_W + tx];
            uint16_t exist_tile = GEN_GET_TILE(existing);
            if (exist_tile >= TILE_SCOPE_BASE && exist_tile < TILE_SCOPE_BASE + SCOPE_TW * SCOPE_TH)
                continue;
            char left  = g_text[ty * TEXT_W + tx * 2];
            char right = g_text[ty * TEXT_W + tx * 2 + 1];
            if (left == ' ' && right == ' ') {
                g_plane_a[ty * MAP_W + tx] = (uint16_t)GEN_ENTRY(TILE_BLANK, 0, 0, 0);
                continue;
            }
            uint16_t tile = cache_get_tile(left, right);
            g_plane_a[ty * MAP_W + tx] = (uint16_t)GEN_ENTRY(tile, 0, 0, 0);
        }
    }
}

/* Fill a tile with one solid color index. */
static void fill_tile_with(uint8_t tile_idx, uint8_t color)
{
    uint8_t *d = g_tiles + tile_idx * 32;
    uint8_t b = (uint8_t)((color << 4) | color);
    for (int i = 0; i < 32; i++) d[i] = b;
}


/* ================================================================
 *  Tile / palette init
 * ================================================================ */
static void init_palette(void)
{
    /* Use only palette 0 (entries 0..15). Other palettes left zeroed. */
    g_cram[0]  = 0xFF000000;          /* transparent placeholder */
    g_cram[1]  = 0xFF001008;          /* CRT background */
    g_cram[2]  = 0xFF206030;          /* dim green */
    g_cram[3]  = 0xFF40A040;          /* mid green */
    g_cram[4]  = 0xFF60E060;          /* bright green */
    g_cram[5]  = 0xFFA0FF80;          /* hot green */
    g_cram[6]  = 0xFFE0C040;          /* amber */
    for (int i = 7; i < 16; i++) g_cram[i] = 0xFF000000;
    for (int i = 16; i < 64; i++) g_cram[i] = 0xFF000000;
}

static void init_tiles(void)
{
    memset(g_tiles, 0, sizeof(g_tiles));
    fill_tile_with(TILE_BLANK, 0);
    fill_tile_with(TILE_BG,    COL_BG);
    init_glyph_lookup();
    /* Packed-pair tiles are composed on-demand from g_text into
     * TILE_PACKED_BASE+... by cache_get_tile during nametable build. */
}

static void map_clear(uint16_t *map, uint16_t entry)
{
    for (int i = 0; i < MAP_W * MAP_H; i++) map[i] = entry;
}


/* ================================================================
 *  Soft tile renderer — the genuinely-Genesis trick
 *
 *  For dynamic content (oscilloscopes, lifebars, real-time graphs)
 *  Genesis games allocate a fixed range of tile slots in VRAM, then
 *  re-upload the tile pixel data each time the content changes. The
 *  nametable entries that reference those tiles stay constant —
 *  only the underlying tile bytes get rewritten via DMA. On a real
 *  Genesis this would be a VDP DMA copy; here we just write into
 *  g_tiles[] in main memory.
 *
 *  We dedicate tile indices [TILE_SCOPE_BASE, TILE_SCOPE_BASE +
 *  SCOPE_TW*SCOPE_TH) for the scope: 30 × 3 = 90 tiles = 240 × 24
 *  pixels. The render functions below build a 240×24 pixel buffer
 *  in scratch, then slice it back into 4bpp tile bytes.
 * ================================================================ */
static uint8_t g_scope_px[SCOPE_PX_W * SCOPE_PX_H];   /* paletted */

/* Pack the scope's pixel buffer into the dedicated tile range. */
static void scope_pack_tiles(void)
{
    for (int ty = 0; ty < SCOPE_TH; ty++) {
        for (int tx = 0; tx < SCOPE_TW; tx++) {
            uint8_t *dst = g_tiles + (TILE_SCOPE_BASE + ty * SCOPE_TW + tx) * 32;
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 4; c++) {
                    uint8_t p0 = g_scope_px[(ty * 8 + r) * SCOPE_PX_W + tx * 8 + c * 2];
                    uint8_t p1 = g_scope_px[(ty * 8 + r) * SCOPE_PX_W + tx * 8 + c * 2 + 1];
                    dst[r * 4 + c] = (uint8_t)((p0 << 4) | p1);
                }
            }
        }
    }
}

/* Place the scope tiles into the nametable at (x_cells, y_cells). */
static void scope_lay_nametable(uint16_t *map, int x_cells, int y_cells, uint8_t pal)
{
    for (int ty = 0; ty < SCOPE_TH; ty++) {
        for (int tx = 0; tx < SCOPE_TW; tx++) {
            int cx = x_cells + tx, cy = y_cells + ty;
            if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) continue;
            uint16_t tile_idx = (uint16_t)(TILE_SCOPE_BASE + ty * SCOPE_TW + tx);
            map[cy * MAP_W + cx] = (uint16_t)GEN_ENTRY(tile_idx, pal, 0, 0);
        }
    }
}

/* Render a sine-plus-3rd-harmonic waveform into g_scope_px, then
 * re-pack into tile bytes. Called from render_page_wavetable. */
static void scope_render_wavetable(int wave_idx)
{
    /* Clear to BG (color index 1) */
    for (int i = 0; i < SCOPE_PX_W * SCOPE_PX_H; i++) g_scope_px[i] = COL_BG;
    /* Border */
    for (int x = 0; x < SCOPE_PX_W; x++) {
        g_scope_px[0 * SCOPE_PX_W + x] = COL_DIM;
        g_scope_px[(SCOPE_PX_H - 1) * SCOPE_PX_W + x] = COL_DIM;
    }
    for (int y = 0; y < SCOPE_PX_H; y++) {
        g_scope_px[y * SCOPE_PX_W + 0] = COL_DIM;
        g_scope_px[y * SCOPE_PX_W + (SCOPE_PX_W - 1)] = COL_DIM;
    }
    /* Waveform */
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
    int cy = SCOPE_PX_H / 2;
    int amp = (SCOPE_PX_H / 2) - 2;
    for (int x = 1; x < SCOPE_PX_W - 1; x++) {
        int i  = (x * 64) / SCOPE_PX_W;
        int i3 = ((x + wave_idx) * 192) / SCOPE_PX_W;
        int v = sin_tbl[i & 63] / 2 + sin_tbl[i3 & 63] / 4;
        int py = cy - (v * amp) / 110;
        if (py > 0 && py < SCOPE_PX_H - 1)
            g_scope_px[py * SCOPE_PX_W + x] = COL_HOT;
    }
    scope_pack_tiles();
}

/* Render a noise-shaped sampled waveform for the Sample page. */
static void scope_render_sample(int sample_slot)
{
    for (int i = 0; i < SCOPE_PX_W * SCOPE_PX_H; i++) g_scope_px[i] = COL_BG;
    for (int x = 0; x < SCOPE_PX_W; x++) {
        g_scope_px[0 * SCOPE_PX_W + x] = COL_DIM;
        g_scope_px[(SCOPE_PX_H - 1) * SCOPE_PX_W + x] = COL_DIM;
    }
    int cy = SCOPE_PX_H / 2;
    uint32_t seed = (uint32_t)sample_slot * 0x9E3779B1u;
    for (int x = 0; x < SCOPE_PX_W; x++) {
        int env = (x * x + (x ^ 0x37)) & 0x3F;
        int s = (int)((((seed + (uint32_t)(x * 17 + 3)) ^ (uint32_t)(x * 7)) & 0x7F) - 64);
        int v = (s * env) / 64;
        int py = cy - v / 6;
        if (py >= 0 && py < SCOPE_PX_H)
            g_scope_px[py * SCOPE_PX_W + x] = COL_HOT;
    }
    scope_pack_tiles();
}

/* ================================================================
 *  Pages — same numbering as the bitmap WaveTerm
 * ================================================================ */
enum {
    PAGE_SYSTEM = 0, PAGE_VOICE, PAGE_WAVETABLE, PAGE_SAMPLE,
    PAGE_SEQUENCE, PAGE_FILE, PAGE_COUNT,
};
static const int   page_numbers[PAGE_COUNT] = { 0, 1, 2, 3, 5, 9 };
static const char *page_titles[PAGE_COUNT] = {
    "AUTOMATION", "VOICE PROGRAMS", "WAVETABLES",
    "TRANSIENTS", "SEQUENCE EDITOR", "FILE SEARCH",
};

static const char *fkey_labels[PAGE_COUNT][10] = {
    { "ESC","RTRY","CMPN","DSPL","RCRD","PBCK","MULT","GRP","BANK","PRNT" },
    { "ESC","EXEC","LEFT","RGHT","DOWN","UP","SET","DEL","SCRN","HELP" },
    { "ESC","EXEC","LEFT","RGHT","DOWN","UP","DRAW","SMTH","WAVE","HELP" },
    { "ESC","EXEC","RWND","FWD","DOWN","UP","RCRD","PLAY","MARK","HELP" },
    { "ESC","EXEC","LEFT","RGHT","STP-","STP+","SET","DEL","PLAY","HELP" },
    { "ESC","LOAD","LEFT","RGHT","DOWN","UP","SAVE","DEL","DIR","HELP" },
};

/* ================================================================
 *  Minimal data model (subset of bitmap WaveTerm's, just enough to
 *  prove the VDP path can drive interactive editing).
 * ================================================================ */
typedef struct {
    char    name[9];
    uint8_t osc1_tbl, osc1_pos;
    int8_t  osc1_semi;
    uint8_t mix_osc1, mix_osc2;
    uint8_t vcf_cutoff, vcf_resonance;
} program_t;

#define NUM_PROGRAMS 16
static program_t g_programs[NUM_PROGRAMS];
static int       g_cur_prog = 0;
static int       g_cur_field = 0;
static int       g_cur_component = 0;
static int       g_cur_wave_idx  = 0;
static int       g_cur_wavetable = 1;
static int       g_cur_file = 0;
static int       g_dirty = 1;

static void init_model(void)
{
    static const char *names[NUM_PROGRAMS] = {
        "PPGBRSS ","STRINGS1","SAWLEAD ","SUBBASS1",
        "PADWASH ","WAVEPAD ","HARPHIT ","CHOIRSYN",
        "BELLPLNK","FUNKBASS","ARPSEQ  ","NOISEFX ",
        "DRUMHIT ","FORMANT ","E.PIANO ","ORGWAVE ",
    };
    for (int i = 0; i < NUM_PROGRAMS; i++) {
        int n = 0;
        while (n < 8 && names[i][n]) {
            g_programs[i].name[n] = names[i][n]; n++;
        }
        while (n < 8) g_programs[i].name[n++] = ' ';
        g_programs[i].name[8] = 0;
        g_programs[i].osc1_tbl       = (uint8_t)(1 + (i & 31));
        g_programs[i].osc1_pos       = 0;
        g_programs[i].osc1_semi      = 0;
        g_programs[i].mix_osc1       = 100;
        g_programs[i].mix_osc2       = 80;
        g_programs[i].vcf_cutoff     = 90;
        g_programs[i].vcf_resonance  = 12;
    }
}

/* ================================================================
 *  Shared chrome — header band, page meta, F-key row
 * ================================================================ */
static void draw_chrome(int active_page)
{
    /* All coordinates here are in CHAR positions (TEXT_W = 80 wide,
     * TEXT_H = 28 tall). With the packed font we get true PPG-density
     * 80-char-per-row layout. */

    /* Top header — asterisk-bracketed title across row 0 */
    const char *title = " PPG WAVE-TERM SYSTEM ";
    int title_n = 0; while (title[title_n]) title_n++;
    int side = (TEXT_W - title_n) / 2;
    for (int i = 0; i < side; i++)
        text_put_char(i, 0, '*');
    text_put_str(side, 0, title);
    for (int i = 0; i < side; i++)
        text_put_char(side + title_n + i, 0, '*');

    /* Row 1 — version + page indicator + page title (right-aligned) */
    text_put_str(0, 1, "REV 1.00.0001");
    text_put_str(40, 1, "PAGE");
    text_put_int(45, 1, page_numbers[active_page]);
    const char *t = page_titles[active_page];
    int tn = 0; while (t[tn]) tn++;
    text_put_str(TEXT_W - tn, 1, t);

    /* Row 25 — separator (dashes across full width) */
    for (int x = 0; x < TEXT_W; x++) text_put_char(x, 25, '-');

    /* Row 26 — F-key labels, row 27 — numbers 1..0 */
    int cell_w = TEXT_W / 10;
    for (int i = 0; i < 10; i++) {
        int x = i * cell_w;
        const char *lbl = fkey_labels[active_page][i];
        int ln = 0; while (lbl[ln]) ln++;
        text_put_str(x + (cell_w - ln) / 2, 26, lbl);
        char nb[2];
        nb[0] = (i < 9) ? (char)('1' + i) : '0';
        nb[1] = 0;
        text_put_str(x + cell_w / 2, 27, nb);
    }
}

/* ================================================================
 *  Page renderers — write into the 80-char text grid (TEXT_W=80)
 * ================================================================ */

/* Helper: dashed divider with a centered label across the full row. */
static void divider(int y, const char *label)
{
    int ln = 0; while (label[ln]) ln++;
    int gap = (TEXT_W - ln - 2) / 2;
    for (int x = 0; x < gap; x++)              text_put_char(x, y, '-');
    text_put_char(gap, y, ' ');
    text_put_str(gap + 1, y, label);
    text_put_char(gap + 1 + ln, y, ' ');
    for (int x = gap + ln + 2; x < TEXT_W; x++) text_put_char(x, y, '-');
}

static void render_page_system(void)
{
    int y = 3;
    divider(y, "IDENTIFICATION"); y += 2;

    /* LEFT column: components (cols 2..35) */
    text_put_str(2, y, "COMPONENT          VERSION");
    y += 2;
    static const char *comps[8] = {
        "0 WAV                    6",
        "1 WAV                    6",
        "2 EVU                    2",
        "3 EVU                    1",
        "4 PRK                    1",
        "5 NO LINE",
        "6 NO LINE",
        "7 NO LINE",
    };
    for (int i = 0; i < 8; i++) {
        if (i == g_cur_component) text_put_char(0, y, '>');
        text_put_str(2, y, comps[i]);
        y += 1;
    }
    /* RIGHT column: memory banks (cols 42..78) */
    int by = 5;
    text_put_str(42, by, "M E M O R Y    B A N K S"); by += 2;
    text_put_str(42, by, "  #0   #1   #2   #3   #4"); by += 1;
    for (int row = 0; row < 4; row++) {
        char buf[8];
        for (int col = 0; col < 5; col++) {
            int n = row * 5 + col + 12;
            buf[0] = 'T';
            buf[1] = (char)('0' + n / 100);
            buf[2] = (char)('0' + (n / 10) % 10);
            buf[3] = (char)('0' + n % 10);
            buf[4] = 0;
            text_put_str(42 + col * 5, by + row, buf);
        }
    }

    /* Bottom info — single line */
    y = 16;
    text_put_str(2, y, "SELECTED COMPONENT:");
    text_put_int(22, y, g_cur_component);
    text_put_str(36, y, "MULTI-SAMPLING:");
    text_put_int(52, y, 1);
    text_put_str(60, y, "LOAD HDU:");
    text_put_int(70, y, 0);
    y += 2;
    text_put_str(2, y, "GROUP-ASSIGNMENT A = BANK #0    B = BANK #0");
}

static void render_page_voice(void)
{
    program_t *p = &g_programs[g_cur_prog];
    int y = 3;
    divider(y, "VOICE PROGRAM"); y += 2;
    text_put_str(2, y, "PROGRAM #");
    text_put_int(12, y, g_cur_prog + 1);
    text_put_str(16, y, "/ 16");
    text_put_str(24, y, "NAME:");
    text_put_str(30, y, p->name);
    y += 3;
    struct { const char *lbl; int val; const char *unit; } fields[] = {
        {"OSC1 WAVETABLE",  p->osc1_tbl,       " / 32"},
        {"OSC1 WAVE POS",   p->osc1_pos,       " / 64"},
        {"OSC1 SEMITONE",   p->osc1_semi,      ""},
        {"MIX  OSC1 LEVEL", p->mix_osc1,       " / 127"},
        {"MIX  OSC2 LEVEL", p->mix_osc2,       " / 127"},
        {"VCF  CUTOFF",     p->vcf_cutoff,     " / 127"},
        {"VCF  RESONANCE",  p->vcf_resonance,  " / 127"},
    };
    for (uint32_t i = 0; i < sizeof(fields)/sizeof(fields[0]); i++) {
        if ((int)i == g_cur_field) text_put_char(1, y, '>');
        text_put_str(3, y, fields[i].lbl);
        text_put_int(24, y, fields[i].val);
        text_put_str(30, y, fields[i].unit);
        y += 2;
    }
}
#define VOICE_FIELDS 7

static void render_page_wavetable(void)
{
    int y = 3;
    divider(y, "WAVETABLE EDIT"); y += 2;
    text_put_str(2, y, "TABLE");
    text_put_int(9, y, g_cur_wavetable);
    text_put_str(13, y, "/ 32");
    text_put_str(30, y, "WAVE");
    text_put_int(35, y, g_cur_wave_idx);
    text_put_str(39, y, "/ 64");
    text_put_str(60, y, "RAM BANK 0");
    y += 3;
    /* Soft-tile scope: 30 × 3 tiles = 240 × 24 px. Tile coords use
     * MAP_W, so x_cells=5 places at pixel x=40 (centered in 320-px). */
    scope_render_wavetable(g_cur_wave_idx);
    scope_lay_nametable(g_plane_a, /*x_cells=*/5, /*y_cells=*/y, 0);
    y += SCOPE_TH + 2;
    text_put_str(2, y, "POS");
    text_put_int(6, y, g_cur_wave_idx);
    text_put_str(10, y, "/ 64");
    text_put_str(22, y, "HARMONIC 1.0");
    text_put_str(46, y, "OFFSET +0");
    text_put_str(64, y, "PHASE 0");
}

static void render_page_sample(void)
{
    int y = 3;
    divider(y, "TRANSIENT SOUND TOOL"); y += 2;
    text_put_str(2, y, "SYSTEM COMPONENT: 0");
    text_put_str(28, y, "LINE 0");
    text_put_str(40, y, "BANK: 0");
    text_put_str(54, y, "PROGSEL");
    text_put_str(66, y, "FCT 0/02");
    y += 2;
    text_put_str(2, y, "MANUAL: RECORD");
    text_put_str(30, y, "AUTO-TRIGGER: OFF");
    y += 1;
    text_put_str(2, y, "WAVE TYPE: 0");
    text_put_str(22, y, "SAMPLE RATE: 41.6 KHZ");
    text_put_str(54, y, "LENGTH 16384");
    y += 2;
    scope_render_sample(0);
    scope_lay_nametable(g_plane_a, /*x_cells=*/5, /*y_cells=*/y, 0);
    y += SCOPE_TH + 2;
    text_put_str(2, y, "COMMENT: NO LINE");
    text_put_str(40, y, "LOOP: OFF");
    text_put_str(56, y, "START 0  END 0");
}

static void render_page_sequence(void)
{
    int y = 3;
    divider(y, "SEQUENCE EDITOR"); y += 2;
    text_put_str(2, y, "SEQ 01");
    text_put_str(14, y, "TEMPO 120");
    text_put_str(30, y, "SWING 50%");
    text_put_str(48, y, "TRACK 1/8");
    text_put_str(64, y, "LEN 16");
    y += 3;
    /* 16-step grid: 16 cells × 4 chars wide = 64 chars */
    for (int s = 0; s < 16; s++) {
        int x = 4 + s * 4;
        text_put_char(x,     y, '[');
        text_put_char(x + 1, y, (s == 5) ? '*' : ' ');
        text_put_char(x + 2, y, ']');
        char nb[3];
        nb[0] = (char)('0' + (s + 1) / 10);
        nb[1] = (char)('0' + (s + 1) % 10);
        nb[2] = 0;
        text_put_str(x, y + 2, nb);
    }
    y += 5;
    text_put_str(2, y, "STEP 5");
    text_put_str(14, y, "NOTE 60");
    text_put_str(28, y, "VEL 100");
    text_put_str(42, y, "GATE 80");
    text_put_str(58, y, "TIE: OFF");
    y += 2;
    text_put_str(2, y, "PRESS 1 = ESC   2 = EXEC   3-6 = NAV   7 = SET");
}

static void render_page_file(void)
{
    int y = 3;
    divider(y, "FILE SEARCH"); y += 2;
    text_put_str(2, y, "DEVICE: SD-CARD");
    text_put_str(28, y, "FREE: 184 KB");
    text_put_str(50, y, "FILES: 8");
    text_put_str(66, y, "VIEW: ALL");
    y += 2;
    text_put_str(2, y, "NAME                       KIND       SIZE");
    y += 1;
    for (int x = 0; x < TEXT_W; x++) text_put_char(x, y, '-');
    y += 1;
    static const char *files[8][3] = {
        {"WT_BRASS.WAV",        "WTBL",  "1024 B"},
        {"WT_STRINGS.WAV",      "WTBL",  "2048 B"},
        {"WT_LEAD.WAV",         "WTBL",  "1024 B"},
        {"SMP_KICK.PCM",        "SMPL",  "256 B"},
        {"SMP_SNARE.PCM",       "SMPL",  "256 B"},
        {"SEQ_TRACK1.SEQ",      "SEQ",   "512 B"},
        {"PROG_BANK.PRG",       "PROG",  "768 B"},
        {"SYS_GLOBAL.SYS",      "SYS",   "128 B"},
    };
    for (int i = 0; i < 8; i++) {
        if (i == g_cur_file) text_put_char(0, y, '>');
        text_put_str(2,  y, files[i][0]);
        text_put_str(30, y, files[i][1]);
        text_put_str(40, y, files[i][2]);
        y += 1;
    }
}

/* ================================================================
 *  Input handlers
 * ================================================================ */
static void handle_system(uint32_t pressed)
{
    if (pressed & BTN_UP)   { g_cur_component = (g_cur_component + 7) % 8; g_dirty = 1; }
    if (pressed & BTN_DOWN) { g_cur_component = (g_cur_component + 1) % 8; g_dirty = 1; }
}

static void handle_voice(uint32_t pressed)
{
    if (pressed & BTN_UP)   { g_cur_field = (g_cur_field + VOICE_FIELDS - 1) % VOICE_FIELDS; g_dirty = 1; }
    if (pressed & BTN_DOWN) { g_cur_field = (g_cur_field + 1) % VOICE_FIELDS; g_dirty = 1; }
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    program_t *p = &g_programs[g_cur_prog];
    switch (g_cur_field) {
    case 0: { int v = p->osc1_tbl + dir; if (v<1)v=1; if (v>32)v=32; p->osc1_tbl = (uint8_t)v; } break;
    case 1: { int v = p->osc1_pos + dir; if (v<0)v=0; if (v>63)v=63; p->osc1_pos = (uint8_t)v; } break;
    case 2: { int v = p->osc1_semi + dir; if (v<-36)v=-36; if (v>36)v=36; p->osc1_semi = (int8_t)v; } break;
    case 3: { int v = p->mix_osc1 + dir; if (v<0)v=0; if (v>127)v=127; p->mix_osc1 = (uint8_t)v; } break;
    case 4: { int v = p->mix_osc2 + dir; if (v<0)v=0; if (v>127)v=127; p->mix_osc2 = (uint8_t)v; } break;
    case 5: { int v = p->vcf_cutoff + dir; if (v<0)v=0; if (v>127)v=127; p->vcf_cutoff = (uint8_t)v; } break;
    case 6: { int v = p->vcf_resonance + dir; if (v<0)v=0; if (v>127)v=127; p->vcf_resonance = (uint8_t)v; } break;
    }
    g_dirty = 1;
}

static void handle_wavetable(uint32_t pressed)
{
    if (pressed & BTN_LEFT)  { if (g_cur_wave_idx > 0)  { g_cur_wave_idx--; g_dirty = 1; } }
    if (pressed & BTN_RIGHT) { if (g_cur_wave_idx < 63) { g_cur_wave_idx++; g_dirty = 1; } }
    if (pressed & BTN_UP)    { if (g_cur_wavetable > 1)  { g_cur_wavetable--; g_dirty = 1; } }
    if (pressed & BTN_DOWN)  { if (g_cur_wavetable < 32) { g_cur_wavetable++; g_dirty = 1; } }
}

static void handle_file(uint32_t pressed)
{
    if (pressed & BTN_UP)   { if (g_cur_file > 0) { g_cur_file--; g_dirty = 1; } }
    if (pressed & BTN_DOWN) { if (g_cur_file < 7) { g_cur_file++; g_dirty = 1; } }
}

/* ================================================================
 *  Build plane nametables for the active page
 * ================================================================ */
static void build_plane_b(void)
{
    /* Plane B is solid CRT background — tile 1 everywhere. */
    for (int i = 0; i < MAP_W * MAP_H; i++)
        g_plane_b[i] = (uint16_t)GEN_ENTRY(TILE_BG, 0, 0, 0);
}

static void build_plane_a(int active_page)
{
    /* Plane A workflow:
     *   1. Clear nametable to TILE_BLANK and clear the 80-char text grid
     *   2. Page renderers write text into g_text[] (via put_* shims) and
     *      directly stamp scope tile entries into g_plane_a where needed
     *   3. build_nametable_from_text converts g_text → packed pair tiles
     *      → nametable entries (skipping scope cells already placed) */
    map_clear(g_plane_a, (uint16_t)GEN_ENTRY(TILE_BLANK, 0, 0, 0));
    text_clear();
    draw_chrome(active_page);
    switch (active_page) {
    case PAGE_SYSTEM:    render_page_system(); break;
    case PAGE_VOICE:     render_page_voice(); break;
    case PAGE_WAVETABLE: render_page_wavetable(); break;
    case PAGE_SAMPLE:    render_page_sample(); break;
    case PAGE_SEQUENCE:  render_page_sequence(); break;
    case PAGE_FILE:      render_page_file(); break;
    }
    build_nametable_from_text();
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void)
{
    uart_puts("\n\n=== WaveTerm VDP (Genesis-style prototype) ===\n");
    timer_init();
    mmu_init();
    pmu_init();
    video_init();
    input_init(INPUT_N64);

    /* Clear both framebuffers + sprite layer; renderer never touches
     * pillarbox area when authentic=1. */
    memset32_neon(FB0_ADDR,  0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(FB1_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    init_palette();
    init_tiles();
    init_model();
    build_plane_b();

    /* Genesis plane descriptors */
    genesis_plane_t plane_a = {
        .tiles = g_tiles, .map = g_plane_a, .cram = g_cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = 0,
        .map_w = MAP_W, .map_h = MAP_H, .enabled = 1,
    };
    genesis_plane_t plane_b = {
        .tiles = g_tiles, .map = g_plane_b, .cram = g_cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = 0,
        .map_w = MAP_W, .map_h = MAP_H, .enabled = 1,
    };

    int active_page = PAGE_SYSTEM;
    uint32_t back_fb  = FB1_ADDR;
    uint32_t front_fb = FB0_ADDR;

    while (1) {
        uint32_t t0 = timer_read();
        (void)input_poll();
        uint32_t pressed = input_pressed();

        if (pressed & BTN_L) { active_page = (active_page + PAGE_COUNT - 1) % PAGE_COUNT; g_dirty = 1; }
        if (pressed & BTN_R) { active_page = (active_page + 1) % PAGE_COUNT; g_dirty = 1; }

        switch (active_page) {
        case PAGE_SYSTEM:    handle_system(pressed); break;
        case PAGE_VOICE:     handle_voice(pressed); break;
        case PAGE_WAVETABLE: handle_wavetable(pressed); break;
        case PAGE_FILE:      handle_file(pressed); break;
        default: break;
        }

        if (g_dirty) {
            build_plane_a(active_page);
            g_dirty = 0;
        }

        genesis_render((uint32_t *)back_fb, (uint32_t *)OVL_ADDR,
                       LCD_W, LCD_H,
                       /*backdrop=*/g_cram[COL_BG],
                       &plane_a, &plane_b, /*window=*/0,
                       /*sprites=*/0, 0, /*authentic=*/1);

        dcache_clean_fb(back_fb);
        dcache_clean_fb(OVL_ADDR);
        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;
    }
    return 0;
}
