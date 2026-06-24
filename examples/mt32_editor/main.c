/*
 * Jupiter SDK — Roland MT-32 Sound Editor
 *
 * Second of three planned hardware-instrument editors for the Lichee
 * Pi Zero (FB-01 → MT-32 → Behringer WaveTerm). Same UI / control
 * conventions as the FB-01 editor — tabbed fullscreen, N64 controller,
 * D-pad navigation, Z = send dump, C-up = request dump.
 *
 * Tabs: CFG / PTS (parts) / PCH (patches) / TIM (timbres) / PTL
 * (partial editor — 4 sub-views Pitch/Filter/Amp/LFO) / RHY (rhythm).
 *
 * Phase 1 (this file): UI + data model + Roland SysEx wire-format
 * builder. MIDI byte stream stubbed to UART0 hex log.
 *
 * Build: make GAME=examples/mt32_editor/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>

/* ================================================================
 *  5x7 font (same as fb01_editor)
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

static void draw_hline(uint32_t *fb, int x, int y, int w, uint32_t color)
{
    fill_rect(fb, x, y, w, 1, color);
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
 *  Colors — slightly different palette from FB-01 so the two
 *  editors are visually distinguishable (MT-32 = teal/coral, FB-01 =
 *  amber/cyan).
 * ================================================================ */
#define COL_BG          0xFF101A20
#define COL_BG_ALT      0xFF182830
#define COL_TEXT        0xFFD0D8D8
#define COL_TEXT_DIM    0xFF78807F
#define COL_HEADING     0xFF40D0C0
#define COL_CURSOR      0xFFFFFFFF
#define COL_CURSOR_BG   0xFF305058
#define COL_TAB_OFF     0xFF283840
#define COL_TAB_ON      0xFF40D0C0
#define COL_TAB_TEXT    0xFF101A20
#define COL_VALUE       0xFFFF8888
#define COL_BAR_BG      0xFF202830
#define COL_BAR_FILL    0xFF40D0C0

/* ================================================================
 *  Tabs
 * ================================================================ */
enum {
    TAB_CFG = 0,
    TAB_PTS,    /* Parts (8 melodic) */
    TAB_PCH,    /* Patches (64) */
    TAB_TIM,    /* Timbres (4 groups: A=ROM, B=ROM, M=RAM, R=Rhythm) */
    TAB_PTL,    /* Partial editor (Pitch/Filter/Amp/LFO sub-views) */
    TAB_RHY,    /* Rhythm part (drum kit) */
    TAB_COUNT,
};

static const char *tab_labels[TAB_COUNT] = {
    "CFG", "PTS", "PCH", "TIM", "PTL", "RHY",
};

/* ================================================================
 *  Data model
 * ================================================================ */

/* System / master */
typedef struct {
    uint8_t unit_id;            /* 0x10..0x1F */
    uint8_t master_volume;      /* 0..100 */
    uint8_t master_tune_hz;     /* relative to 440 (just for display) */
    int8_t  master_pan;         /* -64..+63 */
    uint8_t reverb_mode;        /* 0=Room,1=Hall,2=Plate,3=TapDelay */
    uint8_t reverb_time;        /* 0..7 */
    uint8_t reverb_level;       /* 0..7 */
    uint8_t partial_reserve[9]; /* per part (8 melodic + 1 rhythm) */
    uint8_t midi_in, midi_out;
} mt32_system_t;

/* One of 9 parts */
typedef struct {
    uint8_t midi_channel;       /* 1..16, 0 = off */
    uint8_t patch_idx;          /* 1..128 */
    uint8_t volume;             /* 0..100 */
    int8_t  key_shift;          /* -24..+24 */
    int8_t  fine_tune;          /* -50..+50 */
    uint8_t bender_range;       /* 0..24 */
    uint8_t assign_mode;        /* 0..3 = poly1/2/3/4 */
    uint8_t reverb_switch;      /* 0/1 */
} mt32_part_t;

/* Patch — what timbre + how to play it */
typedef struct {
    uint8_t timbre_group;       /* 0=A,1=B,2=M,3=R */
    uint8_t timbre_num;         /* 0..63 */
    int8_t  key_shift;
    int8_t  fine_tune;
    uint8_t bender_range;
    uint8_t assign_mode;
    uint8_t reverb_switch;
    uint8_t output_level;       /* 0..100 */
} mt32_patch_t;

/* Partial — one of 4 in a timbre. Heavy. */
typedef struct {
    uint8_t enabled;
    uint8_t pcm_select;         /* 0..255 (synth + PCM) */
    /* Pitch envelope (4 points + sustain) */
    uint8_t pe_T1, pe_T2, pe_T3, pe_T4;
    int8_t  pe_L1, pe_L2, pe_L3, pe_L4;
    /* TVF (filter) */
    uint8_t tvf_cutoff;         /* 0..100 */
    uint8_t tvf_resonance;      /* 0..30 */
    int8_t  tvf_keyfollow;      /* -100..+125 */
    int8_t  tvf_bias;
    uint8_t tvf_T1, tvf_T2, tvf_T3, tvf_T4;
    uint8_t tvf_L1, tvf_L2, tvf_L3;
    uint8_t tvf_sustain;
    uint8_t tvf_release;
    /* TVA (amp) */
    uint8_t tva_level;          /* 0..100 */
    uint8_t tva_velocity;       /* 0..127 */
    uint8_t tva_T1, tva_T2, tva_T3, tva_T4;
    uint8_t tva_L1, tva_L2, tva_L3;
    uint8_t tva_sustain;
    uint8_t tva_release;
    /* LFO */
    uint8_t lfo_rate;
    uint8_t lfo_depth;
    uint8_t lfo_pmd;
    uint8_t lfo_amd;
} mt32_partial_t;

/* Timbre — 4 partials + structure */
typedef struct {
    char    name[11];           /* 10 chars + null */
    uint8_t structure;          /* 0..11 */
    uint8_t partial_mute;       /* bit mask, low 4 bits */
    uint8_t sustain;            /* 0/1 */
    mt32_partial_t partial[4];
} mt32_timbre_t;

/* Rhythm key assignment */
typedef struct {
    uint8_t timbre_num;         /* 0..63 within R group */
    uint8_t output_level;
    int8_t  panpot;             /* -64..+63 */
    uint8_t reverb_switch;
} mt32_rhythm_key_t;

/* ----- Globals ----- */
static mt32_system_t      g_system;
static mt32_part_t        g_parts[9];          /* 0..7 melodic, 8 rhythm */
static mt32_patch_t       g_patches[64];
static mt32_timbre_t      g_timbres_m[64];     /* RAM group M */
static mt32_rhythm_key_t  g_rhythm[85];        /* MIDI notes 35..119 */

/* Cursors */
static int g_cur_cfg          = 0;
static int g_cur_pts_inst     = 0;
static int g_cur_pts_field    = 0;
static int g_pts_mode         = 0;   /* 0 list, 1 sub-edit */
static int g_cur_pch_idx      = 0;
static int g_cur_pch_field    = 0;
static int g_pch_mode         = 0;
static int g_cur_tim_idx      = 0;
static int g_cur_tim_group    = 2;   /* default to M (RAM) */
static int g_cur_ptl_partial  = 0;   /* 0..3 which partial of current timbre */
static int g_cur_ptl_subview  = 0;   /* 0=pitch, 1=filter, 2=amp, 3=lfo */
static int g_cur_ptl_field    = 0;
static int g_cur_rhy_key      = 0;   /* 0..84 → notes 35..119 */
static int g_rhy_scroll       = 0;
static int g_cur_rhy_field    = 0;
static int g_rhy_mode         = 0;

/* The timbre currently loaded into PTL editor */
static int g_current_timbre = 0;     /* index into g_timbres_m */

/* ================================================================
 *  Roland SysEx wire-format builder (MT-32)
 *
 *   F0 41 10 16 <cmd> <addr_hi> <addr_mid> <addr_low> [data] <cs> F7
 *
 *   cmd = 0x12 (DT1 send) or 0x11 (RQ1 request)
 *   cs  = (-(sum_of_addr + sum_of_data)) & 0x7F
 * ================================================================ */
static void uart_puthex_byte(uint8_t b)
{
    static const char hex[] = "0123456789ABCDEF";
    char s[3];
    s[0] = hex[b >> 4]; s[1] = hex[b & 0xF]; s[2] = 0;
    uart_puts(s);
}

static void midi_send_bytes(const uint8_t *buf, uint32_t len, const char *label)
{
    uart_puts("[midi tx ");
    uart_puts(label);
    uart_puts("] ");
    uart_putdec(len);
    uart_puts(" bytes:");
    for (uint32_t i = 0; i < len; i++) {
        uart_puts(" ");
        uart_puthex_byte(buf[i]);
        if (((i + 1) % 16) == 0 && (i + 1) < len) uart_puts("\n             ");
    }
    uart_puts("\n");
}

static uint8_t roland_checksum(const uint8_t *bytes, uint32_t n)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; i++) sum += bytes[i];
    return (uint8_t)((0x100 - (sum & 0x7F)) & 0x7F);
}

