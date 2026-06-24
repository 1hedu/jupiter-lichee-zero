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
 *  PPG Wave 2.3 / Behringer Wave — data model
 *
 * No public SysEx implementation chart exists for the PPG Wave V8.3
 * upgrade. The structures and SysEx packets below follow common
 * PPG/Waldorf conventions (manufacturer ID 0x3E = Waldorf, who own
 * the PPG IP) and are designed to map cleanly to the Behringer Wave
 * via Behringer's MIDI implementation. The manufacturer ID and
 * model byte are #defined here so they can be adjusted in one place
 * once the actual Behringer SysEx chart is verified.
 * ================================================================ */
#define PPG_SYSEX_MFR     0x3E    /* Waldorf manufacturer ID */
#define PPG_SYSEX_MODEL   0x10    /* Wave-class device ID (placeholder) */
#define PPG_CMD_SEND_PROG  0x10   /* DT1-style: send program */
#define PPG_CMD_REQ_PROG   0x00   /* RQ1-style: request program */
#define PPG_CMD_SEND_WAVE  0x12   /* send wavetable */
#define PPG_CMD_REQ_WAVE   0x02   /* request wavetable */
#define PPG_CMD_SEND_SYS   0x14   /* send system block */
#define PPG_CMD_REQ_SYS    0x04   /* request system */

/* Single-cycle wave: 128 8-bit samples (-128..+127 in our model). */
typedef int8_t wave_t[128];

/* A wavetable is 64 waves. */
typedef wave_t wavetable_t[64];

/* A program (voice patch) */
typedef struct {
    char     name[9];               /* 8 chars + null */
    uint8_t  osc1_wavetable;        /* 1..32 */
    uint8_t  osc1_wave_pos;         /* 0..63 */
    int8_t   osc1_semi;             /* -36..+36 */
    int8_t   osc1_detune;           /* -50..+50 cents */
    uint8_t  osc2_wavetable;
    uint8_t  osc2_wave_pos;
    int8_t   osc2_semi;
    int8_t   osc2_detune;
    /* Mixer */
    uint8_t  mix_osc1;              /* 0..127 */
    uint8_t  mix_osc2;
    uint8_t  mix_noise;
    uint8_t  mix_sub;
    /* VCF (24 dB lowpass) */
    uint8_t  vcf_cutoff;            /* 0..127 */
    uint8_t  vcf_resonance;         /* 0..127 */
    int8_t   vcf_env_amt;           /* -64..+63 */
    uint8_t  vcf_key_follow;        /* 0..127 % */
    /* Envelopes 1 (filter), 2 (amp), 3 (wave) — ADSR */
    uint8_t  env1_a, env1_d, env1_s, env1_r;
    uint8_t  env2_a, env2_d, env2_s, env2_r;
    uint8_t  env3_a, env3_d, env3_s, env3_r;
    /* LFO */
    uint8_t  lfo_rate;              /* 0..127 */
    uint8_t  lfo_depth;
    uint8_t  lfo_dest;              /* 0=pitch, 1=cutoff, 2=amp, 3=wave_pos */
    uint8_t  lfo_sync;              /* 0/1 */
    /* Voice */
    uint8_t  voice_mode;            /* 0=poly, 1=mono, 2=legato */
    uint8_t  glide;                 /* 0..127 */
    uint8_t  bend_range;            /* 0..24 */
    int8_t   transpose;             /* -24..+24 */
} ppg_program_t;

/* System (global) */
typedef struct {
    uint8_t  midi_channel;          /* 1..16 */
    uint8_t  device_id;             /* 0..0x7F */
    uint8_t  master_volume;         /* 0..127 */
    int8_t   master_tune;           /* -50..+50 cents */
    uint8_t  global_program;        /* 0..63 currently selected */
} ppg_system_t;

/* Sample slot (Transient Sound Tool) */
typedef struct {
    char     name[9];
    uint32_t length_samples;        /* recorded length */
    uint16_t sample_rate;           /* in 0.1 kHz units (416 = 41.6 kHz) */
    uint8_t  loop_enabled;
    uint32_t loop_start;
    uint32_t loop_end;
} ppg_sample_t;

/* Sequence: 16 steps × 8 tracks */
typedef struct {
    uint8_t  note[16];              /* MIDI note per step */
    uint8_t  velocity[16];          /* 0 = rest */
    uint8_t  gate[16];              /* 0..127 length in % */
} ppg_seq_track_t;

typedef struct {
    char     name[9];
    uint16_t tempo;                 /* BPM */
    uint8_t  swing;                 /* 50..75 % */
    uint8_t  length;                /* 1..16 active steps */
    ppg_seq_track_t track[8];
} ppg_sequence_t;

/* File entry (SD-card mock) */
typedef struct {
    char     name[16];
    uint8_t  kind;                  /* 0=program, 1=wavetable, 2=sample, 3=seq, 4=sys */
    uint32_t size_bytes;
} ppg_file_t;

/* ----- Globals ----- */
#define NUM_PROGRAMS   64
#define NUM_WAVETABLES 32
#define NUM_SAMPLES     8
#define NUM_SEQUENCES  16
#define NUM_FILES      32

static ppg_system_t   g_sys;
static ppg_program_t  g_programs[NUM_PROGRAMS];
static ppg_sample_t   g_samples[NUM_SAMPLES];
static ppg_sequence_t g_sequences[NUM_SEQUENCES];
static ppg_file_t     g_files[NUM_FILES];
static uint8_t        g_file_count = 0;
/* Note: we do NOT statically allocate 32 wavetables × 64 waves ×
 * 128 samples (= 256 KB) — too heavy. Wavetables are procedural
 * (rendered live in render_page_wavetable based on table+wave idx). */