/* Build & send a DT1 (data-set) message. addr is 3 bytes, data is n bytes. */
static void sysex_dt1(uint32_t addr, const uint8_t *data, uint32_t n,
                      const char *label)
{
    uint8_t pkt[256];
    uint8_t *p = pkt;
    *p++ = 0xF0; *p++ = 0x41; *p++ = g_system.unit_id; *p++ = 0x16;
    *p++ = 0x12;                  /* DT1 */
    uint8_t addr_bytes[3] = {
        (uint8_t)((addr >> 16) & 0x7F),
        (uint8_t)((addr >>  8) & 0x7F),
        (uint8_t)( addr        & 0x7F),
    };
    *p++ = addr_bytes[0]; *p++ = addr_bytes[1]; *p++ = addr_bytes[2];
    for (uint32_t i = 0; i < n; i++) *p++ = data[i] & 0x7F;
    /* Checksum covers address + data */
    uint8_t cs_input[256];
    cs_input[0] = addr_bytes[0]; cs_input[1] = addr_bytes[1]; cs_input[2] = addr_bytes[2];
    for (uint32_t i = 0; i < n; i++) cs_input[3 + i] = data[i] & 0x7F;
    *p++ = roland_checksum(cs_input, 3 + n);
    *p++ = 0xF7;
    midi_send_bytes(pkt, (uint32_t)(p - pkt), label);
}

/* Build & send an RQ1 (data-request) message. */
static void sysex_rq1(uint32_t addr, uint32_t size, const char *label)
{
    uint8_t pkt[16];
    uint8_t *p = pkt;
    *p++ = 0xF0; *p++ = 0x41; *p++ = g_system.unit_id; *p++ = 0x16;
    *p++ = 0x11;                  /* RQ1 */
    uint8_t bytes[6] = {
        (uint8_t)((addr >> 16) & 0x7F),
        (uint8_t)((addr >>  8) & 0x7F),
        (uint8_t)( addr        & 0x7F),
        (uint8_t)((size >> 16) & 0x7F),
        (uint8_t)((size >>  8) & 0x7F),
        (uint8_t)( size        & 0x7F),
    };
    for (int i = 0; i < 6; i++) *p++ = bytes[i];
    *p++ = roland_checksum(bytes, 6);
    *p++ = 0xF7;
    midi_send_bytes(pkt, (uint32_t)(p - pkt), label);
}

/* Convenience: write one byte to system area (offset within 10 00 00). */
static void sysex_system_byte(uint8_t offset, uint8_t value)
{
    uint8_t v = value;
    sysex_dt1(0x100000u + offset, &v, 1, "sys param");
}

/* Push the entire current part record to MT-32's patch-temp area. */
static void sysex_send_part(int p)
{
    if (p < 0 || p > 8) return;
    /* Patch temp = 03 00 00 + (p * 0x10). 7 bytes per part record. */
    uint8_t data[7];
    mt32_part_t *pr = &g_parts[p];
    data[0] = pr->patch_idx ? (pr->patch_idx - 1) : 0;
    data[1] = (uint8_t)((int)pr->key_shift + 24);
    data[2] = (uint8_t)((int)pr->fine_tune + 50);
    data[3] = pr->bender_range;
    data[4] = pr->assign_mode;
    data[5] = pr->reverb_switch;
    data[6] = pr->volume;
    sysex_dt1(0x030000u + (uint32_t)p * 0x10u, data, sizeof(data), "part->temp");
}

/* Push current timbre to timbre-temp area (one of 8 parts' slot). */
static void sysex_send_timbre(int part)
{
    if (part < 0 || part > 7) return;
    /* Timbre temp = 04 00 00 + part * 0x100. Pack our model in
     * partial-major order. Real MT-32 timbre dump is 246 bytes — we
     * serialize the canonical fields and pad. */
    uint8_t buf[256] = {0};
    mt32_timbre_t *t = &g_timbres_m[g_current_timbre];
    /* Common header (10 bytes name + structure + mute + sustain) */
    for (int i = 0; i < 10; i++) buf[i] = (uint8_t)(t->name[i] ? t->name[i] : ' ');
    buf[10] = t->structure;
    buf[11] = t->partial_mute & 0x0F;
    buf[12] = t->sustain;
    /* 4 partials, ~58 bytes each (we'll just pack the modelled ones) */
    uint8_t *p = buf + 14;
    for (int i = 0; i < 4; i++) {
        mt32_partial_t *pp = &t->partial[i];
        *p++ = pp->pcm_select;
        *p++ = pp->pe_T1; *p++ = pp->pe_T2; *p++ = pp->pe_T3; *p++ = pp->pe_T4;
        *p++ = (uint8_t)((int)pp->pe_L1 + 50);
        *p++ = (uint8_t)((int)pp->pe_L2 + 50);
        *p++ = (uint8_t)((int)pp->pe_L3 + 50);
        *p++ = (uint8_t)((int)pp->pe_L4 + 50);
        *p++ = pp->tvf_cutoff; *p++ = pp->tvf_resonance;
        *p++ = (uint8_t)((int)pp->tvf_keyfollow + 100);
        *p++ = (uint8_t)((int)pp->tvf_bias + 64);
        *p++ = pp->tvf_T1; *p++ = pp->tvf_T2; *p++ = pp->tvf_T3; *p++ = pp->tvf_T4;
        *p++ = pp->tvf_L1; *p++ = pp->tvf_L2; *p++ = pp->tvf_L3;
        *p++ = pp->tvf_sustain; *p++ = pp->tvf_release;
        *p++ = pp->tva_level; *p++ = pp->tva_velocity;
        *p++ = pp->tva_T1; *p++ = pp->tva_T2; *p++ = pp->tva_T3; *p++ = pp->tva_T4;
        *p++ = pp->tva_L1; *p++ = pp->tva_L2; *p++ = pp->tva_L3;
        *p++ = pp->tva_sustain; *p++ = pp->tva_release;
        *p++ = pp->lfo_rate; *p++ = pp->lfo_depth;
        *p++ = pp->lfo_pmd;  *p++ = pp->lfo_amd;
        *p++ = pp->enabled;
        /* pad remaining bytes in this partial's 58-byte slot */
        while (((p - buf - 14) % 58) != 0) *p++ = 0;
    }
    sysex_dt1(0x040000u + (uint32_t)part * 0x100u, buf, 246, "timbre->temp");
}

/* Push one patch to patch memory. 8 bytes per patch. */
static void sysex_send_patch(int idx)
{
    if (idx < 0 || idx >= 64) return;
    mt32_patch_t *q = &g_patches[idx];
    uint8_t data[8];
    data[0] = (uint8_t)(q->timbre_group * 64 + q->timbre_num);
    data[1] = (uint8_t)((int)q->key_shift + 24);
    data[2] = (uint8_t)((int)q->fine_tune + 50);
    data[3] = q->bender_range;
    data[4] = q->assign_mode;
    data[5] = q->reverb_switch;
    data[6] = q->output_level;
    data[7] = 0;
    sysex_dt1(0x050000u + (uint32_t)idx * 8u, data, sizeof(data), "patch");
}

/* Push system-area whole-block. */
static void sysex_send_system(void)
{
    uint8_t buf[23];
    memset(buf, 0, sizeof(buf));
    buf[0]  = (uint8_t)((int)g_system.master_tune_hz);   /* 0..127 */
    buf[1]  = g_system.reverb_mode;
    buf[2]  = g_system.reverb_time;
    buf[3]  = g_system.reverb_level;
    for (int i = 0; i < 9; i++) buf[4 + i] = g_system.partial_reserve[i];
    buf[13] = g_system.midi_in;
    buf[14] = g_system.midi_out;
    buf[15] = g_system.master_volume;
    sysex_dt1(0x100000u, buf, sizeof(buf), "system");
}

/* Request bulk dumps */
static void sysex_request_patches(void)  { sysex_rq1(0x050000u, 64u * 8u, "req patches"); }
static void sysex_request_timbres(void)  { sysex_rq1(0x080000u, 32u * 256u, "req timbres A"); }
static void sysex_request_system(void)   { sysex_rq1(0x100000u, 23u, "req system"); }

/* ================================================================
 *  Common chrome
 * ================================================================ */
#define TAB_BAR_H     24
#define STATUS_BAR_H  12

static void render_tab_bar(uint32_t *fb, int active_tab, const char *title_right)
{
    fill_rect(fb, 0, 0, LCD_W, TAB_BAR_H, COL_BG_ALT);
    int x = 4;
    for (int i = 0; i < TAB_COUNT; i++) {
        uint32_t c_bg   = (i == active_tab) ? COL_TAB_ON : COL_TAB_OFF;
        uint32_t c_text = (i == active_tab) ? COL_TAB_TEXT : COL_TEXT;
        int w = (FNT_W + 1) * 3 * 2 + 8;
        fill_rect(fb, x, 3, w, 18, c_bg);
        draw_text(fb, tab_labels[i], x + 4, 6, 2, c_text);
        x += w + 2;
    }
    if (title_right && *title_right) {
        int w = text_width(title_right, 1);
        draw_text(fb, title_right, LCD_W - w - 6, 9, 1, COL_TEXT_DIM);
    }
}