/* Forward decls for cursor state used in both renderers and handlers
 * (full handler definitions live below the renderers; we just hoist
 * the cursor globals so the renderers can read them). */
enum { SF_SLOT = 0, SF_LEN, SF_RATE, SF_LOOP, SF_LSTART, SF_LEND, SF_COUNT };
static int g_cur_sample        = 0;
static int g_cur_sample_field  = 0;
static int g_cur_seq           = 0;
static int g_cur_seq_track     = 0;
static int g_cur_seq_step      = 0;

/* ================================================================
 *  SysEx wire-format builder (PPG/Waldorf convention)
 *
 *   F0 <mfr> <dev_id> <model> <cmd> <addr_hi> <addr_low> [data] <cs> F7
 *
 *   checksum = (sum of all bytes between mfr and last data) & 0x7F
 *              (placeholder convention — adjust to match Behringer)
 * ================================================================ */
static void uart_puthex_byte(uint8_t b)
{
    static const char hex[] = "0123456789ABCDEF";
    char s[3]; s[0] = hex[b >> 4]; s[1] = hex[b & 0xF]; s[2] = 0;
    uart_puts(s);
}

static void midi_send_bytes(const uint8_t *buf, uint32_t len, const char *label)
{
    uart_puts("[midi tx "); uart_puts(label); uart_puts("] ");
    uart_putdec(len); uart_puts(" bytes:");
    for (uint32_t i = 0; i < len; i++) {
        uart_puts(" ");
        uart_puthex_byte(buf[i]);
        if (((i + 1) % 16) == 0 && (i + 1) < len) uart_puts("\n             ");
    }
    uart_puts("\n");
}

static uint8_t ppg_checksum(const uint8_t *bytes, uint32_t n)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; i++) sum += bytes[i];
    return (uint8_t)(sum & 0x7F);
}

/* Send one ppg_program_t to the device. We serialize the canonical
 * order of the fields we model — Behringer-side firmware will need
 * to map this to its own param IDs (typically via SysEx upgrade). */
static void sysex_send_program(uint8_t prog_idx)
{
    if (prog_idx >= NUM_PROGRAMS) return;
    ppg_program_t *p = &g_programs[prog_idx];
    uint8_t pkt[256]; uint8_t *q = pkt;
    *q++ = 0xF0;
    *q++ = PPG_SYSEX_MFR;
    *q++ = g_sys.device_id;
    *q++ = PPG_SYSEX_MODEL;
    *q++ = PPG_CMD_SEND_PROG;
    *q++ = 0;                                 /* addr hi: program bank */
    *q++ = prog_idx & 0x7F;                   /* addr lo: program number */
    uint8_t *cs_start = q;
    /* Name (8 chars) */
    for (int i = 0; i < 8; i++)
        *q++ = (uint8_t)(p->name[i] ? (p->name[i] & 0x7F) : ' ');
    /* Oscillators */
    *q++ = p->osc1_wavetable & 0x7F;  *q++ = p->osc1_wave_pos & 0x7F;
    *q++ = (uint8_t)((int)p->osc1_semi + 64);
    *q++ = (uint8_t)((int)p->osc1_detune + 64);
    *q++ = p->osc2_wavetable & 0x7F;  *q++ = p->osc2_wave_pos & 0x7F;
    *q++ = (uint8_t)((int)p->osc2_semi + 64);
    *q++ = (uint8_t)((int)p->osc2_detune + 64);
    /* Mix */
    *q++ = p->mix_osc1 & 0x7F;  *q++ = p->mix_osc2 & 0x7F;
    *q++ = p->mix_noise & 0x7F; *q++ = p->mix_sub & 0x7F;
    /* VCF */
    *q++ = p->vcf_cutoff & 0x7F;  *q++ = p->vcf_resonance & 0x7F;
    *q++ = (uint8_t)((int)p->vcf_env_amt + 64);
    *q++ = p->vcf_key_follow & 0x7F;
    /* Envelopes 1/2/3 */
    *q++ = p->env1_a & 0x7F; *q++ = p->env1_d & 0x7F;
    *q++ = p->env1_s & 0x7F; *q++ = p->env1_r & 0x7F;
    *q++ = p->env2_a & 0x7F; *q++ = p->env2_d & 0x7F;
    *q++ = p->env2_s & 0x7F; *q++ = p->env2_r & 0x7F;
    *q++ = p->env3_a & 0x7F; *q++ = p->env3_d & 0x7F;
    *q++ = p->env3_s & 0x7F; *q++ = p->env3_r & 0x7F;
    /* LFO */
    *q++ = p->lfo_rate & 0x7F;  *q++ = p->lfo_depth & 0x7F;
    *q++ = p->lfo_dest & 0x7F;  *q++ = p->lfo_sync & 0x7F;
    /* Voice */
    *q++ = p->voice_mode & 0x7F; *q++ = p->glide & 0x7F;
    *q++ = p->bend_range & 0x7F;
    *q++ = (uint8_t)((int)p->transpose + 64);
    /* Checksum + end */
    {
        uint8_t cs = ppg_checksum(cs_start, (uint32_t)(q - cs_start));
        *q++ = cs;
    }
    *q++ = 0xF7;
    midi_send_bytes(pkt, (uint32_t)(q - pkt), "program");
}

static void sysex_request_program(uint8_t prog_idx)
{
    uint8_t pkt[10];
    pkt[0] = 0xF0; pkt[1] = PPG_SYSEX_MFR; pkt[2] = g_sys.device_id; pkt[3] = PPG_SYSEX_MODEL;
    pkt[4] = PPG_CMD_REQ_PROG; pkt[5] = 0; pkt[6] = prog_idx & 0x7F;
    pkt[7] = ppg_checksum(&pkt[5], 2);
    pkt[8] = 0xF7;
    midi_send_bytes(pkt, 9, "req program");
}

static void sysex_request_wavetable(uint8_t table_idx)
{
    uint8_t pkt[10];
    pkt[0] = 0xF0; pkt[1] = PPG_SYSEX_MFR; pkt[2] = g_sys.device_id; pkt[3] = PPG_SYSEX_MODEL;
    pkt[4] = PPG_CMD_REQ_WAVE; pkt[5] = 0; pkt[6] = table_idx & 0x7F;
    pkt[7] = ppg_checksum(&pkt[5], 2);
    pkt[8] = 0xF7;
    midi_send_bytes(pkt, 9, "req wavetable");
}

static void sysex_send_system(void)
{
    uint8_t pkt[16]; uint8_t *q = pkt;
    *q++ = 0xF0; *q++ = PPG_SYSEX_MFR; *q++ = g_sys.device_id; *q++ = PPG_SYSEX_MODEL;
    *q++ = PPG_CMD_SEND_SYS; *q++ = 0; *q++ = 0;
    uint8_t *cs_start = q;
    *q++ = g_sys.midi_channel - 1;
    *q++ = g_sys.master_volume & 0x7F;
    *q++ = (uint8_t)((int)g_sys.master_tune + 64);
    *q++ = g_sys.global_program & 0x7F;
    {
        uint8_t cs = ppg_checksum(cs_start, (uint32_t)(q - cs_start));
        *q++ = cs;
    }
    *q++ = 0xF7;
    midi_send_bytes(pkt, (uint32_t)(q - pkt), "system");
}

/* ================================================================
 *  Model init — placeholder defaults
 * ================================================================ */
static const char *placeholder_prog_names[] = {
    "PPG BRSS","STRINGS1","SAW LEAD","SUBBASS1","PADWASH",
    "WAVE PAD","HARP HIT","CHOIRSYN","BELLPLNK","FUNKBASS",
    "ARP SEQ ","NOISE FX","DRUM HIT","FORMANT ","E.PIANO ",
    "ORG WAVE",
};

static void copy_name(char *dst, const char *src, int max)
{
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    while (i < max - 1)           { dst[i++] = ' '; }
    dst[max - 1] = 0;
}

static void init_model(void)
{
    /* System */
    g_sys.midi_channel    = 1;
    g_sys.device_id       = 0x00;
    g_sys.master_volume   = 100;
    g_sys.master_tune     = 0;
    g_sys.global_program  = 0;

    /* Programs */
    for (int i = 0; i < NUM_PROGRAMS; i++) {
        ppg_program_t *p = &g_programs[i];
        copy_name(p->name, placeholder_prog_names[i & 15], 9);
        p->osc1_wavetable = (uint8_t)(1 + (i & 31));
        p->osc1_wave_pos  = 0;
        p->osc1_semi      = 0;
        p->osc1_detune    = 0;
        p->osc2_wavetable = (uint8_t)(1 + ((i + 7) & 31));
        p->osc2_wave_pos  = 32;
        p->osc2_semi      = -12;
        p->osc2_detune    = -7;
        p->mix_osc1 = 100; p->mix_osc2 = 80;
        p->mix_noise = 0;  p->mix_sub  = 0;
        p->vcf_cutoff = 90; p->vcf_resonance = 12;
        p->vcf_env_amt = 24; p->vcf_key_follow = 100;
        p->env1_a = 4;  p->env1_d = 30; p->env1_s = 60;  p->env1_r = 50;
        p->env2_a = 1;  p->env2_d = 5;  p->env2_s = 100; p->env2_r = 24;
        p->env3_a = 0;  p->env3_d = 8;  p->env3_s = 70;  p->env3_r = 24;
        p->lfo_rate = 50; p->lfo_depth = 8; p->lfo_dest = 0; p->lfo_sync = 0;
        p->voice_mode = 0; p->glide = 0; p->bend_range = 2; p->transpose = 0;
    }

    /* Samples */
    for (int i = 0; i < NUM_SAMPLES; i++) {
        copy_name(g_samples[i].name, i == 0 ? "KICK    " : "(empty) ", 9);
        g_samples[i].length_samples = i == 0 ? 16384 : 0;
        g_samples[i].sample_rate    = 416;     /* 41.6 kHz */
        g_samples[i].loop_enabled   = 0;
        g_samples[i].loop_start     = 0;
        g_samples[i].loop_end       = 0;
    }

    /* Sequences */
    for (int s = 0; s < NUM_SEQUENCES; s++) {
        copy_name(g_sequences[s].name, "SEQ     ", 9);
        g_sequences[s].name[3] = (char)('A' + (s & 7));
        g_sequences[s].tempo  = 120;
        g_sequences[s].swing  = 50;
        g_sequences[s].length = 16;
        for (int t = 0; t < 8; t++) {
            for (int k = 0; k < 16; k++) {
                g_sequences[s].track[t].note[k]     = 60;
                g_sequences[s].track[t].velocity[k] = (k % 4 == 0) ? 100 : 0;
                g_sequences[s].track[t].gate[k]     = 80;
            }
        }
    }

    /* Files (mock SD listing) */
    static const struct { const char *n; uint8_t k; uint32_t s; } init_files[] = {
        {"WT_BRASS.WAV   ", 1, 1024},
        {"WT_STRINGS.WAV ", 1, 2048},
        {"WT_LEAD.WAV    ", 1, 1024},
        {"SMP_KICK.PCM   ", 2, 256},
        {"SMP_SNARE.PCM  ", 2, 256},
        {"SEQ_TRACK1.SEQ ", 3, 512},
        {"PROG_BANK.PRG  ", 0, 768},
        {"SYS_GLOBAL.SYS ", 4, 128},
    };
    g_file_count = sizeof(init_files) / sizeof(init_files[0]);
    for (uint8_t i = 0; i < g_file_count; i++) {
        copy_name(g_files[i].name, init_files[i].n, 16);
        g_files[i].kind       = init_files[i].k;
        g_files[i].size_bytes = init_files[i].s;
    }
}

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
/* Voice-program fields, in render order. The cursor moves through
 * these via D-pad up/down; D-pad left/right adjusts. */