static void render_status_bar(uint32_t *fb, const char *hint)
{
    int y = LCD_H - STATUS_BAR_H;
    fill_rect(fb, 0, y, LCD_W, STATUS_BAR_H, COL_BG_ALT);
    draw_text(fb, hint, 4, y + 3, 1, COL_TEXT_DIM);
}

/* Compact column-bound field renderer. */
static void render_field_c(uint32_t *fb, int x, int y, int w, int row_h,
                           const char *label, const char *value, int is_cursor)
{
    if (is_cursor) fill_rect(fb, x - 2, y - 1, w, row_h, COL_CURSOR_BG);
    draw_text(fb, label, x + 2, y, 1, is_cursor ? COL_CURSOR : COL_TEXT);
    if (value)
        draw_text(fb, value, x + w - 2 - text_width(value, 1), y, 1,
                  is_cursor ? COL_CURSOR : COL_VALUE);
}

/* Full-width field renderer (size 2). */
static void render_field(uint32_t *fb, int y, int row_h, const char *label,
                         const char *value, int is_cursor)
{
    if (is_cursor) {
        fill_rect(fb, 4, y - 2, LCD_W - 8, row_h, COL_CURSOR_BG);
        draw_text(fb, ">", 8, y, 2, COL_CURSOR);
    }
    draw_text(fb, label, 24, y, 2, is_cursor ? COL_CURSOR : COL_TEXT);
    if (value)
        draw_text(fb, value, LCD_W - 4 - text_width(value, 2), y, 2,
                  is_cursor ? COL_CURSOR : COL_VALUE);
}

/* ================================================================
 *  Model init
 * ================================================================ */
static const char *placeholder_timbre_names[] = {
    "AcouPno1","AcouPno2","ElecPno1","ElecPno2",
    "Organ1","Organ2","Strings1","Brass1",
    "SynLead1","SynLead2","SynBass1","SynBass2",
    "Flute","Clarinet","Trumpet","Sax",
};

static void init_model(void)
{
    /* System defaults */
    g_system.unit_id        = 0x10;
    g_system.master_volume  = 80;
    g_system.master_tune_hz = 442 - 415;   /* arbitrary mapping */
    g_system.master_pan     = 0;
    g_system.reverb_mode    = 1;            /* Hall */
    g_system.reverb_time    = 5;
    g_system.reverb_level   = 4;
    for (int i = 0; i < 9; i++) g_system.partial_reserve[i] = (i < 8) ? 4 : 0;
    g_system.midi_in        = 0;
    g_system.midi_out       = 0;

    /* Parts: 8 melodic on chans 2..9, 1 rhythm on ch 10 */
    for (int i = 0; i < 8; i++) {
        g_parts[i].midi_channel  = (uint8_t)(i + 2);
        g_parts[i].patch_idx     = (uint8_t)(i + 1);
        g_parts[i].volume        = 80;
        g_parts[i].key_shift     = 0;
        g_parts[i].fine_tune     = 0;
        g_parts[i].bender_range  = 2;
        g_parts[i].assign_mode   = 0;
        g_parts[i].reverb_switch = 1;
    }
    g_parts[8].midi_channel = 10; g_parts[8].volume = 90;

    /* Patches: simple defaults */
    for (int i = 0; i < 64; i++) {
        g_patches[i].timbre_group  = 0;          /* A */
        g_patches[i].timbre_num    = (uint8_t)(i & 0x3F);
        g_patches[i].key_shift     = 0;
        g_patches[i].fine_tune     = 0;
        g_patches[i].bender_range  = 2;
        g_patches[i].assign_mode   = 0;
        g_patches[i].reverb_switch = 1;
        g_patches[i].output_level  = 100;
    }

    /* RAM timbres */
    for (int i = 0; i < 64; i++) {
        const char *nm = placeholder_timbre_names[i & 15];
        int k = 0; while (nm[k] && k < 10) { g_timbres_m[i].name[k] = nm[k]; k++; }
        while (k < 10) g_timbres_m[i].name[k++] = ' ';
        g_timbres_m[i].name[10] = 0;
        g_timbres_m[i].structure    = 0;
        g_timbres_m[i].partial_mute = 0x0F;     /* all 4 active */
        g_timbres_m[i].sustain      = 1;
        for (int p = 0; p < 4; p++) {
            mt32_partial_t *pp = &g_timbres_m[i].partial[p];
            pp->enabled       = 1;
            pp->pcm_select    = 0;
            pp->pe_T1 = 0;  pp->pe_T2 = 0;  pp->pe_T3 = 0;  pp->pe_T4 = 50;
            pp->pe_L1 = 0;  pp->pe_L2 = 0;  pp->pe_L3 = 0;  pp->pe_L4 = 0;
            pp->tvf_cutoff = 100; pp->tvf_resonance = 0;
            pp->tvf_keyfollow = 0; pp->tvf_bias = 0;
            pp->tvf_T1 = 0; pp->tvf_T2 = 0; pp->tvf_T3 = 0; pp->tvf_T4 = 0;
            pp->tvf_L1 = 0; pp->tvf_L2 = 0; pp->tvf_L3 = 0;
            pp->tvf_sustain = 100; pp->tvf_release = 50;
            pp->tva_level = 100; pp->tva_velocity = 64;
            pp->tva_T1 = 5;  pp->tva_T2 = 30; pp->tva_T3 = 40; pp->tva_T4 = 30;
            pp->tva_L1 = 100; pp->tva_L2 = 80; pp->tva_L3 = 60;
            pp->tva_sustain = 80; pp->tva_release = 30;
            pp->lfo_rate  = 50; pp->lfo_depth = 0;
            pp->lfo_pmd   = 0;  pp->lfo_amd   = 0;
        }
    }

    /* Rhythm: GM-ish defaults for keys 35..119 */
    for (int i = 0; i < 85; i++) {
        g_rhythm[i].timbre_num    = (uint8_t)(i & 0x3F);
        g_rhythm[i].output_level  = 80;
        g_rhythm[i].panpot        = 0;
        g_rhythm[i].reverb_switch = 1;
    }
}

/* ================================================================
 *  CFG screen
 * ================================================================ */
enum {
    CFG_VOL = 0, CFG_TUNE, CFG_PAN,
    CFG_REV_MODE, CFG_REV_TIME, CFG_REV_LVL,
    CFG_UNIT,
    CFG_COUNT,
};
static const char *reverb_mode_names[] = {"Room","Hall","Plate","TapDly"};

static void render_screen_cfg(uint32_t *fb)
{
    int y = TAB_BAR_H + 4;
    int row_h = 18;
    char buf[16];
    draw_text(fb, "MASTER", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 90, COL_TEXT_DIM);
    y += row_h;

    /* Master volume bar */
    int is = (g_cur_cfg == CFG_VOL);
    if (is) { fill_rect(fb, 4, y - 2, LCD_W - 8, row_h, COL_CURSOR_BG); draw_text(fb, ">", 8, y, 2, COL_CURSOR); }
    draw_text(fb, "Master volume", 24, y, 2, is ? COL_CURSOR : COL_TEXT);
    int bar_x = 260, bar_w = 160, bar_h = 12;
    fill_rect(fb, bar_x, y + 2, bar_w, bar_h, COL_BAR_BG);
    fill_rect(fb, bar_x, y + 2, (bar_w * g_system.master_volume) / 100, bar_h, is ? COL_CURSOR : COL_BAR_FILL);
    int_to_str(g_system.master_volume, buf);
    draw_text(fb, buf, bar_x + bar_w + 4, y, 2, is ? COL_CURSOR : COL_VALUE);
    y += row_h;

    int_to_str(g_system.master_tune_hz + 415, buf);   /* shown as Hz */
    render_field(fb, y, row_h, "Master tune (Hz)", buf, g_cur_cfg == CFG_TUNE); y += row_h;
    int_to_str(g_system.master_pan, buf);
    render_field(fb, y, row_h, "Master pan",       buf, g_cur_cfg == CFG_PAN);  y += row_h;

    y += 2;
    draw_text(fb, "REVERB", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 90, COL_TEXT_DIM);
    y += row_h;
    render_field(fb, y, row_h, "Mode", reverb_mode_names[g_system.reverb_mode],
                 g_cur_cfg == CFG_REV_MODE); y += row_h;
    int_to_str(g_system.reverb_time, buf);
    render_field(fb, y, row_h, "Time",  buf, g_cur_cfg == CFG_REV_TIME); y += row_h;
    int_to_str(g_system.reverb_level, buf);
    render_field(fb, y, row_h, "Level", buf, g_cur_cfg == CFG_REV_LVL);  y += row_h;

    y += 2;
    draw_text(fb, "DEVICE", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 90, COL_TEXT_DIM);
    y += row_h;
    char ub[8]; int_to_str(g_system.unit_id, ub);
    render_field(fb, y, row_h, "Unit ID (hex)", ub, g_cur_cfg == CFG_UNIT);
}