enum {
    VF_PROG = 0,
    VF_OSC1_TBL, VF_OSC1_POS, VF_OSC1_SEMI, VF_OSC1_DET,
    VF_OSC2_TBL, VF_OSC2_POS, VF_OSC2_SEMI, VF_OSC2_DET,
    VF_MIX_O1, VF_MIX_O2, VF_MIX_NS, VF_MIX_SUB,
    VF_VCF_CUT, VF_VCF_RES, VF_VCF_ENV, VF_VCF_KF,
    VF_E1A, VF_E1D, VF_E1S, VF_E1R,
    VF_E2A, VF_E2D, VF_E2S, VF_E2R,
    VF_E3A, VF_E3D, VF_E3S, VF_E3R,
    VF_LFO_RATE, VF_LFO_DEPTH, VF_LFO_DEST, VF_LFO_SYNC,
    VF_VOICE, VF_GLIDE, VF_BEND, VF_TRANSPOSE,
    VF_COUNT,
};
static int g_cur_prog       = 0;
static int g_cur_voice_field = 0;

static const char *lfo_dest_names[]   = {"pitch","cutof","amp  ","wpos "};
static const char *voice_mode_names[] = {"poly ","mono ","legto"};

/* Render a labeled field at (x,y). `is_cur` highlights it as cursor. */
static void rv_int(uint32_t *fb, int x, int y, const char *label,
                   int value, int is_cur)
{
    char buf[12]; int_to_str(value, buf);
    uint32_t c = is_cur ? COL_PHOS_HOT : COL_PHOS_HI;
    if (is_cur) draw_text(fb, ">", x - 6, y, 1, COL_PHOS_HOT);
    draw_text(fb, label, x, y, 1, COL_PHOS_MID);
    int lw = text_width(label, 1);
    draw_text(fb, buf, x + lw + 2, y, 1, c);
}
static void rv_str(uint32_t *fb, int x, int y, const char *label,
                   const char *value, int is_cur)
{
    uint32_t c = is_cur ? COL_PHOS_HOT : COL_PHOS_HI;
    if (is_cur) draw_text(fb, ">", x - 6, y, 1, COL_PHOS_HOT);
    draw_text(fb, label, x, y, 1, COL_PHOS_MID);
    int lw = text_width(label, 1);
    draw_text(fb, value, x + lw + 2, y, 1, c);
}

static void render_page_voice(uint32_t *fb)
{
    int y = 28;
    ppg_program_t *p = &g_programs[g_cur_prog];
    render_section_divider(fb, y, " VOICE PROGRAM ");
    y += 12;
    char buf[16]; int_to_str(g_cur_prog + 1, buf);
    int is = (g_cur_voice_field == VF_PROG);
    if (is) draw_text(fb, ">", 0, y, 1, COL_PHOS_HOT);
    draw_text(fb, "PROG #", 6, y, 1, COL_PHOS_MID);
    draw_text(fb, buf, 50, y, 1, is ? COL_PHOS_HOT : COL_PHOS_HOT);
    draw_text(fb, p->name, 80, y, 1, COL_PHOS_HI);
    y += 12;

    /* Three-column compact layout. Each row holds 3 fields side-by-side. */
    int col_w = LCD_W / 3;
    int row_h = 9;
    #define RV3(yA, lA, vA, fA, lB, vB, fB, lC, vC, fC) do {                \
        rv_int(fb, 8,            yA, lA, vA, g_cur_voice_field == fA);       \
        rv_int(fb, 8 + col_w,    yA, lB, vB, g_cur_voice_field == fB);       \
        rv_int(fb, 8 + col_w*2,  yA, lC, vC, g_cur_voice_field == fC);       \
    } while (0)

    RV3(y, "OSC1 TBL ", p->osc1_wavetable, VF_OSC1_TBL,
            "POS ",     p->osc1_wave_pos,  VF_OSC1_POS,
            "SEMI ",    p->osc1_semi,      VF_OSC1_SEMI); y += row_h;
    rv_int(fb, 8 + col_w*2 + 80, y - row_h, "DET ", p->osc1_detune, g_cur_voice_field == VF_OSC1_DET);

    RV3(y, "OSC2 TBL ", p->osc2_wavetable, VF_OSC2_TBL,
            "POS ",     p->osc2_wave_pos,  VF_OSC2_POS,
            "SEMI ",    p->osc2_semi,      VF_OSC2_SEMI); y += row_h;
    rv_int(fb, 8 + col_w*2 + 80, y - row_h, "DET ", p->osc2_detune, g_cur_voice_field == VF_OSC2_DET);
    y += 4;

    RV3(y, "MIX O1 ",  p->mix_osc1, VF_MIX_O1,
            "O2 ",      p->mix_osc2, VF_MIX_O2,
            "NS ",      p->mix_noise,VF_MIX_NS); y += row_h;
    rv_int(fb, 8 + col_w*2 + 60, y - row_h, "SUB ", p->mix_sub, g_cur_voice_field == VF_MIX_SUB);
    y += 4;

    RV3(y, "VCF CUT ", p->vcf_cutoff,    VF_VCF_CUT,
            "RES ",     p->vcf_resonance, VF_VCF_RES,
            "ENV ",     p->vcf_env_amt,   VF_VCF_ENV); y += row_h;
    rv_int(fb, 8 + col_w*2 + 60, y - row_h, "KF ", p->vcf_key_follow, g_cur_voice_field == VF_VCF_KF);
    y += 4;

    /* ENV grid (3 envs × ADSR) */
    draw_text(fb, "         A    D    S    R", 8, y, 1, COL_PHOS_DIM); y += row_h;
    RV3(y, "ENV1 fil ", p->env1_a, VF_E1A,
            "  ",       p->env1_d, VF_E1D,
            "  ",       p->env1_s, VF_E1S); y += row_h;
    rv_int(fb, 8 + col_w*2 + 30, y - row_h, "  ", p->env1_r, g_cur_voice_field == VF_E1R);
    RV3(y, "ENV2 amp ", p->env2_a, VF_E2A,
            "  ",       p->env2_d, VF_E2D,
            "  ",       p->env2_s, VF_E2S); y += row_h;
    rv_int(fb, 8 + col_w*2 + 30, y - row_h, "  ", p->env2_r, g_cur_voice_field == VF_E2R);
    RV3(y, "ENV3 wav ", p->env3_a, VF_E3A,
            "  ",       p->env3_d, VF_E3D,
            "  ",       p->env3_s, VF_E3S); y += row_h;
    rv_int(fb, 8 + col_w*2 + 30, y - row_h, "  ", p->env3_r, g_cur_voice_field == VF_E3R);
    y += 4;

    RV3(y, "LFO R ",  p->lfo_rate,  VF_LFO_RATE,
            "D ",      p->lfo_depth, VF_LFO_DEPTH,
            "SYN ",    p->lfo_sync,  VF_LFO_SYNC); y += row_h;
    rv_str(fb, 8 + col_w*2 + 60, y - row_h, "DST ",
           lfo_dest_names[p->lfo_dest & 3], g_cur_voice_field == VF_LFO_DEST);

    rv_str(fb, 8,             y, "VOICE ", voice_mode_names[p->voice_mode % 3],
           g_cur_voice_field == VF_VOICE);
    rv_int(fb, 8 + col_w,     y, "GLIDE ", p->glide, g_cur_voice_field == VF_GLIDE);
    rv_int(fb, 8 + col_w*2,   y, "BEND ",  p->bend_range, g_cur_voice_field == VF_BEND);
    rv_int(fb, 8 + col_w*2+60,y, "TRP ",   p->transpose, g_cur_voice_field == VF_TRANSPOSE);
    #undef RV3
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
    char buf[32];
    ppg_sample_t *s = &g_samples[g_cur_sample];
    render_section_divider(fb, y, " TRANSIENT SOUND TOOL ");
    y += 12;
    int is;
    is = (g_cur_sample_field == SF_SLOT);
    if (is) draw_text(fb, ">", 0, y, 1, COL_PHOS_HOT);
    draw_text(fb, "SLOT", 6, y, 1, COL_PHOS_MID);
    int_to_str(g_cur_sample + 1, buf);
    draw_text(fb, buf, 38, y, 1, is ? COL_PHOS_HOT : COL_PHOS_HOT);
    draw_text(fb, "/8", 58, y, 1, COL_PHOS_DIM);
    draw_text(fb, "NAME ", 90, y, 1, COL_PHOS_MID);
    draw_text(fb, s->name, 124, y, 1, COL_PHOS_HI);
    y += 12;

    is = (g_cur_sample_field == SF_LEN);
    if (is) draw_text(fb, ">", 0, y, 1, COL_PHOS_HOT);
    draw_text(fb, "LENGTH", 6, y, 1, COL_PHOS_MID);
    int_to_str((int)s->length_samples, buf);
    draw_text(fb, buf, 60, y, 1, is ? COL_PHOS_HOT : COL_PHOS_HI);
    draw_text(fb, "samples", 130, y, 1, COL_PHOS_DIM);

    is = (g_cur_sample_field == SF_RATE);
    if (is) draw_text(fb, ">", 240, y, 1, COL_PHOS_HOT);
    draw_text(fb, "RATE", 248, y, 1, COL_PHOS_MID);
    /* Show as kHz with one decimal: rate stored × 0.1 kHz */
    int khz_int  = s->sample_rate / 10;
    int khz_frac = s->sample_rate % 10;
    int_to_str(khz_int, buf);
    int blen = 0; while (buf[blen]) blen++;
    buf[blen++] = '.'; buf[blen++] = (char)('0' + khz_frac);
    buf[blen++] = 'k'; buf[blen++] = 'H'; buf[blen++] = 'z'; buf[blen] = 0;
    draw_text(fb, buf, 285, y, 1, is ? COL_PHOS_HOT : COL_PHOS_HI);
    y += 12;

    is = (g_cur_sample_field == SF_LOOP);
    if (is) draw_text(fb, ">", 0, y, 1, COL_PHOS_HOT);
    draw_text(fb, "LOOP", 6, y, 1, COL_PHOS_MID);
    draw_text(fb, s->loop_enabled ? "[X]" : "[ ]", 38, y, 1, is ? COL_PHOS_HOT : COL_PHOS_HI);

    is = (g_cur_sample_field == SF_LSTART);
    if (is) draw_text(fb, ">", 100, y, 1, COL_PHOS_HOT);
    draw_text(fb, "START", 108, y, 1, COL_PHOS_MID);
    int_to_str((int)s->loop_start, buf);
    draw_text(fb, buf, 148, y, 1, is ? COL_PHOS_HOT : COL_PHOS_HI);

    is = (g_cur_sample_field == SF_LEND);
    if (is) draw_text(fb, ">", 240, y, 1, COL_PHOS_HOT);
    draw_text(fb, "END", 248, y, 1, COL_PHOS_MID);
    int_to_str((int)s->loop_end, buf);
    draw_text(fb, buf, 280, y, 1, is ? COL_PHOS_HOT : COL_PHOS_HI);
    y += 14;

    /* Sampled-waveform display (procedural, varies per slot) */
    int wx = 8, ww = LCD_W - 16, wh = 100;
    fill_rect(fb, wx, y, ww, 1, COL_PHOS_DIM);
    fill_rect(fb, wx, y + wh, ww, 1, COL_PHOS_DIM);
    int cy = y + wh / 2;
    uint32_t seed = (uint32_t)g_cur_sample * 0x9E3779B1u;
    for (int x = 0; x < ww; x++) {
        int env = (x * x + (x ^ 0x37)) & 0x3F;
        int sv = (int)((((seed + (uint32_t)(x * 17 + 3)) ^ (uint32_t)(x * 7)) & 0x7F) - 64);
        int v = (sv * env) / 64;
        int py = cy - v / 2;
        if (py >= y && py < y + wh)
            fb[py * LCD_W + (wx + x)] = COL_PHOS_HI;
    }
    y += wh + 4;
    draw_text(fb, "COMMENT: ready", 6, y, 1, COL_PHOS_DIM);
}