static void cfg_handle_input(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_cfg = (g_cur_cfg + CFG_COUNT - 1) % CFG_COUNT;
    if (pressed & BTN_DOWN) g_cur_cfg = (g_cur_cfg + 1) % CFG_COUNT;
    if (pressed & BTN_Z)    sysex_send_system();
    if (pressed & BTN_C_UP) sysex_request_system();
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    switch (g_cur_cfg) {
    case CFG_VOL:      { int t = g_system.master_volume + dir; if (t<0)t=0; if (t>100)t=100; g_system.master_volume = (uint8_t)t; } break;
    case CFG_TUNE:     { int t = g_system.master_tune_hz + dir; if (t<0)t=0; if (t>50)t=50; g_system.master_tune_hz = (uint8_t)t; } break;
    case CFG_PAN:      { int t = g_system.master_pan + dir; if (t<-64)t=-64; if (t>63)t=63; g_system.master_pan = (int8_t)t; } break;
    case CFG_REV_MODE: { int t = g_system.reverb_mode + dir; if (t<0)t=0; if (t>3)t=3; g_system.reverb_mode = (uint8_t)t; } break;
    case CFG_REV_TIME: { int t = g_system.reverb_time + dir; if (t<0)t=0; if (t>7)t=7; g_system.reverb_time = (uint8_t)t; } break;
    case CFG_REV_LVL:  { int t = g_system.reverb_level + dir; if (t<0)t=0; if (t>7)t=7; g_system.reverb_level = (uint8_t)t; } break;
    case CFG_UNIT:     { int t = g_system.unit_id + dir; if (t<0x10)t=0x10; if (t>0x1F)t=0x1F; g_system.unit_id = (uint8_t)t; } break;
    }
}

/* ================================================================
 *  PARTS screen (8 melodic + 1 rhythm = 9 parts)
 * ================================================================ */
static const char *assign_names[] = {"Poly1","Poly2","Poly3","Poly4"};
enum {
    PF_CHANNEL = 0, PF_PATCH, PF_VOLUME, PF_KSHIFT, PF_FTUNE,
    PF_BENDER, PF_ASSIGN, PF_REVERB,
    PF_COUNT,
};

static void render_screen_pts_list(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    draw_text(fb, "PARTS (8 melodic + 1 rhythm)", 6, y, 2, COL_HEADING);
    draw_text(fb, "A = edit  Z = send  Cu = req",
              LCD_W - text_width("A = edit  Z = send  Cu = req", 1) - 6,
              y + 6, 1, COL_TEXT_DIM);
    draw_text(fb, "#  Ch  Patch  Vol  KSft  FTu  Bend  Asgn  Rev",
              6, y + 20, 1, COL_TEXT_DIM);
    draw_hline(fb, 6, y + 30, LCD_W - 12, COL_TEXT_DIM);
    y += 34;

    int row_h = 21;
    for (int i = 0; i < 9; i++) {
        int ry = y + i * row_h;
        if (ry + row_h > LCD_H - STATUS_BAR_H) break;
        int is = (i == g_cur_pts_inst);
        if (is) fill_rect(fb, 4, ry - 2, LCD_W - 8, row_h, COL_CURSOR_BG);
        uint32_t c  = is ? COL_CURSOR : COL_TEXT;
        uint32_t cv = is ? COL_CURSOR : COL_VALUE;
        int_to_str(i + 1, buf);
        draw_text(fb, i == 8 ? "R" : buf, 8, ry + 4, 2, c);
        if (g_parts[i].midi_channel == 0) {
            draw_text(fb, "--", 36, ry + 4, 2, COL_TEXT_DIM);
            continue;
        }
        int_to_str(g_parts[i].midi_channel, buf);
        draw_text(fb, buf, 36, ry + 4, 2, cv);
        int_to_str(g_parts[i].patch_idx, buf);
        draw_text(fb, buf, 90, ry + 4, 2, cv);
        int_to_str(g_parts[i].volume, buf);
        draw_text(fb, buf, 160, ry + 4, 2, cv);
        int_to_str(g_parts[i].key_shift, buf);
        draw_text(fb, buf, 220, ry + 4, 2, cv);
        int_to_str(g_parts[i].fine_tune, buf);
        draw_text(fb, buf, 280, ry + 4, 2, cv);
        int_to_str(g_parts[i].bender_range, buf);
        draw_text(fb, buf, 330, ry + 4, 2, cv);
        draw_text(fb, assign_names[g_parts[i].assign_mode], 380, ry + 4, 1, cv);
        draw_text(fb, g_parts[i].reverb_switch ? "[X]" : "[ ]", 440, ry + 4, 1, cv);
    }
}

static void render_screen_pts_edit(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    mt32_part_t *pr = &g_parts[g_cur_pts_inst];
    draw_text(fb, "PART ", 6, y, 2, COL_HEADING);
    int_to_str(g_cur_pts_inst + 1, buf);
    draw_text(fb, g_cur_pts_inst == 8 ? "R (rhythm)" : buf,
              6 + text_width("PART ", 2), y, 2, COL_VALUE);
    draw_text(fb, "B = back", LCD_W - text_width("B = back", 1) - 6, y + 6, 1, COL_TEXT_DIM);
    y += 18;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 4;

    int row_h = 16;
    int_to_str(pr->midi_channel, buf);
    render_field(fb, y, row_h, "MIDI channel", buf, g_cur_pts_field == PF_CHANNEL); y += row_h;
    int_to_str(pr->patch_idx, buf);
    render_field(fb, y, row_h, "Patch (1-128)", buf, g_cur_pts_field == PF_PATCH);  y += row_h;
    int_to_str(pr->volume, buf);
    render_field(fb, y, row_h, "Volume",        buf, g_cur_pts_field == PF_VOLUME); y += row_h;
    int_to_str(pr->key_shift, buf);
    render_field(fb, y, row_h, "Key shift",     buf, g_cur_pts_field == PF_KSHIFT); y += row_h;
    int_to_str(pr->fine_tune, buf);
    render_field(fb, y, row_h, "Fine tune",     buf, g_cur_pts_field == PF_FTUNE);  y += row_h;
    int_to_str(pr->bender_range, buf);
    render_field(fb, y, row_h, "Bender range",  buf, g_cur_pts_field == PF_BENDER); y += row_h;
    render_field(fb, y, row_h, "Assign mode", assign_names[pr->assign_mode],
                 g_cur_pts_field == PF_ASSIGN); y += row_h;
    render_field(fb, y, row_h, "Reverb",
                 pr->reverb_switch ? "[X]" : "[ ]", g_cur_pts_field == PF_REVERB);
}

static void render_screen_pts(uint32_t *fb)
{
    if (g_pts_mode == 0) render_screen_pts_list(fb);
    else                 render_screen_pts_edit(fb);
}

static void pts_handle_input(uint32_t pressed)
{
    if (g_pts_mode == 0) {
        if (pressed & BTN_UP)   g_cur_pts_inst = (g_cur_pts_inst + 9 - 1) % 9;
        if (pressed & BTN_DOWN) g_cur_pts_inst = (g_cur_pts_inst + 1) % 9;
        if (pressed & BTN_A) { g_pts_mode = 1; g_cur_pts_field = 0; }
        if (pressed & BTN_Z) sysex_send_part(g_cur_pts_inst);
        return;
    }
    if (pressed & BTN_B) { g_pts_mode = 0; return; }
    if (pressed & BTN_UP)   g_cur_pts_field = (g_cur_pts_field + PF_COUNT - 1) % PF_COUNT;
    if (pressed & BTN_DOWN) g_cur_pts_field = (g_cur_pts_field + 1) % PF_COUNT;
    if (pressed & BTN_Z)    sysex_send_part(g_cur_pts_inst);
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    mt32_part_t *pr = &g_parts[g_cur_pts_inst];
    switch (g_cur_pts_field) {
    case PF_CHANNEL: { int t = pr->midi_channel + dir; if (t<0)t=0; if (t>16)t=16; pr->midi_channel = (uint8_t)t; } break;
    case PF_PATCH:   { int t = pr->patch_idx + dir; if (t<1)t=1; if (t>128)t=128; pr->patch_idx = (uint8_t)t; } break;
    case PF_VOLUME:  { int t = pr->volume + dir; if (t<0)t=0; if (t>100)t=100; pr->volume = (uint8_t)t; } break;
    case PF_KSHIFT:  { int t = pr->key_shift + dir; if (t<-24)t=-24; if (t>24)t=24; pr->key_shift = (int8_t)t; } break;
    case PF_FTUNE:   { int t = pr->fine_tune + dir; if (t<-50)t=-50; if (t>50)t=50; pr->fine_tune = (int8_t)t; } break;
    case PF_BENDER:  { int t = pr->bender_range + dir; if (t<0)t=0; if (t>24)t=24; pr->bender_range = (uint8_t)t; } break;
    case PF_ASSIGN:  { int t = pr->assign_mode + dir; if (t<0)t=0; if (t>3)t=3; pr->assign_mode = (uint8_t)t; } break;
    case PF_REVERB:  pr->reverb_switch ^= 1; break;
    }
}

/* ================================================================
 *  PATCHES screen (64 patches)
 * ================================================================ */
enum {
    QF_TGROUP = 0, QF_TNUM, QF_KSHIFT, QF_FTUNE, QF_BENDER,
    QF_ASSIGN, QF_REVERB, QF_OUTLVL,
    QF_COUNT,
};
static const char *tgroup_names[] = {"A", "B", "M", "R"};