/* ================================================================
 *  Page 5 — Sequence Editor (placeholder)
 * ================================================================ */
static void render_page_sequence(uint32_t *fb)
{
    int y = 28;
    char buf[16];
    ppg_sequence_t *seq = &g_sequences[g_cur_seq];
    render_section_divider(fb, y, " SEQUENCE EDITOR ");
    y += 12;
    /* Header: SEQ name + tempo + track + step indicator */
    draw_text(fb, "SEQ", 6, y, 1, COL_PHOS_MID);
    int_to_str(g_cur_seq + 1, buf);
    draw_text(fb, buf, 28, y, 1, COL_PHOS_HOT);
    draw_text(fb, seq->name, 50, y, 1, COL_PHOS_HI);
    draw_text(fb, "TEMPO", 130, y, 1, COL_PHOS_MID);
    int_to_str(seq->tempo, buf);
    draw_text(fb, buf, 165, y, 1, COL_PHOS_HI);
    draw_text(fb, "TRACK", 220, y, 1, COL_PHOS_MID);
    int_to_str(g_cur_seq_track + 1, buf);
    draw_text(fb, buf, 254, y, 1, COL_PHOS_HOT);
    draw_text(fb, "/8", 270, y, 1, COL_PHOS_DIM);
    y += 12;

    /* 16-step grid for the currently-selected track */
    int sx = 8, sy = y, cw = (LCD_W - 16 - 2) / 16, ch = 16;
    ppg_seq_track_t *trk = &seq->track[g_cur_seq_track];
    for (int s = 0; s < 16; s++) {
        int x = sx + s * cw;
        uint32_t bg = ((s & 3) == 0) ? COL_PHOS_DIM : COL_PHOS_BG;
        fill_rect(fb, x, sy, cw - 2, ch, bg);
        if (trk->velocity[s] > 0)
            fill_rect(fb, x, sy, cw - 2, ch, COL_PHOS_HI);
        if (s == g_cur_seq_step)
            fill_rect(fb, x, sy + ch + 1, cw - 2, 2, COL_PHOS_HOT);
        char nb[3]; nb[0] = (char)('0' + (s + 1) / 10); nb[1] = (char)('0' + (s + 1) % 10); nb[2] = 0;
        draw_text(fb, nb, x + 2, sy + 5, 1, (trk->velocity[s] > 0) ? COL_PHOS_BG : COL_PHOS_MID);
    }
    y += ch + 8;

    /* Selected step info */
    draw_text(fb, "STEP", 6, y, 1, COL_PHOS_MID);
    int_to_str(g_cur_seq_step + 1, buf);
    draw_text(fb, buf, 32, y, 1, COL_PHOS_HOT);
    draw_text(fb, "NOTE", 70, y, 1, COL_PHOS_MID);
    int_to_str(trk->note[g_cur_seq_step], buf);
    draw_text(fb, buf, 100, y, 1, COL_PHOS_HI);
    draw_text(fb, "VEL", 140, y, 1, COL_PHOS_MID);
    int_to_str(trk->velocity[g_cur_seq_step], buf);
    draw_text(fb, buf, 165, y, 1, COL_PHOS_HI);
    draw_text(fb, "GATE", 210, y, 1, COL_PHOS_MID);
    int_to_str(trk->gate[g_cur_seq_step], buf);
    draw_text(fb, buf, 240, y, 1, COL_PHOS_HI);
    y += 12;
    draw_text(fb, "A = toggle step  Cu/Cd = change sequence", 6, y, 1, COL_PHOS_DIM);
}