static void render_screen_pch_list(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    draw_text(fb, "PATCHES (64)", 6, y, 2, COL_HEADING);
    draw_text(fb, "A = edit  Z = send sel",
              LCD_W - text_width("A = edit  Z = send sel", 1) - 6,
              y + 6, 1, COL_TEXT_DIM);
    draw_text(fb, "#   Timbre     KSft  Out  Rev",
              6, y + 20, 1, COL_TEXT_DIM);
    draw_hline(fb, 6, y + 30, LCD_W - 12, COL_TEXT_DIM);
    y += 34;
    int col_w = (LCD_W - 16) / 2;
    int row_h = 8;
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 2; col++) {
            int pidx = col * 32 + row;
            int x = 6 + col * col_w;
            int ry = y + row * row_h;
            int is_sel = (pidx == g_cur_pch_idx);
            if (is_sel) fill_rect(fb, x - 2, ry - 1, col_w - 4, row_h, COL_CURSOR_BG);
            uint32_t c = is_sel ? COL_CURSOR : COL_TEXT;
            char idx_s[4];
            if (pidx + 1 < 10) { idx_s[0] = '0'; idx_s[1] = (char)('0' + pidx + 1); idx_s[2] = 0; }
            else               { idx_s[0] = (char)('0' + (pidx + 1)/10); idx_s[1] = (char)('0' + (pidx + 1) % 10); idx_s[2] = 0; }
            draw_text(fb, idx_s, x, ry, 1, c);
            /* Show timbre group#num + key shift */
            char tg[10];
            tg[0] = tgroup_names[g_patches[pidx].timbre_group][0];
            tg[1] = '#';
            tg[2] = (char)('0' + (g_patches[pidx].timbre_num + 1) / 10);
            tg[3] = (char)('0' + (g_patches[pidx].timbre_num + 1) % 10);
            tg[4] = 0;
            draw_text(fb, tg, x + 18, ry, 1, c);
            int_to_str(g_patches[pidx].key_shift, buf);
            draw_text(fb, buf, x + 70, ry, 1, c);
            int_to_str(g_patches[pidx].output_level, buf);
            draw_text(fb, buf, x + 110, ry, 1, c);
            draw_text(fb, g_patches[pidx].reverb_switch ? "[X]" : "[ ]", x + 150, ry, 1, c);
        }
    }
}

static void render_screen_pch_edit(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    mt32_patch_t *q = &g_patches[g_cur_pch_idx];
    draw_text(fb, "PATCH ", 6, y, 2, COL_HEADING);
    int_to_str(g_cur_pch_idx + 1, buf);
    draw_text(fb, buf, 6 + text_width("PATCH ", 2), y, 2, COL_VALUE);
    draw_text(fb, "B = back", LCD_W - text_width("B = back", 1) - 6, y + 6, 1, COL_TEXT_DIM);
    y += 18;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 4;

    int row_h = 16;
    render_field(fb, y, row_h, "Timbre group", tgroup_names[q->timbre_group],
                 g_cur_pch_field == QF_TGROUP); y += row_h;
    int_to_str(q->timbre_num + 1, buf);
    render_field(fb, y, row_h, "Timbre number", buf, g_cur_pch_field == QF_TNUM); y += row_h;
    int_to_str(q->key_shift, buf);
    render_field(fb, y, row_h, "Key shift", buf, g_cur_pch_field == QF_KSHIFT); y += row_h;
    int_to_str(q->fine_tune, buf);
    render_field(fb, y, row_h, "Fine tune", buf, g_cur_pch_field == QF_FTUNE); y += row_h;
    int_to_str(q->bender_range, buf);
    render_field(fb, y, row_h, "Bender range", buf, g_cur_pch_field == QF_BENDER); y += row_h;
    render_field(fb, y, row_h, "Assign mode", assign_names[q->assign_mode],
                 g_cur_pch_field == QF_ASSIGN); y += row_h;
    render_field(fb, y, row_h, "Reverb",
                 q->reverb_switch ? "[X]" : "[ ]", g_cur_pch_field == QF_REVERB); y += row_h;
    int_to_str(q->output_level, buf);
    render_field(fb, y, row_h, "Output level", buf, g_cur_pch_field == QF_OUTLVL);
}

static void render_screen_pch(uint32_t *fb)
{
    if (g_pch_mode == 0) render_screen_pch_list(fb);
    else                 render_screen_pch_edit(fb);
}

static void pch_handle_input(uint32_t pressed)
{
    if (g_pch_mode == 0) {
        if (pressed & BTN_UP)    g_cur_pch_idx = (g_cur_pch_idx + 64 - 1) % 64;
        if (pressed & BTN_DOWN)  g_cur_pch_idx = (g_cur_pch_idx + 1) % 64;
        if (pressed & BTN_LEFT)  { if (g_cur_pch_idx >= 32) g_cur_pch_idx -= 32; }
        if (pressed & BTN_RIGHT) { if (g_cur_pch_idx <  32) g_cur_pch_idx += 32; }
        if (pressed & BTN_A) { g_pch_mode = 1; g_cur_pch_field = 0; }
        if (pressed & BTN_Z) sysex_send_patch(g_cur_pch_idx);
        if (pressed & BTN_C_UP) sysex_request_patches();
        return;
    }
    if (pressed & BTN_B) { g_pch_mode = 0; return; }
    if (pressed & BTN_UP)   g_cur_pch_field = (g_cur_pch_field + QF_COUNT - 1) % QF_COUNT;
    if (pressed & BTN_DOWN) g_cur_pch_field = (g_cur_pch_field + 1) % QF_COUNT;
    if (pressed & BTN_Z)    sysex_send_patch(g_cur_pch_idx);
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    mt32_patch_t *q = &g_patches[g_cur_pch_idx];
    switch (g_cur_pch_field) {
    case QF_TGROUP:  { int t = q->timbre_group + dir; if (t<0)t=0; if (t>3)t=3; q->timbre_group = (uint8_t)t; } break;
    case QF_TNUM:    { int t = q->timbre_num + dir; if (t<0)t=0; if (t>63)t=63; q->timbre_num = (uint8_t)t; } break;
    case QF_KSHIFT:  { int t = q->key_shift + dir; if (t<-24)t=-24; if (t>24)t=24; q->key_shift = (int8_t)t; } break;
    case QF_FTUNE:   { int t = q->fine_tune + dir; if (t<-50)t=-50; if (t>50)t=50; q->fine_tune = (int8_t)t; } break;
    case QF_BENDER:  { int t = q->bender_range + dir; if (t<0)t=0; if (t>24)t=24; q->bender_range = (uint8_t)t; } break;
    case QF_ASSIGN:  { int t = q->assign_mode + dir; if (t<0)t=0; if (t>3)t=3; q->assign_mode = (uint8_t)t; } break;
    case QF_REVERB:  q->reverb_switch ^= 1; break;
    case QF_OUTLVL:  { int t = q->output_level + dir; if (t<0)t=0; if (t>100)t=100; q->output_level = (uint8_t)t; } break;
    }
}

/* ================================================================
 *  TIMBRES screen (4 groups: A=ROM, B=ROM, M=RAM, R=Rhythm)
 *  For Phase 1 only group M is editable; A/B render placeholder names.
 * ================================================================ */
static void render_screen_tim(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];

    /* Group selector */
    draw_text(fb, "GROUP", 6, y, 2, COL_HEADING);
    for (int i = 0; i < 4; i++) {
        const char *names[] = {"A=ROM", "B=ROM", "M=RAM", "R=Rhy"};
        int x = 80 + i * 80;
        int is = (i == g_cur_tim_group);
        if (is) fill_rect(fb, x - 4, y - 2, 76, 18, COL_CURSOR_BG);
        draw_text(fb, names[i], x, y, 2, is ? COL_CURSOR : COL_TEXT);
    }
    y += 22;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 4;

    /* List of 32 timbres in current group (or 64 for M/R) */
    int count = (g_cur_tim_group < 2) ? 32 : 64;
    int row_h = 8;
    int col_w = (LCD_W - 16) / 2;
    int rows_per_col = count / 2;
    for (int row = 0; row < rows_per_col; row++) {
        for (int col = 0; col < 2; col++) {
            int tidx = col * rows_per_col + row;
            int x = 6 + col * col_w;
            int ry = y + row * row_h;
            if (ry + row_h > LCD_H - STATUS_BAR_H) break;
            int is_sel = (tidx == g_cur_tim_idx);
            if (is_sel) fill_rect(fb, x - 2, ry - 1, col_w - 4, row_h, COL_CURSOR_BG);
            uint32_t c = is_sel ? COL_CURSOR : COL_TEXT;
            char idx_s[4];
            if (tidx + 1 < 10) { idx_s[0] = '0'; idx_s[1] = (char)('0' + tidx + 1); idx_s[2] = 0; }
            else               { idx_s[0] = (char)('0' + (tidx + 1)/10); idx_s[1] = (char)('0' + (tidx + 1) % 10); idx_s[2] = 0; }
            draw_text(fb, idx_s, x, ry, 1, c);
            const char *nm;
            if (g_cur_tim_group == 2 && tidx < 64) nm = g_timbres_m[tidx].name;
            else nm = placeholder_timbre_names[tidx & 15];
            draw_text(fb, nm, x + 18, ry, 1, c);
        }
    }
    (void)buf;
}