/* ================================================================
 *  Page 9 — File Search
 * ================================================================ */
static int g_cur_file = 0;

static const char *file_kind_name(uint8_t k)
{
    switch (k) {
    case 0: return "PROG";
    case 1: return "WTBL";
    case 2: return "SMPL";
    case 3: return "SEQ ";
    case 4: return "SYS ";
    }
    return "????";
}

static void render_page_file(uint32_t *fb)
{
    int y = 28;
    char buf[16];
    render_section_divider(fb, y, " FILE SEARCH ");
    y += 12;
    draw_text(fb, "DEV: SD-CARD   FILES:", 6, y, 1, COL_PHOS_MID);
    int_to_str(g_file_count, buf);
    draw_text(fb, buf, 130, y, 1, COL_PHOS_HI);
    draw_text(fb, "FREE: 184 KB", LCD_W - 100, y, 1, COL_PHOS_MID);
    y += 12;
    draw_text(fb, "NAME              KIND  SIZE", 8, y, 1, COL_PHOS_DIM);
    y += 9;
    fill_rect(fb, 4, y, LCD_W - 8, 1, COL_PHOS_DIM);
    y += 3;
    for (uint8_t i = 0; i < g_file_count; i++) {
        int is_sel = ((int)i == g_cur_file);
        uint32_t c = is_sel ? COL_PHOS_HOT : COL_PHOS_HI;
        if (is_sel) draw_text(fb, ">", 0, y, 1, COL_PHOS_HOT);
        draw_text(fb, g_files[i].name, 8, y, 1, c);
        draw_text(fb, file_kind_name(g_files[i].kind), 110, y, 1, is_sel ? COL_PHOS_HOT : COL_PHOS_MID);
        int_to_str((int)g_files[i].size_bytes, buf);
        draw_text(fb, buf, 160, y, 1, c);
        draw_text(fb, "B", 195, y, 1, COL_PHOS_DIM);
        y += 10;
    }
}

/* ================================================================
 *  Per-page input handlers
 * ================================================================ */
static void handle_system(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_component = (g_cur_component + 8 - 1) % 8;
    if (pressed & BTN_DOWN) g_cur_component = (g_cur_component + 1) % 8;
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (dir) {
        /* Tap LEFT/RIGHT to adjust MIDI channel as a quick demo
         * (full system editing comes later). */
        int v = g_sys.midi_channel + dir;
        if (v < 1) v = 1; if (v > 16) v = 16;
        g_sys.midi_channel = (uint8_t)v;
    }
    if (pressed & BTN_Z)    sysex_send_system();
    if (pressed & BTN_C_UP) sysex_request_program(g_sys.global_program);
}

static void handle_voice(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_voice_field = (g_cur_voice_field + VF_COUNT - 1) % VF_COUNT;
    if (pressed & BTN_DOWN) g_cur_voice_field = (g_cur_voice_field + 1) % VF_COUNT;
    if (pressed & BTN_Z)    sysex_send_program(g_cur_prog);
    if (pressed & BTN_C_UP) sysex_request_program(g_cur_prog);
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    ppg_program_t *p = &g_programs[g_cur_prog];
    #define CU(v, hi) do { int _t = (v) + dir; if (_t<0)_t=0; if (_t>(hi))_t=(hi); (v) = (uint8_t)_t; } while (0)
    #define CS(v, lo, hi) do { int _t = (v) + dir; if (_t<(lo))_t=(lo); if (_t>(hi))_t=(hi); (v) = (int8_t)_t; } while (0)
    switch (g_cur_voice_field) {
    case VF_PROG:      { int v = g_cur_prog + dir; if (v<0)v=0; if (v>=NUM_PROGRAMS)v=NUM_PROGRAMS-1; g_cur_prog = v; } break;
    case VF_OSC1_TBL:  { int v = p->osc1_wavetable + dir; if (v<1)v=1; if (v>32)v=32; p->osc1_wavetable = (uint8_t)v; } break;
    case VF_OSC1_POS:  CU(p->osc1_wave_pos, 63); break;
    case VF_OSC1_SEMI: CS(p->osc1_semi, -36, 36); break;
    case VF_OSC1_DET:  CS(p->osc1_detune, -50, 50); break;
    case VF_OSC2_TBL:  { int v = p->osc2_wavetable + dir; if (v<1)v=1; if (v>32)v=32; p->osc2_wavetable = (uint8_t)v; } break;
    case VF_OSC2_POS:  CU(p->osc2_wave_pos, 63); break;
    case VF_OSC2_SEMI: CS(p->osc2_semi, -36, 36); break;
    case VF_OSC2_DET:  CS(p->osc2_detune, -50, 50); break;
    case VF_MIX_O1:    CU(p->mix_osc1, 127); break;
    case VF_MIX_O2:    CU(p->mix_osc2, 127); break;
    case VF_MIX_NS:    CU(p->mix_noise, 127); break;
    case VF_MIX_SUB:   CU(p->mix_sub, 127); break;
    case VF_VCF_CUT:   CU(p->vcf_cutoff, 127); break;
    case VF_VCF_RES:   CU(p->vcf_resonance, 127); break;
    case VF_VCF_ENV:   CS(p->vcf_env_amt, -64, 63); break;
    case VF_VCF_KF:    CU(p->vcf_key_follow, 127); break;
    case VF_E1A: CU(p->env1_a, 127); break;
    case VF_E1D: CU(p->env1_d, 127); break;
    case VF_E1S: CU(p->env1_s, 127); break;
    case VF_E1R: CU(p->env1_r, 127); break;
    case VF_E2A: CU(p->env2_a, 127); break;
    case VF_E2D: CU(p->env2_d, 127); break;
    case VF_E2S: CU(p->env2_s, 127); break;
    case VF_E2R: CU(p->env2_r, 127); break;
    case VF_E3A: CU(p->env3_a, 127); break;
    case VF_E3D: CU(p->env3_d, 127); break;
    case VF_E3S: CU(p->env3_s, 127); break;
    case VF_E3R: CU(p->env3_r, 127); break;
    case VF_LFO_RATE:  CU(p->lfo_rate, 127); break;
    case VF_LFO_DEPTH: CU(p->lfo_depth, 127); break;
    case VF_LFO_DEST:  { int v = p->lfo_dest + dir; if (v<0)v=0; if (v>3)v=3; p->lfo_dest = (uint8_t)v; } break;
    case VF_LFO_SYNC:  p->lfo_sync ^= 1; break;
    case VF_VOICE:     { int v = p->voice_mode + dir; if (v<0)v=0; if (v>2)v=2; p->voice_mode = (uint8_t)v; } break;
    case VF_GLIDE:     CU(p->glide, 127); break;
    case VF_BEND:      CU(p->bend_range, 24); break;
    case VF_TRANSPOSE: CS(p->transpose, -24, 24); break;
    }
    #undef CU
    #undef CS
}