static void tim_handle_input(uint32_t pressed)
{
    int count = (g_cur_tim_group < 2) ? 32 : 64;
    int rows_per_col = count / 2;
    if (pressed & BTN_UP)    g_cur_tim_idx = (g_cur_tim_idx + count - 1) % count;
    if (pressed & BTN_DOWN)  g_cur_tim_idx = (g_cur_tim_idx + 1) % count;
    if (pressed & BTN_LEFT)  { if (g_cur_tim_idx >= rows_per_col) g_cur_tim_idx -= rows_per_col; }
    if (pressed & BTN_RIGHT) { if (g_cur_tim_idx <  rows_per_col) g_cur_tim_idx += rows_per_col; }
    if (pressed & BTN_C_LEFT)  g_cur_tim_group = (g_cur_tim_group + 4 - 1) % 4;
    if (pressed & BTN_C_RIGHT) g_cur_tim_group = (g_cur_tim_group + 1) % 4;
    if (pressed & BTN_A) {
        if (g_cur_tim_group == 2 && g_cur_tim_idx < 64) {
            g_current_timbre = g_cur_tim_idx;
            uart_puts("[mt32] load timbre M#"); uart_putdec(g_cur_tim_idx + 1); uart_puts("\n");
        }
    }
    if (pressed & BTN_Z) sysex_send_timbre(0);
    if (pressed & BTN_C_UP) sysex_request_timbres();
}

/* ================================================================
 *  PARTIAL screen (4 sub-views, Cu/Cd cycles them)
 * ================================================================ */
enum {
    PTL_SUB_PITCH = 0, PTL_SUB_FILTER, PTL_SUB_AMP, PTL_SUB_LFO,
    PTL_SUB_COUNT,
};
static const char *ptl_sub_names[] = {"PITCH", "FILTER", "AMP", "LFO"};

/* Field enums per sub-view */
enum { LF_RATE = 0, LF_DEPTH, LF_PMD, LF_AMD, LF_PCM, LF_ENABLED, LF_COUNT };
enum { PEF_T1 = 0, PEF_L1, PEF_T2, PEF_L2, PEF_T3, PEF_L3, PEF_T4, PEF_L4, PEF_COUNT };
enum { TVF_CUT = 0, TVF_RES, TVF_KEYF, TVF_BIAS,
       TVF_T1, TVF_L1, TVF_T2, TVF_L2, TVF_T3, TVF_L3,
       TVF_T4, TVF_SUS, TVF_REL, TVFV_COUNT };
enum { TVA_LVL = 0, TVA_VEL,
       TVA_T1, TVA_L1, TVA_T2, TVA_L2, TVA_T3, TVA_L3,
       TVA_T4, TVA_SUS, TVA_REL, TVAV_COUNT };

static int ptl_field_count_for_sub(int sub)
{
    switch (sub) {
    case PTL_SUB_PITCH:  return PEF_COUNT;
    case PTL_SUB_FILTER: return TVFV_COUNT;
    case PTL_SUB_AMP:    return TVAV_COUNT;
    case PTL_SUB_LFO:    return LF_COUNT;
    }
    return 0;
}

/* ADSR-ish envelope graph for visual feedback */
static void draw_env_4pt(uint32_t *fb, int x, int y, int w, int h,
                         int t1, int t2, int t3, int t4,
                         int l1, int l2, int l3, int l4)
{
    fill_rect(fb, x, y, w, h, COL_BG_ALT);
    int seg_w = w / 4;
    int seg[4]    = { seg_w, seg_w, seg_w, seg_w };
    int lvls[5]   = { 0, l1, l2, l3, l4 };
    int times[4]  = { t1, t2, t3, t4 };
    (void)times;
    int px = x;
    int py = y + h - 2;
    for (int s = 0; s < 4; s++) {
        int ny = y + h - 2 - (lvls[s+1] * (h - 4)) / 100;
        for (int i = 0; i < seg[s]; i++) {
            int ly = py + (ny - py) * (i + 1) / seg[s];
            if (px + i < x + w && ly < y + h && ly >= y)
                fb[ly * LCD_W + (px + i)] = COL_BAR_FILL;
        }
        px += seg[s];
        py = ny;
    }
}

static void render_screen_ptl(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    mt32_partial_t *pp = &g_timbres_m[g_current_timbre].partial[g_cur_ptl_partial];

    /* Header: timbre + partial + sub-view */
    draw_text(fb, "TIMBRE ", 6, y, 2, COL_HEADING);
    draw_text(fb, g_timbres_m[g_current_timbre].name,
              6 + text_width("TIMBRE ", 2), y, 2, COL_VALUE);
    char ph[12]; ph[0] = 'P'; ph[1] = (char)('0' + g_cur_ptl_partial + 1); ph[2] = ' ';
    ph[3] = '/'; ph[4] = ' '; ph[5] = (char)('0' + 4); ph[6] = 0;
    draw_text(fb, ph, LCD_W - text_width(ph, 2) - 6, y, 2, COL_VALUE);
    y += 18;
    /* Sub-view tabs */
    for (int i = 0; i < PTL_SUB_COUNT; i++) {
        int x = 6 + i * 100;
        int is = (i == g_cur_ptl_subview);
        if (is) fill_rect(fb, x - 4, y - 2, 96, 14, COL_CURSOR_BG);
        draw_text(fb, ptl_sub_names[i], x, y, 1, is ? COL_CURSOR : COL_TEXT);
    }
    y += 14;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 4;

    int row_h = 14;
    int col_w = LCD_W / 2 - 6;
    int xL = 6, xR = LCD_W / 2 + 2;
    int yL = y, yR = y;

    switch (g_cur_ptl_subview) {
    case PTL_SUB_PITCH:
        draw_env_4pt(fb, xR, y, col_w - 4, 60,
                     pp->pe_T1, pp->pe_T2, pp->pe_T3, pp->pe_T4,
                     pp->pe_L1 + 50, pp->pe_L2 + 50, pp->pe_L3 + 50, pp->pe_L4 + 50);
        int_to_str(pp->pe_T1, buf); render_field_c(fb, xL, yL, col_w, row_h, "T1", buf, g_cur_ptl_field == PEF_T1); yL += row_h;
        int_to_str(pp->pe_L1, buf); render_field_c(fb, xL, yL, col_w, row_h, "L1", buf, g_cur_ptl_field == PEF_L1); yL += row_h;
        int_to_str(pp->pe_T2, buf); render_field_c(fb, xL, yL, col_w, row_h, "T2", buf, g_cur_ptl_field == PEF_T2); yL += row_h;
        int_to_str(pp->pe_L2, buf); render_field_c(fb, xL, yL, col_w, row_h, "L2", buf, g_cur_ptl_field == PEF_L2); yL += row_h;
        int_to_str(pp->pe_T3, buf); render_field_c(fb, xL, yL, col_w, row_h, "T3", buf, g_cur_ptl_field == PEF_T3); yL += row_h;
        int_to_str(pp->pe_L3, buf); render_field_c(fb, xL, yL, col_w, row_h, "L3", buf, g_cur_ptl_field == PEF_L3); yL += row_h;
        int_to_str(pp->pe_T4, buf); render_field_c(fb, xL, yL, col_w, row_h, "T4", buf, g_cur_ptl_field == PEF_T4); yL += row_h;
        int_to_str(pp->pe_L4, buf); render_field_c(fb, xL, yL, col_w, row_h, "L4", buf, g_cur_ptl_field == PEF_L4); yL += row_h;
        break;
    case PTL_SUB_FILTER:
        int_to_str(pp->tvf_cutoff,    buf); render_field_c(fb, xL, yL, col_w, row_h, "Cutoff",       buf, g_cur_ptl_field == TVF_CUT);  yL += row_h;
        int_to_str(pp->tvf_resonance, buf); render_field_c(fb, xL, yL, col_w, row_h, "Resonance",    buf, g_cur_ptl_field == TVF_RES);  yL += row_h;
        int_to_str(pp->tvf_keyfollow, buf); render_field_c(fb, xL, yL, col_w, row_h, "Key follow",   buf, g_cur_ptl_field == TVF_KEYF); yL += row_h;
        int_to_str(pp->tvf_bias,      buf); render_field_c(fb, xL, yL, col_w, row_h, "Bias",         buf, g_cur_ptl_field == TVF_BIAS); yL += row_h;
        int_to_str(pp->tvf_T1, buf); render_field_c(fb, xL, yL, col_w, row_h, "T1", buf, g_cur_ptl_field == TVF_T1); yL += row_h;
        int_to_str(pp->tvf_L1, buf); render_field_c(fb, xL, yL, col_w, row_h, "L1", buf, g_cur_ptl_field == TVF_L1); yL += row_h;
        int_to_str(pp->tvf_T2, buf); render_field_c(fb, xL, yL, col_w, row_h, "T2", buf, g_cur_ptl_field == TVF_T2); yL += row_h;

        int_to_str(pp->tvf_L2, buf); render_field_c(fb, xR, yR, col_w, row_h, "L2",      buf, g_cur_ptl_field == TVF_L2); yR += row_h;
        int_to_str(pp->tvf_T3, buf); render_field_c(fb, xR, yR, col_w, row_h, "T3",      buf, g_cur_ptl_field == TVF_T3); yR += row_h;
        int_to_str(pp->tvf_L3, buf); render_field_c(fb, xR, yR, col_w, row_h, "L3",      buf, g_cur_ptl_field == TVF_L3); yR += row_h;
        int_to_str(pp->tvf_T4, buf); render_field_c(fb, xR, yR, col_w, row_h, "T4",      buf, g_cur_ptl_field == TVF_T4); yR += row_h;
        int_to_str(pp->tvf_sustain, buf); render_field_c(fb, xR, yR, col_w, row_h, "Sustain", buf, g_cur_ptl_field == TVF_SUS); yR += row_h;
        int_to_str(pp->tvf_release, buf); render_field_c(fb, xR, yR, col_w, row_h, "Release", buf, g_cur_ptl_field == TVF_REL); yR += row_h;
        break;
    case PTL_SUB_AMP:
        draw_env_4pt(fb, xR, y, col_w - 4, 60,
                     pp->tva_T1, pp->tva_T2, pp->tva_T3, pp->tva_T4,
                     pp->tva_L1, pp->tva_L2, pp->tva_L3, 0);
        int_to_str(pp->tva_level,    buf); render_field_c(fb, xL, yL, col_w, row_h, "Level",    buf, g_cur_ptl_field == TVA_LVL); yL += row_h;
        int_to_str(pp->tva_velocity, buf); render_field_c(fb, xL, yL, col_w, row_h, "Velocity", buf, g_cur_ptl_field == TVA_VEL); yL += row_h;
        int_to_str(pp->tva_T1, buf); render_field_c(fb, xL, yL, col_w, row_h, "T1", buf, g_cur_ptl_field == TVA_T1); yL += row_h;
        int_to_str(pp->tva_L1, buf); render_field_c(fb, xL, yL, col_w, row_h, "L1", buf, g_cur_ptl_field == TVA_L1); yL += row_h;
        int_to_str(pp->tva_T2, buf); render_field_c(fb, xL, yL, col_w, row_h, "T2", buf, g_cur_ptl_field == TVA_T2); yL += row_h;
        int_to_str(pp->tva_L2, buf); render_field_c(fb, xL, yL, col_w, row_h, "L2", buf, g_cur_ptl_field == TVA_L2); yL += row_h;
        int_to_str(pp->tva_T3, buf); render_field_c(fb, xL, yL, col_w, row_h, "T3", buf, g_cur_ptl_field == TVA_T3); yL += row_h;
        int_to_str(pp->tva_L3, buf); render_field_c(fb, xL, yL, col_w, row_h, "L3", buf, g_cur_ptl_field == TVA_L3); yL += row_h;
        int_to_str(pp->tva_T4, buf); render_field_c(fb, xL, yL, col_w, row_h, "T4", buf, g_cur_ptl_field == TVA_T4); yL += row_h;
        int_to_str(pp->tva_sustain, buf); render_field_c(fb, xL, yL, col_w, row_h, "Sustain", buf, g_cur_ptl_field == TVA_SUS); yL += row_h;
        int_to_str(pp->tva_release, buf); render_field_c(fb, xL, yL, col_w, row_h, "Release", buf, g_cur_ptl_field == TVA_REL); yL += row_h;
        break;
    case PTL_SUB_LFO:
        int_to_str(pp->lfo_rate,  buf); render_field_c(fb, xL, yL, col_w, row_h, "Rate",  buf, g_cur_ptl_field == LF_RATE);  yL += row_h;
        int_to_str(pp->lfo_depth, buf); render_field_c(fb, xL, yL, col_w, row_h, "Depth", buf, g_cur_ptl_field == LF_DEPTH); yL += row_h;
        int_to_str(pp->lfo_pmd,   buf); render_field_c(fb, xL, yL, col_w, row_h, "PMD",   buf, g_cur_ptl_field == LF_PMD);   yL += row_h;
        int_to_str(pp->lfo_amd,   buf); render_field_c(fb, xL, yL, col_w, row_h, "AMD",   buf, g_cur_ptl_field == LF_AMD);   yL += row_h;
        int_to_str(pp->pcm_select, buf); render_field_c(fb, xL, yL, col_w, row_h, "PCM/Wave", buf, g_cur_ptl_field == LF_PCM); yL += row_h;
        render_field_c(fb, xL, yL, col_w, row_h, "Enabled",
                       pp->enabled ? "[X]" : "[ ]", g_cur_ptl_field == LF_ENABLED);
        break;
    }
}