static void handle_wavetable(uint32_t pressed)
{
    if (pressed & BTN_LEFT)  { if (g_cur_wave_idx > 0)  g_cur_wave_idx--; }
    if (pressed & BTN_RIGHT) { if (g_cur_wave_idx < 63) g_cur_wave_idx++; }
    if (pressed & BTN_UP)    { if (g_cur_wavetable > 1)  g_cur_wavetable--; }
    if (pressed & BTN_DOWN)  { if (g_cur_wavetable < 32) g_cur_wavetable++; }
    if (pressed & BTN_Z)     sysex_request_wavetable(g_cur_wavetable);
}

static void handle_sample(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_sample_field = (g_cur_sample_field + SF_COUNT - 1) % SF_COUNT;
    if (pressed & BTN_DOWN) g_cur_sample_field = (g_cur_sample_field + 1) % SF_COUNT;
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    ppg_sample_t *s = &g_samples[g_cur_sample];
    switch (g_cur_sample_field) {
    case SF_SLOT:   { int v = g_cur_sample + dir; if (v<0)v=0; if (v>=NUM_SAMPLES)v=NUM_SAMPLES-1; g_cur_sample = v; } break;
    case SF_LEN:    { int v = (int)s->length_samples + dir * 256; if (v<0)v=0; s->length_samples = (uint32_t)v; } break;
    case SF_RATE:   { int v = s->sample_rate + dir; if (v<80)v=80; if (v>480)v=480; s->sample_rate = (uint16_t)v; } break;
    case SF_LOOP:   s->loop_enabled ^= 1; break;
    case SF_LSTART: { int v = (int)s->loop_start + dir * 256; if (v<0)v=0; s->loop_start = (uint32_t)v; } break;
    case SF_LEND:   { int v = (int)s->loop_end + dir * 256; if (v<0)v=0; s->loop_end = (uint32_t)v; } break;
    }
}

static void handle_sequence(uint32_t pressed)
{
    if (pressed & BTN_UP)    { if (g_cur_seq_track > 0)  g_cur_seq_track--; }
    if (pressed & BTN_DOWN)  { if (g_cur_seq_track < 7)  g_cur_seq_track++; }
    if (pressed & BTN_LEFT)  { if (g_cur_seq_step > 0)   g_cur_seq_step--; }
    if (pressed & BTN_RIGHT) { if (g_cur_seq_step < 15)  g_cur_seq_step++; }
    if (pressed & BTN_A) {
        /* Toggle step velocity between 0 and 100 */
        uint8_t *v = &g_sequences[g_cur_seq].track[g_cur_seq_track].velocity[g_cur_seq_step];
        *v = (*v == 0) ? 100 : 0;
    }
    if (pressed & BTN_C_UP)   { if (g_cur_seq > 0)              g_cur_seq--; }
    if (pressed & BTN_C_DOWN) { if (g_cur_seq < NUM_SEQUENCES-1) g_cur_seq++; }
}

static void handle_file(uint32_t pressed)
{
    if (pressed & BTN_UP)   { if (g_cur_file > 0)                       g_cur_file--; }
    if (pressed & BTN_DOWN) { if (g_cur_file < g_file_count - 1)        g_cur_file++; }
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

    init_model();

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

        switch (active_page) {
        case PAGE_SYSTEM:    handle_system(pressed);    break;
        case PAGE_VOICE:     handle_voice(pressed);     break;
        case PAGE_WAVETABLE: handle_wavetable(pressed); break;
        case PAGE_SAMPLE:    handle_sample(pressed);    break;
        case PAGE_SEQUENCE:  handle_sequence(pressed);  break;
        case PAGE_FILE:      handle_file(pressed);      break;
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