static void ptl_handle_input(uint32_t pressed)
{
    /* Cu/Cd cycle partial 1..4; Cl/Cr cycle sub-view */
    if (pressed & BTN_C_UP)    g_cur_ptl_partial = (g_cur_ptl_partial + 4 - 1) % 4;
    if (pressed & BTN_C_DOWN)  g_cur_ptl_partial = (g_cur_ptl_partial + 1) % 4;
    if (pressed & BTN_C_LEFT)  { g_cur_ptl_subview = (g_cur_ptl_subview + PTL_SUB_COUNT - 1) % PTL_SUB_COUNT; g_cur_ptl_field = 0; }
    if (pressed & BTN_C_RIGHT) { g_cur_ptl_subview = (g_cur_ptl_subview + 1) % PTL_SUB_COUNT; g_cur_ptl_field = 0; }
    if (pressed & BTN_Z)       sysex_send_timbre(0);

    int n = ptl_field_count_for_sub(g_cur_ptl_subview);
    if (n == 0) return;
    if (pressed & BTN_UP)   g_cur_ptl_field = (g_cur_ptl_field + n - 1) % n;
    if (pressed & BTN_DOWN) g_cur_ptl_field = (g_cur_ptl_field + 1) % n;
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    mt32_partial_t *pp = &g_timbres_m[g_current_timbre].partial[g_cur_ptl_partial];
    #define CLAMP_U8(v, hi) do { int t = (v) + dir; if (t<0)t=0; if (t>(hi))t=(hi); (v) = (uint8_t)t; } while (0)
    #define CLAMP_S8(v, lo, hi) do { int t = (v) + dir; if (t<(lo))t=(lo); if (t>(hi))t=(hi); (v) = (int8_t)t; } while (0)
    switch (g_cur_ptl_subview) {
    case PTL_SUB_PITCH:
        switch (g_cur_ptl_field) {
        case PEF_T1: CLAMP_U8(pp->pe_T1, 100); break;
        case PEF_L1: CLAMP_S8(pp->pe_L1, -50, 50); break;
        case PEF_T2: CLAMP_U8(pp->pe_T2, 100); break;
        case PEF_L2: CLAMP_S8(pp->pe_L2, -50, 50); break;
        case PEF_T3: CLAMP_U8(pp->pe_T3, 100); break;
        case PEF_L3: CLAMP_S8(pp->pe_L3, -50, 50); break;
        case PEF_T4: CLAMP_U8(pp->pe_T4, 100); break;
        case PEF_L4: CLAMP_S8(pp->pe_L4, -50, 50); break;
        } break;
    case PTL_SUB_FILTER:
        switch (g_cur_ptl_field) {
        case TVF_CUT:  CLAMP_U8(pp->tvf_cutoff, 100); break;
        case TVF_RES:  CLAMP_U8(pp->tvf_resonance, 30); break;
        case TVF_KEYF: CLAMP_S8(pp->tvf_keyfollow, -100, 100); break;
        case TVF_BIAS: CLAMP_S8(pp->tvf_bias, -64, 63); break;
        case TVF_T1:   CLAMP_U8(pp->tvf_T1, 100); break;
        case TVF_L1:   CLAMP_U8(pp->tvf_L1, 100); break;
        case TVF_T2:   CLAMP_U8(pp->tvf_T2, 100); break;
        case TVF_L2:   CLAMP_U8(pp->tvf_L2, 100); break;
        case TVF_T3:   CLAMP_U8(pp->tvf_T3, 100); break;
        case TVF_L3:   CLAMP_U8(pp->tvf_L3, 100); break;
        case TVF_T4:   CLAMP_U8(pp->tvf_T4, 100); break;
        case TVF_SUS:  CLAMP_U8(pp->tvf_sustain, 100); break;
        case TVF_REL:  CLAMP_U8(pp->tvf_release, 100); break;
        } break;
    case PTL_SUB_AMP:
        switch (g_cur_ptl_field) {
        case TVA_LVL:  CLAMP_U8(pp->tva_level, 100); break;
        case TVA_VEL:  CLAMP_U8(pp->tva_velocity, 127); break;
        case TVA_T1:   CLAMP_U8(pp->tva_T1, 100); break;
        case TVA_L1:   CLAMP_U8(pp->tva_L1, 100); break;
        case TVA_T2:   CLAMP_U8(pp->tva_T2, 100); break;
        case TVA_L2:   CLAMP_U8(pp->tva_L2, 100); break;
        case TVA_T3:   CLAMP_U8(pp->tva_T3, 100); break;
        case TVA_L3:   CLAMP_U8(pp->tva_L3, 100); break;
        case TVA_T4:   CLAMP_U8(pp->tva_T4, 100); break;
        case TVA_SUS:  CLAMP_U8(pp->tva_sustain, 100); break;
        case TVA_REL:  CLAMP_U8(pp->tva_release, 100); break;
        } break;
    case PTL_SUB_LFO:
        switch (g_cur_ptl_field) {
        case LF_RATE:  CLAMP_U8(pp->lfo_rate, 100); break;
        case LF_DEPTH: CLAMP_U8(pp->lfo_depth, 100); break;
        case LF_PMD:   CLAMP_U8(pp->lfo_pmd, 100); break;
        case LF_AMD:   CLAMP_U8(pp->lfo_amd, 100); break;
        case LF_PCM:   CLAMP_U8(pp->pcm_select, 255); break;
        case LF_ENABLED: pp->enabled ^= 1; break;
        } break;
    }
    #undef CLAMP_U8
    #undef CLAMP_S8
}

/* ================================================================
 *  RHYTHM screen (85 drum keys, scrollable)
 * ================================================================ */
#define RHY_KEY_FIRST 35
static const char *gm_drum_name(int key)
{
    /* General MIDI drum names for keys 35..81; the rest are extension. */
    static const char *names[] = {
        /* 35 */ "AcouBass1", "AcouBass2", "SideStick", "AcouSnare", "HandClap",
        /* 40 */ "ElecSnare", "LoFloorTom", "ClsHiHat", "HiFloorTom", "PdHiHat",
        /* 45 */ "LoTom", "OpenHiHat", "LowMidTom", "HiMidTom", "CrshCym1",
        /* 50 */ "HighTom", "RideCym1", "ChinCym", "RideBell", "Tamb",
        /* 55 */ "SplashCym", "Cowbell", "CrshCym2", "Vibraslap", "RideCym2",
        /* 60 */ "HiBongo", "LowBongo", "MuteHiConga", "OpenHiConga", "LowConga",
        /* 65 */ "HiTimbale", "LoTimbale", "HiAgogo", "LowAgogo", "Cabasa",
        /* 70 */ "Maracas", "ShWhistle", "LWhistle", "ShortGuiro", "LongGuiro",
        /* 75 */ "Claves", "HiWoodBlk", "LoWoodBlk", "MuteCuica", "OpenCuica",
        /* 80 */ "MuteTri", "OpenTri",
    };
    if (key < 35 || key > 81) return "(ext)";
    return names[key - 35];
}

static void render_screen_rhy(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    draw_text(fb, "RHYTHM KIT", 6, y, 2, COL_HEADING);
    draw_text(fb, "Key  Sound        Timbre  Lvl  Pan  Rev",
              6, y + 20, 1, COL_TEXT_DIM);
    draw_hline(fb, 6, y + 30, LCD_W - 12, COL_TEXT_DIM);
    y += 34;
    int row_h = 14;
    int visible = (LCD_H - y - STATUS_BAR_H) / row_h;
    /* Clamp scroll so selected stays visible */
    if (g_cur_rhy_key < g_rhy_scroll)              g_rhy_scroll = g_cur_rhy_key;
    if (g_cur_rhy_key >= g_rhy_scroll + visible)   g_rhy_scroll = g_cur_rhy_key - visible + 1;
    if (g_rhy_scroll < 0)                          g_rhy_scroll = 0;
    if (g_rhy_scroll > 85 - visible)               g_rhy_scroll = (85 - visible < 0) ? 0 : 85 - visible;

    for (int row = 0; row < visible && (g_rhy_scroll + row) < 85; row++) {
        int idx = g_rhy_scroll + row;
        int ry = y + row * row_h;
        int is = (idx == g_cur_rhy_key);
        if (is) fill_rect(fb, 4, ry - 1, LCD_W - 8, row_h, COL_CURSOR_BG);
        uint32_t c  = is ? COL_CURSOR : COL_TEXT;
        uint32_t cv = is ? COL_CURSOR : COL_VALUE;
        int_to_str(idx + RHY_KEY_FIRST, buf);
        draw_text(fb, buf, 8, ry, 1, c);
        draw_text(fb, gm_drum_name(idx + RHY_KEY_FIRST), 40, ry, 1, c);
        int_to_str(g_rhythm[idx].timbre_num + 1, buf);
        draw_text(fb, "R#", 160, ry, 1, c);
        draw_text(fb, buf, 174, ry, 1, cv);
        int_to_str(g_rhythm[idx].output_level, buf);
        draw_text(fb, buf, 230, ry, 1, cv);
        int_to_str(g_rhythm[idx].panpot, buf);
        draw_text(fb, buf, 290, ry, 1, cv);
        draw_text(fb, g_rhythm[idx].reverb_switch ? "[X]" : "[ ]", 340, ry, 1, cv);
    }
}

static void rhy_handle_input(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_rhy_key = (g_cur_rhy_key + 85 - 1) % 85;
    if (pressed & BTN_DOWN) g_cur_rhy_key = (g_cur_rhy_key + 1) % 85;
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    /* Adjust whichever sub-field is "selected" — for now just bind LR
     * to timbre number (most useful), Cu/Cd to level. */
    if (dir != 0) {
        int t = g_rhythm[g_cur_rhy_key].timbre_num + dir;
        if (t < 0) t = 0; if (t > 63) t = 63;
        g_rhythm[g_cur_rhy_key].timbre_num = (uint8_t)t;
    }
    if (pressed & BTN_A) g_rhythm[g_cur_rhy_key].reverb_switch ^= 1;
    if (pressed & BTN_Z) {
        /* Send rhythm setup for this one key (4 bytes per key at 03 01 10 + key*4 in patch-temp R) */
        uint8_t data[4];
        data[0] = g_rhythm[g_cur_rhy_key].timbre_num;
        data[1] = g_rhythm[g_cur_rhy_key].output_level;
        data[2] = (uint8_t)((int)g_rhythm[g_cur_rhy_key].panpot + 64);
        data[3] = g_rhythm[g_cur_rhy_key].reverb_switch;
        uint32_t addr = 0x030110u + (uint32_t)g_cur_rhy_key * 4u;
        sysex_dt1(addr, data, sizeof(data), "rhy key");
    }
    (void)g_cur_rhy_field; (void)g_rhy_mode;
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void)
{
    uart_puts("\n\n=== MT-32 Sound Editor ===\n");
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

    init_model();

    int active_tab = TAB_CFG;
    uint32_t back_fb  = FB1_ADDR;
    uint32_t front_fb = FB0_ADDR;

    while (1) {
        uint32_t t0 = timer_read();

        (void)input_poll();
        uint32_t pressed = input_pressed();

        int tabs_locked = (active_tab == TAB_PTS && g_pts_mode == 1)
                       || (active_tab == TAB_PCH && g_pch_mode == 1);
        if (!tabs_locked) {
            if (pressed & BTN_L) active_tab = (active_tab + TAB_COUNT - 1) % TAB_COUNT;
            if (pressed & BTN_R) active_tab = (active_tab + 1) % TAB_COUNT;
        }

        const char *hint;
        switch (active_tab) {
        case TAB_CFG:
            hint = "Dpad nav  Dpad-LR adj  Z send sys  Cu req  L/R tab"; break;
        case TAB_PTS:
            hint = (g_pts_mode == 0)
                ? "Du/Dd part  A edit  Z send  L/R tab"
                : "Dpad nav  Dpad-LR adj  Z send  B back"; break;
        case TAB_PCH:
            hint = (g_pch_mode == 0)
                ? "Dpad nav  A edit  Z send  Cu req  L/R tab"
                : "Dpad nav  Dpad-LR adj  Z send  B back"; break;
        case TAB_TIM:
            hint = "Dpad nav  Cl/Cr group  A load  Z send  L/R tab"; break;
        case TAB_PTL:
            hint = "Dpad nav  Dpad-LR adj  Cu/Cd partial  Cl/Cr view  Z send"; break;
        case TAB_RHY:
            hint = "Du/Dd key  Dpad-LR timbre  A toggle rev  Z send key"; break;
        default: hint = "L/R tab";
        }

        switch (active_tab) {
        case TAB_CFG: cfg_handle_input(pressed); break;
        case TAB_PTS: pts_handle_input(pressed); break;
        case TAB_PCH: pch_handle_input(pressed); break;
        case TAB_TIM: tim_handle_input(pressed); break;
        case TAB_PTL: ptl_handle_input(pressed); break;
        case TAB_RHY: rhy_handle_input(pressed); break;
        }

        uint32_t *fb = (uint32_t *)back_fb;
        fill_rect(fb, 0, 0, LCD_W, LCD_H, COL_BG);
        render_tab_bar(fb, active_tab, "");

        switch (active_tab) {
        case TAB_CFG: render_screen_cfg(fb); break;
        case TAB_PTS: render_screen_pts(fb); break;
        case TAB_PCH: render_screen_pch(fb); break;
        case TAB_TIM: render_screen_tim(fb); break;
        case TAB_PTL: render_screen_ptl(fb); break;
        case TAB_RHY: render_screen_rhy(fb); break;
        }
        render_status_bar(fb, hint);

        dcache_clean_fb(back_fb);
        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;
    }
    return 0;
}
