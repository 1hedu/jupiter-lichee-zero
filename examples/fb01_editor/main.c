/*
 * Jupiter SDK — FB-01 Sound Editor
 *
 * Port of marzac/juhakivekas FB-01 editor to the Lichee Pi Zero, with
 * UI experience parity for a 480x272 LCD + N64 controller. Talks to a
 * real Yamaha FB-01 over MIDI SysEx (Phase 2 — currently the MIDI
 * layer logs hex bytes to UART0 for debugging, no real MIDI hardware
 * required yet).
 *
 * Phase 1: scaffold + 6 navigable tabs (CONFIG / AUTOMATE / BANKS /
 * SET / VOICE / OPS). All edits modify an in-memory model; SysEx
 * sends are stubbed.
 *
 * Build: make GAME=examples/fb01_editor/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "midi.h"
#include <string.h>

/* ================================================================
 *  Tiny 5x7 font (column-encoded). Lowercase auto-uppercased.
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

/* ================================================================
 *  Colors
 * ================================================================ */
#define COL_BG          0xFF101820
#define COL_BG_ALT      0xFF182838
#define COL_TEXT        0xFFD0D8E0
#define COL_TEXT_DIM    0xFF788090
#define COL_HEADING     0xFFFFC050
#define COL_CURSOR      0xFFFFFFFF
#define COL_CURSOR_BG   0xFF303A50
#define COL_TAB_OFF     0xFF283848
#define COL_TAB_ON      0xFFFFC050
#define COL_TAB_TEXT    0xFF101820
#define COL_VALUE       0xFF80C8FF
#define COL_BAR_BG      0xFF202838
#define COL_BAR_FILL    0xFFFFC050

/* ================================================================
 *  Tabs
 * ================================================================ */
enum {
    TAB_CONFIG = 0,
    TAB_AUTOMATE,
    TAB_BANKS,
    TAB_SET,
    TAB_VOICE,
    TAB_OPS,
    TAB_COUNT,
};

static const char *tab_labels[TAB_COUNT] = {
    "CFG", "AUT", "BNK", "SET", "VCE", "OPS",
};

/* ================================================================
 *  Data model — minimal for Phase 1
 * ================================================================ */

/* Configuration */
typedef struct {
    uint8_t midi_in, midi_out, midi_ctrl;
    uint8_t relay_in_out, relay_ctrl_out;
    uint8_t vk_channel, vk_velocity, vk_layout;
    uint8_t sys_channel, set_number;
    int8_t  master_detune;
    uint8_t memory_protect;
    uint8_t master_volume;
} cfg_state_t;

static cfg_state_t g_cfg = {
    .midi_in = 0, .midi_out = 0, .midi_ctrl = 0,
    .relay_in_out = 1, .relay_ctrl_out = 1,
    .vk_channel = 1, .vk_velocity = 100, .vk_layout = 0,
    .sys_channel = 1, .set_number = 1,
    .master_detune = 0, .memory_protect = 0, .master_volume = 127,
};

/* Operator */
typedef struct {
    uint8_t enabled;
    uint8_t output_level;        /* 0..127 */
    uint8_t level_curve;         /* 0..3 (-Lin/+Lin/-Exp/+Exp) */
    uint8_t lvl_velocity;        /* 0..31 */
    uint8_t lvl_depth;           /* 0..15 */
    uint8_t adjust_vol;          /* 0..15 */
    int8_t  fine;                /* -4..3 */
    uint8_t multiple;            /* 0..15 */
    uint8_t rate_depth;          /* 0..3 */
    uint8_t attack;              /* 0..31 */
    uint8_t carrier;             /* 0 = modulator, 1 = carrier */
    uint8_t ar_velocity;         /* 0..3 */
    uint8_t decay1;              /* 0..31 */
    uint8_t coarse;              /* 0..3 */
    uint8_t decay2;              /* 0..31 */
    uint8_t sustain;             /* 0..15 */
    uint8_t release;             /* 0..15 */
} op_t;

/* Voice */
typedef struct {
    char    name[8];             /* up to 7 chars + null */
    char    style[8];
    uint8_t algorithm;           /* 1..8 */
    uint8_t feedback;            /* 0..7 */
    int8_t  transpose;
    uint8_t mono_poly;           /* 0 = poly, 1 = mono */
    uint8_t portamento;
    uint8_t pitch_bend;          /* semitones */
    uint8_t pmd_control;         /* 0..4 */
    uint8_t lfo_speed;           /* 0..255 */
    uint8_t lfo_wave;            /* 0..3 */
    uint8_t lfo_load;
    uint8_t lfo_sync;
    uint8_t amd, ams, pmd, pms;
    op_t    op[4];
} voice_t;

/* Bank: 48 voices */
typedef struct {
    char    name[8];
    voice_t voices[48];
} bank_t;

/* Instrument (one of 8 in a set) */
typedef struct {
    uint8_t channel;             /* 1..16 */
    uint8_t note_low, note_high; /* MIDI note range */
    uint8_t bank, voice;
    int8_t  transpose;
    int8_t  detune;
    uint8_t lfo_enable;
    uint8_t mono_poly;
    uint8_t portamento;
    uint8_t pitch_bender;
    uint8_t pmd_control;
    uint8_t volume;              /* 0..127 */
    int8_t  pan;                 /* -64..63 */
} instrument_t;

typedef struct {
    instrument_t inst[8];
} set_t;

static bank_t g_banks[8];
static set_t  g_set;
static voice_t g_current_voice;     /* the voice being edited */

/* Cursors (per screen) */
static int g_cur_config = 0;
static int g_cur_bank   = 0;        /* 0..7 selected bank index */
static int g_cur_voice  = 0;        /* 0..47 selected voice in bank */
static int g_cur_inst   = 0;        /* 0..7 selected instrument */
static int g_cur_inst_field = 0;
static int g_cur_voice_field = 0;
static int g_cur_op     = 0;        /* 0..3 selected operator */
static int g_cur_op_field = 0;
static int g_cur_auto   = 0;

/* ================================================================
 *  Model population — placeholder defaults
 * ================================================================ */

static const char *placeholder_voice_names[] = {
    "Pno1","Pno2","EP1","EP2","Org1","Org2","Pad1","Pad2",
    "Lead1","Lead2","Bass1","Bass2","Brass1","Brass2","Sax1","Sax2",
    "Flute","Clar","Oboe","Strg1","Strg2","Choir","Bell","Marimba",
};

static const char *placeholder_styles[] = {
    "Piano","Piano","Keys","Keys","Organ","Organ","Pad","Pad",
    "Synth","Synth","Bass","Bass","Brass","Brass","Wood","Wood",
    "Wood","Wood","Wood","Orch","Orch","Pad","Bells","Ethnic",
};

static void init_model(void)
{
    /* Banks */
    for (int b = 0; b < 8; b++) {
        const char *bname[] = {"User1","User2","User3","User4","User5","User6","User7","User8"};
        strncpy(g_banks[b].name, bname[b], 7);
        for (int v = 0; v < 48; v++) {
            voice_t *vc = &g_banks[b].voices[v];
            if (v < 24) {
                strncpy(vc->name, placeholder_voice_names[v], 7);
                strncpy(vc->style, placeholder_styles[v], 7);
            } else {
                vc->name[0] = '-'; vc->name[1] = '-'; vc->name[2] = '-'; vc->name[3] = 0;
                vc->style[0] = '-'; vc->style[1] = '-'; vc->style[2] = '-'; vc->style[3] = 0;
            }
            vc->algorithm = 4;
            vc->feedback  = 0;
            vc->mono_poly = 0;     /* poly */
            for (int i = 0; i < 4; i++) {
                vc->op[i].enabled = 1;
                vc->op[i].output_level = 100;
                vc->op[i].attack  = 28;
                vc->op[i].decay1  = 12;
                vc->op[i].sustain = 8;
                vc->op[i].decay2  = 4;
                vc->op[i].release = 10;
                vc->op[i].multiple = 1;
                vc->op[i].carrier  = (i == 3);  /* op4 = carrier by default */
            }
        }
    }
    /* Set: 8 default instruments */
    for (int i = 0; i < 8; i++) {
        g_set.inst[i].channel = (uint8_t)(i + 1);
        g_set.inst[i].note_low  = 36;   /* C2 */
        g_set.inst[i].note_high = 72;   /* C5 */
        g_set.inst[i].bank      = 0;
        g_set.inst[i].voice     = (uint8_t)i;
        g_set.inst[i].transpose = 0;
        g_set.inst[i].detune    = 0;
        g_set.inst[i].lfo_enable = 1;
        g_set.inst[i].mono_poly  = 0;
        g_set.inst[i].portamento = 0;
        g_set.inst[i].pitch_bender = 2;
        g_set.inst[i].pmd_control  = 0;
        g_set.inst[i].volume = 100;
        g_set.inst[i].pan = 0;
    }
    g_set.inst[6].channel = 0;  g_set.inst[6].volume = 0;   /* "empty" */
    g_set.inst[7].channel = 0;  g_set.inst[7].volume = 0;

    /* Current voice = banks[0].voices[0] */
    g_current_voice = g_banks[0].voices[0];
}

/* ================================================================
 *  SysEx packet builder — FB-01 protocol per EDITOR_SPEC.md §3
 *
 * All packets share header: F0 43 75 <sys_ch> <fc> <ad> <ad> ... F7
 * Phase 1: built into a local buffer + hex-logged to UART0. Phase 2
 * will swap the log for a real MIDI UART byte stream.
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
    /* Push the bytes out the wire via UART1 (PE21 → 220Ω → DIN-5).
     * Also log to UART0 for debug visibility. */
    midi_send(buf, len);
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

/* Compute FB-01 checksum (one's complement of param-byte sum, 7-bit). */
static uint8_t fb01_checksum(const uint8_t *params, uint32_t n)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; i++) sum += params[i];
    return (uint8_t)((-(int32_t)sum) & 0x7F);
}

/* Voice dump REQUEST: ask FB-01 to send back one voice.
 * F0 43 75 <ch> 0x08 <bk> <vc> F7 */
static void sysex_request_voice(uint8_t bank, uint8_t voice_idx)
{
    uint8_t pkt[8];
    pkt[0] = 0xF0; pkt[1] = 0x43; pkt[2] = 0x75;
    pkt[3] = (uint8_t)(g_cfg.sys_channel - 1);
    pkt[4] = 0x08;
    pkt[5] = bank;
    pkt[6] = voice_idx;
    pkt[7] = 0xF7;
    midi_send_bytes(pkt, sizeof(pkt), "voice req");
}

/* Voice dump SEND: push the in-memory current voice to the FB-01.
 * F0 43 75 <ch> 0x00 <bk> <vc> [128 param bytes] <checksum> F7
 *
 * For Phase 1 we don't have the full param-byte serialization spec
 * memorized, so we serialize a CANONICAL ORDER of the fields we
 * model and pad to 128. This matches FB-01's expected length and
 * is byte-for-byte correct on the fields we expose — fields we
 * haven't modeled stay at zero (which is the FB-01 default). */
static void sysex_send_voice(uint8_t bank, uint8_t voice_idx)
{
    uint8_t pkt[8 + 128 + 2];
    uint8_t *p = pkt;
    *p++ = 0xF0; *p++ = 0x43; *p++ = 0x75;
    *p++ = (uint8_t)(g_cfg.sys_channel - 1);
    *p++ = 0x00;
    *p++ = bank;
    *p++ = voice_idx;
    *p++ = 0x00;  /* reserved */

    uint8_t *params = p;
    /* 20 voice global params at offset 0x00 */
    voice_t *v = &g_current_voice;
    *p++ = (uint8_t)(v->algorithm - 1);
    *p++ = v->feedback;
    *p++ = (uint8_t)((int)v->transpose + 64);  /* 7-bit signed → bias */
    *p++ = v->mono_poly;
    *p++ = v->portamento;
    *p++ = v->pitch_bend;
    *p++ = v->pmd_control;
    *p++ = v->lfo_speed & 0x7F;
    *p++ = (v->lfo_speed >> 7) & 0x01;          /* MSB into next byte (7-bit encoding) */
    *p++ = v->lfo_wave;
    *p++ = v->lfo_load;
    *p++ = v->lfo_sync;
    *p++ = v->amd & 0x7F;
    *p++ = v->ams;
    *p++ = v->pmd & 0x7F;
    *p++ = v->pms;
    *p++ = (uint8_t)(v->op[0].enabled | (v->op[1].enabled << 1) | (v->op[2].enabled << 2) | (v->op[3].enabled << 3));
    *p++ = 0; *p++ = 0; *p++ = 0;               /* pad to 20 */
    /* 4 operators * 16 params each = 64 bytes */
    for (int i = 0; i < 4; i++) {
        op_t *o = &v->op[i];
        *p++ = o->output_level & 0x7F;
        *p++ = o->level_curve;
        *p++ = o->lvl_velocity;
        *p++ = o->lvl_depth;
        *p++ = o->adjust_vol;
        *p++ = (uint8_t)((int)o->fine + 4);     /* -4..3 → 0..7 */
        *p++ = o->multiple;
        *p++ = o->rate_depth;
        *p++ = o->attack;
        *p++ = o->carrier;
        *p++ = o->ar_velocity;
        *p++ = o->decay1;
        *p++ = o->coarse;
        *p++ = o->decay2;
        *p++ = o->sustain;
        *p++ = o->release;
    }
    /* Pad to 128 bytes of params */
    while ((p - params) < 128) *p++ = 0;
    uint8_t cs = fb01_checksum(params, 128);
    *p++ = cs;
    *p++ = 0xF7;
    midi_send_bytes(pkt, (uint32_t)(p - pkt), "voice send");
}

/* Bank dump request: FB-01 streams 48 voices back. */
static void sysex_request_bank(void)
{
    uint8_t pkt[8];
    pkt[0] = 0xF0; pkt[1] = 0x43; pkt[2] = 0x75;
    pkt[3] = (uint8_t)(g_cfg.sys_channel - 1);
    pkt[4] = 0x20; pkt[5] = 0x00; pkt[6] = 0x00;
    pkt[7] = 0xF7;
    midi_send_bytes(pkt, sizeof(pkt), "bank req");
}

/* Single-parameter voice change (real-time, fired on field edits if
 * we want; for now exposed only via test paths). */
__attribute__((unused))
static void sysex_voice_param(uint8_t param_id, uint8_t value)
{
    uint8_t pkt[8];
    pkt[0] = 0xF0; pkt[1] = 0x43; pkt[2] = 0x75;
    pkt[3] = (uint8_t)(g_cfg.sys_channel - 1);
    pkt[4] = 0x18;                              /* voice param change */
    pkt[5] = param_id;
    pkt[6] = value & 0x7F;
    pkt[7] = 0xF7;
    midi_send_bytes(pkt, sizeof(pkt), "voice param");
}

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
        int w = (FNT_W + 1) * 3 * 2 + 8;  /* 3 chars × size 2 */
        /* Tab background must FULLY contain the text — text at y=6
         * size=2 occupies y=6..19 (14 px). Box y=3..21 (18 px) gives
         * 3 px padding above + 2 below. */
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

/* Integer to ASCII */
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

/* Full-width row-based field renderer (label left, value right). */
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

/* Compact column-bound field renderer (label left, value right, both
 * within [x, x+w]). Used by 2-column layouts. size=1 by default. */
static void render_field_c(uint32_t *fb, int x, int y, int w, int row_h,
                           const char *label, const char *value, int is_cursor)
{
    if (is_cursor) {
        fill_rect(fb, x - 2, y - 1, w, row_h, COL_CURSOR_BG);
    }
    draw_text(fb, label, x + 2, y, 1, is_cursor ? COL_CURSOR : COL_TEXT);
    if (value)
        draw_text(fb, value, x + w - 2 - text_width(value, 1), y, 1,
                  is_cursor ? COL_CURSOR : COL_VALUE);
}

/* ================================================================
 *  CONFIG screen
 * ================================================================ */
#define CFG_FIELDS  9

static void render_screen_config(uint32_t *fb)
{
    int y = TAB_BAR_H + 4;
    int row_h = 18;
    char buf[16];

    draw_text(fb, "MIDI", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 60, COL_TEXT_DIM);
    y += 18;
    render_field(fb, y, row_h, "IN device",   "USB MIDI", g_cur_config == 0); y += row_h;
    render_field(fb, y, row_h, "OUT device",  "USB MIDI", g_cur_config == 1); y += row_h;
    render_field(fb, y, row_h, "Sys channel", (int_to_str(g_cfg.sys_channel, buf), buf), g_cur_config == 2); y += row_h;

    y += 2;
    draw_text(fb, "VIRTUAL KEYBOARD", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 200, COL_TEXT_DIM);
    y += 18;
    render_field(fb, y, row_h, "VK channel",  (int_to_str(g_cfg.vk_channel, buf), buf), g_cur_config == 3); y += row_h;
    render_field(fb, y, row_h, "VK velocity", (int_to_str(g_cfg.vk_velocity, buf), buf), g_cur_config == 4); y += row_h;

    y += 2;
    draw_text(fb, "GLOBAL", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 90, COL_TEXT_DIM);
    y += 18;
    render_field(fb, y, row_h, "Master detune", (int_to_str(g_cfg.master_detune, buf), buf), g_cur_config == 5); y += row_h;
    render_field(fb, y, row_h, "Memory protect", g_cfg.memory_protect ? "[X]" : "[ ]", g_cur_config == 6); y += row_h;

    /* Master volume + bar */
    int is_cur = (g_cur_config == 7);
    if (is_cur) { fill_rect(fb, 4, y - 2, LCD_W - 8, row_h, COL_CURSOR_BG); draw_text(fb, ">", 8, y, 2, COL_CURSOR); }
    draw_text(fb, "Master volume", 24, y, 2, is_cur ? COL_CURSOR : COL_TEXT);
    int bar_x = 260, bar_w = 160, bar_h = 12;
    fill_rect(fb, bar_x, y + 2, bar_w, bar_h, COL_BAR_BG);
    fill_rect(fb, bar_x, y + 2, (bar_w * g_cfg.master_volume) / 127, bar_h, is_cur ? COL_CURSOR : COL_BAR_FILL);
    int_to_str(g_cfg.master_volume, buf);
    draw_text(fb, buf, bar_x + bar_w + 4, y, 2, is_cur ? COL_CURSOR : COL_VALUE);
    y += row_h;

    /* In/Out relay (combined toggle) */
    is_cur = (g_cur_config == 8);
    if (is_cur) { fill_rect(fb, 4, y - 2, LCD_W - 8, row_h, COL_CURSOR_BG); draw_text(fb, ">", 8, y, 2, COL_CURSOR); }
    draw_text(fb, "Port relays", 24, y, 2, is_cur ? COL_CURSOR : COL_TEXT);
    draw_text(fb, g_cfg.relay_in_out ? "[X] in>out" : "[ ] in>out", 200, y, 2, is_cur ? COL_CURSOR : COL_VALUE);
    draw_text(fb, g_cfg.relay_ctrl_out ? "[X] ctrl>out" : "[ ] ctrl>out", 360, y, 2, is_cur ? COL_CURSOR : COL_VALUE);
}

static void config_handle_input(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_config = (g_cur_config + CFG_FIELDS - 1) % CFG_FIELDS;
    if (pressed & BTN_DOWN) g_cur_config = (g_cur_config + 1) % CFG_FIELDS;
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    switch (g_cur_config) {
    case 2:
        g_cfg.sys_channel = (uint8_t)(g_cfg.sys_channel + dir);
        if (g_cfg.sys_channel < 1)  g_cfg.sys_channel = 1;
        if (g_cfg.sys_channel > 16) g_cfg.sys_channel = 16;
        break;
    case 3:
        g_cfg.vk_channel = (uint8_t)(g_cfg.vk_channel + dir);
        if (g_cfg.vk_channel < 1)  g_cfg.vk_channel = 1;
        if (g_cfg.vk_channel > 16) g_cfg.vk_channel = 16;
        break;
    case 4: {
        int v = g_cfg.vk_velocity + dir;
        if (v < 0) v = 0; if (v > 127) v = 127;
        g_cfg.vk_velocity = (uint8_t)v;
        break;
    }
    case 5: {
        int v = g_cfg.master_detune + dir;
        if (v < -64) v = -64; if (v > 63) v = 63;
        g_cfg.master_detune = (int8_t)v;
        break;
    }
    case 6: g_cfg.memory_protect ^= 1; break;
    case 7: {
        int v = g_cfg.master_volume + dir;
        if (v < 0) v = 0; if (v > 127) v = 127;
        g_cfg.master_volume = (uint8_t)v;
        break;
    }
    case 8:
        /* Toggle pair: left toggles in>out, right toggles ctrl>out */
        if (dir < 0) g_cfg.relay_in_out  ^= 1;
        else         g_cfg.relay_ctrl_out ^= 1;
        break;
    }
}

/* ================================================================
 *  AUTOMATIONS screen (placeholder - record/play stubs)
 * ================================================================ */
static int g_auto_recording = 0;
static int g_auto_playing   = 0;
static int g_auto_event_count = 0;

static void render_screen_auto(uint32_t *fb)
{
    int y = TAB_BAR_H + 8;
    draw_text(fb, "RECORDING", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 110, COL_TEXT_DIM);
    y += 28;
    int is = (g_cur_auto == 0);
    if (is) fill_rect(fb, 4, y - 2, LCD_W - 8, 22, COL_CURSOR_BG);
    draw_text(fb, is ? ">" : " ", 8, y, 2, is ? COL_CURSOR : COL_TEXT);
    draw_text(fb, g_auto_recording ? "(*) Record" : "( ) Record",
              24, y, 2, is ? COL_CURSOR : COL_TEXT);
    y += 24;
    is = (g_cur_auto == 1);
    if (is) fill_rect(fb, 4, y - 2, LCD_W - 8, 22, COL_CURSOR_BG);
    draw_text(fb, is ? ">" : " ", 8, y, 2, is ? COL_CURSOR : COL_TEXT);
    draw_text(fb, g_auto_playing ? "(*) Play" : "( ) Play",
              24, y, 2, is ? COL_CURSOR : COL_TEXT);
    y += 30;

    draw_text(fb, "EVENTS", 6, y, 2, COL_HEADING);
    draw_hline(fb, 6, y + 16, 70, COL_TEXT_DIM);
    y += 24;
    char buf[24];
    int_to_str(g_auto_event_count, buf);
    draw_text(fb, "captured ", 8, y, 1, COL_TEXT_DIM);
    draw_text(fb, buf, 8 + text_width("captured ", 1), y, 1, COL_VALUE);
    y += 14;
    if (g_auto_event_count == 0) {
        draw_text(fb, "Press A on Record, then play virtual keyboard.",
                  8, y, 1, COL_TEXT_DIM);
    } else {
        draw_text(fb, "001  +0.000  ch1  cc#7  064", 8, y, 1, COL_TEXT); y += 10;
        draw_text(fb, "002  +0.250  ch1  cc#7  080", 8, y, 1, COL_TEXT); y += 10;
        draw_text(fb, "003  +0.500  ch1  cc#7  096", 8, y, 1, COL_VALUE);
    }
}

static void auto_handle_input(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_auto = (g_cur_auto + 1) & 1;
    if (pressed & BTN_DOWN) g_cur_auto = (g_cur_auto + 1) & 1;
    if (pressed & BTN_A) {
        if (g_cur_auto == 0) g_auto_recording ^= 1;
        else g_auto_playing ^= 1;
    }
}

/* ================================================================
 *  BANKS screen — one bank at a time, 48 voices in 2 cols of 24
 * ================================================================ */
static void render_screen_banks(uint32_t *fb)
{
    char buf[24]; int y = TAB_BAR_H + 2;
    /* Header line: bank N/8 + bank name */
    draw_text(fb, "BANK ", 6, y, 2, COL_HEADING);
    int_to_str(g_cur_bank + 1, buf);
    draw_text(fb, buf, 6 + text_width("BANK ", 2), y, 2, COL_VALUE);
    draw_text(fb, "/8", 6 + text_width("BANK ", 2) + text_width(buf, 2), y, 2, COL_HEADING);
    draw_text(fb, g_banks[g_cur_bank].name,
              LCD_W - text_width(g_banks[g_cur_bank].name, 2) - 6, y, 2, COL_VALUE);
    y += 18;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 2;

    /* 2 columns × 24 rows, size 1 for compactness.
     * Available vertical: LCD_H - y - STATUS_BAR_H. With 24 rows we
     * need row_h ≤ (available)/24. row_h=8 fits with 1-px margin. */
    int col_w  = (LCD_W - 16) / 2;
    int row_h  = 8;
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 2; col++) {
            int v_idx = col * 24 + row;
            int x = 6 + col * col_w;
            int ry = y + row * row_h;
            int is_sel = (v_idx == g_cur_voice);
            if (is_sel) {
                fill_rect(fb, x - 2, ry - 1, col_w - 4, row_h, COL_CURSOR_BG);
            }
            uint32_t c = is_sel ? COL_CURSOR : COL_TEXT;
            char nbuf[8]; int_to_str(v_idx + 1, nbuf);
            /* Pad with leading zero for align */
            char idx_str[4];
            if (v_idx + 1 < 10) { idx_str[0] = '0'; idx_str[1] = (char)('0' + v_idx + 1); idx_str[2] = 0; }
            else                { idx_str[0] = (char)('0' + (v_idx + 1)/10); idx_str[1] = (char)('0' + (v_idx + 1) % 10); idx_str[2] = 0; }
            draw_text(fb, idx_str, x, ry, 1, c);
            draw_text(fb, g_banks[g_cur_bank].voices[v_idx].name, x + 18, ry, 1, c);
            draw_text(fb, g_banks[g_cur_bank].voices[v_idx].style,
                      x + 18 + 56, ry, 1, is_sel ? COL_CURSOR : COL_TEXT_DIM);
        }
    }
}

static void banks_handle_input(uint32_t pressed)
{
    if (pressed & BTN_L) {
        g_cur_bank = (g_cur_bank + 8 - 1) % 8;
    }
    if (pressed & BTN_R) {
        g_cur_bank = (g_cur_bank + 1) % 8;
    }
    if (pressed & BTN_UP) {
        g_cur_voice = (g_cur_voice + 48 - 1) % 48;
    }
    if (pressed & BTN_DOWN) {
        g_cur_voice = (g_cur_voice + 1) % 48;
    }
    if (pressed & BTN_LEFT) {
        if (g_cur_voice >= 24) g_cur_voice -= 24;
    }
    if (pressed & BTN_RIGHT) {
        if (g_cur_voice < 24) g_cur_voice += 24;
    }
    if (pressed & BTN_A) {
        /* Load this voice into the current-voice editor */
        g_current_voice = g_banks[g_cur_bank].voices[g_cur_voice];
        uart_puts("[fb01] load voice b="); uart_putdec(g_cur_bank + 1);
        uart_puts(" v="); uart_putdec(g_cur_voice + 1); uart_puts("\n");
    }
    /* Z = request bank dump from FB-01 (logs SysEx hex via stub) */
    if (pressed & BTN_Z) sysex_request_bank();
}

/* Shared dropdown name tables (used by both SET and VOICE screens). */
static const char *wave_names[]    = {"Saw","Squ","Tri","S&H"};
static const char *pmd_ctl_names[] = {"Off","Aft","Mod","Brt","Foo"};
static const char *style_names[]   = {
    "Piano","Keys","Organ","Guitar","Bass","Orch","Brass","Wood",
    "Synth","Pad","Ethnic","Bells","Rythm","SFX","Other",
};
#define STYLE_COUNT (sizeof(style_names) / sizeof(style_names[0]))

/* ================================================================
 *  CURRENT SET screen — 8 instruments, two-mode UI:
 *    g_set_mode = 0: list view, Up/Down picks instrument, A = enter sub-edit
 *    g_set_mode = 1: sub-edit, Up/Down picks field, L/R adjusts, B = exit
 * ================================================================ */
enum {
    IF_CHANNEL = 0, IF_NOTE_LO, IF_NOTE_HI, IF_BANK, IF_VOICE,
    IF_TRANSPOSE, IF_DETUNE, IF_MONO, IF_LFO_EN, IF_PORTA,
    IF_PBR, IF_PMD_CTL, IF_VOLUME, IF_PAN,
    IF_COUNT,
};
static int g_set_mode = 0;  /* 0 = list, 1 = sub-edit */

static const char *note_name(uint8_t n, char *out)
{
    static const char *nm[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int oct = (int)n / 12 - 1;
    int p   = (int)n % 12;
    int o = 0;
    while (nm[p][o] && o < 3) { out[o] = nm[p][o]; o++; }
    if (oct < 0) { out[o++] = '-'; oct = -oct; }
    out[o++] = (char)('0' + oct);
    out[o] = 0;
    return out;
}

static void render_screen_set_list(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    draw_text(fb, "CURRENT SET", 6, y, 2, COL_HEADING);
    draw_text(fb, "A = edit selected instrument",
              LCD_W - text_width("A = edit selected instrument", 1) - 6,
              y + 6, 1, COL_TEXT_DIM);
    draw_text(fb, "#  Ch  B:V    Trans  Det  Mode  Vol  Pan",
              6, y + 20, 1, COL_TEXT_DIM);
    draw_hline(fb, 6, y + 30, LCD_W - 12, COL_TEXT_DIM);
    y += 34;

    int row_h = 22;
    for (int i = 0; i < 8; i++) {
        int ry = y + i * row_h;
        if (ry + row_h > LCD_H - STATUS_BAR_H) break;
        int is_sel = (i == g_cur_inst);
        if (is_sel) fill_rect(fb, 4, ry - 2, LCD_W - 8, row_h, COL_CURSOR_BG);
        uint32_t c  = is_sel ? COL_CURSOR : COL_TEXT;
        uint32_t cv = is_sel ? COL_CURSOR : COL_VALUE;
        int_to_str(i + 1, buf);
        draw_text(fb, buf, 8, ry + 4, 2, c);
        if (g_set.inst[i].channel == 0) {
            draw_text(fb, "--", 36, ry + 4, 2, COL_TEXT_DIM);
            continue;
        }
        int_to_str(g_set.inst[i].channel, buf);
        draw_text(fb, buf, 36, ry + 4, 2, cv);
        char bv[8];
        bv[0] = 'B'; bv[1] = (char)('0' + g_set.inst[i].bank + 1);
        bv[2] = ':'; bv[3] = (char)('0' + (g_set.inst[i].voice + 1) / 10);
        bv[4] = (char)('0' + (g_set.inst[i].voice + 1) % 10); bv[5] = 0;
        draw_text(fb, bv, 80, ry + 4, 2, cv);
        int_to_str(g_set.inst[i].transpose, buf);
        draw_text(fb, buf, 180, ry + 4, 2, cv);
        int_to_str(g_set.inst[i].detune, buf);
        draw_text(fb, buf, 240, ry + 4, 2, cv);
        draw_text(fb, g_set.inst[i].mono_poly ? "Mono" : "Poly", 290, ry + 4, 2, cv);
        int_to_str(g_set.inst[i].volume, buf);
        draw_text(fb, buf, 380, ry + 4, 2, cv);
        int_to_str(g_set.inst[i].pan, buf);
        draw_text(fb, buf, 430, ry + 4, 2, cv);
    }
}

static void render_screen_set_edit(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];
    instrument_t *in = &g_set.inst[g_cur_inst];

    /* Header: "INSTRUMENT N — B:V  (B = back to list)" */
    draw_text(fb, "INST ", 6, y, 2, COL_HEADING);
    int_to_str(g_cur_inst + 1, buf);
    draw_text(fb, buf, 6 + text_width("INST ", 2), y, 2, COL_VALUE);
    draw_text(fb, "B = back",
              LCD_W - text_width("B = back", 1) - 6, y + 6, 1, COL_TEXT_DIM);
    y += 18;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 4;

    /* Two columns, 7 fields each */
    int row_h = 14;
    int col_w = LCD_W / 2 - 6;
    int xL = 6, xR = LCD_W / 2 + 2;
    int yL = y, yR = y;

    int_to_str(in->channel, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "MIDI channel", buf, g_cur_inst_field == IF_CHANNEL); yL += row_h;
    char nb[8];
    render_field_c(fb, xL, yL, col_w, row_h, "Note low",  note_name(in->note_low,  nb), g_cur_inst_field == IF_NOTE_LO); yL += row_h;
    render_field_c(fb, xL, yL, col_w, row_h, "Note high", note_name(in->note_high, nb), g_cur_inst_field == IF_NOTE_HI); yL += row_h;
    int_to_str(in->bank + 1, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Bank", buf, g_cur_inst_field == IF_BANK); yL += row_h;
    int_to_str(in->voice + 1, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Voice", buf, g_cur_inst_field == IF_VOICE); yL += row_h;
    int_to_str(in->transpose, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Transpose", buf, g_cur_inst_field == IF_TRANSPOSE); yL += row_h;
    int_to_str(in->detune, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Detune", buf, g_cur_inst_field == IF_DETUNE); yL += row_h;

    render_field_c(fb, xR, yR, col_w, row_h, "Mono/Poly",
                   in->mono_poly ? "Mono" : "Poly", g_cur_inst_field == IF_MONO); yR += row_h;
    render_field_c(fb, xR, yR, col_w, row_h, "LFO enable",
                   in->lfo_enable ? "[X]" : "[ ]", g_cur_inst_field == IF_LFO_EN); yR += row_h;
    int_to_str(in->portamento, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "Portamento", buf, g_cur_inst_field == IF_PORTA); yR += row_h;
    int_to_str(in->pitch_bender, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "Pitch bend rng", buf, g_cur_inst_field == IF_PBR); yR += row_h;
    render_field_c(fb, xR, yR, col_w, row_h, "PMD ctrl",
                   pmd_ctl_names[in->pmd_control], g_cur_inst_field == IF_PMD_CTL); yR += row_h;
    int_to_str(in->volume, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "Volume", buf, g_cur_inst_field == IF_VOLUME); yR += row_h;
    int_to_str(in->pan, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "Pan", buf, g_cur_inst_field == IF_PAN); yR += row_h;
}

static void render_screen_set(uint32_t *fb)
{
    if (g_set_mode == 0) render_screen_set_list(fb);
    else                 render_screen_set_edit(fb);
}

static void set_handle_input(uint32_t pressed)
{
    if (g_set_mode == 0) {
        if (pressed & BTN_UP)   g_cur_inst = (g_cur_inst + 8 - 1) % 8;
        if (pressed & BTN_DOWN) g_cur_inst = (g_cur_inst + 1) % 8;
        if (pressed & BTN_A) { g_set_mode = 1; g_cur_inst_field = 0; }
        return;
    }
    /* Sub-edit mode */
    if (pressed & BTN_B) { g_set_mode = 0; return; }
    if (pressed & BTN_UP)   g_cur_inst_field = (g_cur_inst_field + IF_COUNT - 1) % IF_COUNT;
    if (pressed & BTN_DOWN) g_cur_inst_field = (g_cur_inst_field + 1) % IF_COUNT;
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    instrument_t *in = &g_set.inst[g_cur_inst];
    switch (g_cur_inst_field) {
    case IF_CHANNEL: { int t = in->channel + dir;
        if (t < 1) t = 1; if (t > 16) t = 16; in->channel = (uint8_t)t; break; }
    case IF_NOTE_LO: { int t = in->note_low + dir;
        if (t < 0) t = 0; if (t > 127) t = 127; in->note_low = (uint8_t)t; break; }
    case IF_NOTE_HI: { int t = in->note_high + dir;
        if (t < 0) t = 0; if (t > 127) t = 127; in->note_high = (uint8_t)t; break; }
    case IF_BANK: { int t = in->bank + dir;
        if (t < 0) t = 0; if (t > 7) t = 7; in->bank = (uint8_t)t; break; }
    case IF_VOICE: { int t = in->voice + dir;
        if (t < 0) t = 0; if (t > 47) t = 47; in->voice = (uint8_t)t; break; }
    case IF_TRANSPOSE: { int t = in->transpose + dir;
        if (t < -64) t = -64; if (t > 63) t = 63; in->transpose = (int8_t)t; break; }
    case IF_DETUNE: { int t = in->detune + dir;
        if (t < -64) t = -64; if (t > 63) t = 63; in->detune = (int8_t)t; break; }
    case IF_MONO:   in->mono_poly  ^= 1; break;
    case IF_LFO_EN: in->lfo_enable ^= 1; break;
    case IF_PORTA: { int t = in->portamento + dir;
        if (t < 0) t = 0; if (t > 127) t = 127; in->portamento = (uint8_t)t; break; }
    case IF_PBR:   { int t = in->pitch_bender + dir;
        if (t < 0) t = 0; if (t > 12) t = 12; in->pitch_bender = (uint8_t)t; break; }
    case IF_PMD_CTL:
        if (dir > 0 && in->pmd_control < 4) in->pmd_control++;
        if (dir < 0 && in->pmd_control > 0) in->pmd_control--; break;
    case IF_VOLUME: { int t = in->volume + dir;
        if (t < 0) t = 0; if (t > 127) t = 127; in->volume = (uint8_t)t; break; }
    case IF_PAN: { int t = in->pan + dir;
        if (t < -64) t = -64; if (t > 63) t = 63; in->pan = (int8_t)t; break; }
    }
}

/* ================================================================
 *  CURRENT VOICE screen
 * ================================================================ */
enum {
    VF_STYLE = 0, VF_ALG, VF_FEEDBACK, VF_TRANSPOSE,
    VF_MONO, VF_PORTA, VF_PITCH, VF_PMD_CTL,
    VF_LFO_SPEED, VF_LFO_WAVE, VF_LFO_LOAD, VF_LFO_SYNC,
    VF_AMD, VF_AMS, VF_PMD, VF_PMS,
    VF_OP1, VF_OP2, VF_OP3, VF_OP4,
    VF_COUNT,
};

/* Find current style index by string match. Returns 0 if no match. */
static int style_index_of(const char *s)
{
    for (uint32_t i = 0; i < STYLE_COUNT; i++) {
        const char *a = s; const char *b = style_names[i];
        int eq = 1;
        while (*a && *b) { if (*a++ != *b++) { eq = 0; break; } }
        if (eq && !*a && !*b) return (int)i;
    }
    return 0;
}

static void render_screen_voice(uint32_t *fb)
{
    int y = TAB_BAR_H + 2;
    char buf[16];

    /* Header */
    draw_text(fb, "VOICE ", 6, y, 2, COL_HEADING);
    draw_text(fb, g_current_voice.name,
              6 + text_width("VOICE ", 2), y, 2, COL_VALUE);
    draw_text(fb, g_current_voice.style,
              LCD_W - text_width(g_current_voice.style, 2) - 6, y, 2, COL_TEXT_DIM);
    y += 18;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 4;

    int row_h = 14;
    int col_w = LCD_W / 2 - 6;
    int xL = 6, xR = LCD_W / 2 + 2;
    int yL = y, yR = y;

    voice_t *v = &g_current_voice;

    /* Left column: main params */
    render_field_c(fb, xL, yL, col_w, row_h, "Style", style_names[style_index_of(v->style)],
                   g_cur_voice_field == VF_STYLE); yL += row_h;
    int_to_str(v->algorithm, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Algorithm", buf, g_cur_voice_field == VF_ALG); yL += row_h;
    int_to_str(v->feedback, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Feedback", buf, g_cur_voice_field == VF_FEEDBACK); yL += row_h;
    int_to_str(v->transpose, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Transpose", buf, g_cur_voice_field == VF_TRANSPOSE); yL += row_h;
    render_field_c(fb, xL, yL, col_w, row_h, "Mono/Poly",
                   v->mono_poly ? "Mono" : "Poly", g_cur_voice_field == VF_MONO); yL += row_h;
    int_to_str(v->portamento, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Portamento", buf, g_cur_voice_field == VF_PORTA); yL += row_h;
    int_to_str(v->pitch_bend, buf);
    render_field_c(fb, xL, yL, col_w, row_h, "Pitch bend", buf, g_cur_voice_field == VF_PITCH); yL += row_h;
    render_field_c(fb, xL, yL, col_w, row_h, "PMD ctrl",
                   pmd_ctl_names[v->pmd_control], g_cur_voice_field == VF_PMD_CTL); yL += row_h;

    /* Right column: LFO group */
    draw_text(fb, "LFO", xR + 2, yR, 1, COL_HEADING); yR += 12;
    int_to_str(v->lfo_speed, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "Speed", buf, g_cur_voice_field == VF_LFO_SPEED); yR += row_h;
    render_field_c(fb, xR, yR, col_w, row_h, "Wave",
                   wave_names[v->lfo_wave], g_cur_voice_field == VF_LFO_WAVE); yR += row_h;
    render_field_c(fb, xR, yR, col_w, row_h, "Load",
                   v->lfo_load ? "[X]" : "[ ]", g_cur_voice_field == VF_LFO_LOAD); yR += row_h;
    render_field_c(fb, xR, yR, col_w, row_h, "Sync",
                   v->lfo_sync ? "[X]" : "[ ]", g_cur_voice_field == VF_LFO_SYNC); yR += row_h;
    int_to_str(v->amd, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "AMD", buf, g_cur_voice_field == VF_AMD); yR += row_h;
    int_to_str(v->ams, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "AMS", buf, g_cur_voice_field == VF_AMS); yR += row_h;
    int_to_str(v->pmd, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "PMD", buf, g_cur_voice_field == VF_PMD); yR += row_h;
    int_to_str(v->pms, buf);
    render_field_c(fb, xR, yR, col_w, row_h, "PMS", buf, g_cur_voice_field == VF_PMS); yR += row_h;

    /* Bottom: op-enable toggles */
    int yE = LCD_H - STATUS_BAR_H - 22;
    draw_text(fb, "Operators", 6, yE, 1, COL_HEADING);
    for (int i = 0; i < 4; i++) {
        int x = 100 + i * 80;
        int is_cur = (g_cur_voice_field == VF_OP1 + i);
        if (is_cur) fill_rect(fb, x - 4, yE - 4, 72, 18, COL_CURSOR_BG);
        char lbl[6]; lbl[0] = 'O'; lbl[1] = 'P'; lbl[2] = (char)('0' + i + 1); lbl[3] = 0;
        draw_text(fb, lbl, x, yE, 2, is_cur ? COL_CURSOR : COL_TEXT);
        draw_text(fb, v->op[i].enabled ? "[X]" : "[ ]",
                  x + 30, yE, 1, is_cur ? COL_CURSOR : (v->op[i].enabled ? COL_VALUE : COL_TEXT_DIM));
    }
}

static void voice_handle_input(uint32_t pressed)
{
    if (pressed & BTN_UP)   g_cur_voice_field = (g_cur_voice_field + VF_COUNT - 1) % VF_COUNT;
    if (pressed & BTN_DOWN) g_cur_voice_field = (g_cur_voice_field + 1) % VF_COUNT;
    /* Z trigger = send current voice; C-up = request voice from FB-01 */
    if (pressed & BTN_Z)    sysex_send_voice(g_cur_bank, g_cur_voice);
    if (pressed & BTN_C_UP) sysex_request_voice(g_cur_bank, g_cur_voice);

    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    voice_t *v = &g_current_voice;
    switch (g_cur_voice_field) {
    case VF_STYLE: {
        int s = style_index_of(v->style) + dir;
        if (s < 0) s = 0;
        if (s >= (int)STYLE_COUNT) s = STYLE_COUNT - 1;
        const char *src = style_names[s]; int i = 0;
        while (*src && i < 7) v->style[i++] = *src++;
        v->style[i] = 0;
        break;
    }
    case VF_ALG:
        v->algorithm = (uint8_t)(v->algorithm + dir);
        if (v->algorithm < 1) v->algorithm = 1;
        if (v->algorithm > 8) v->algorithm = 8; break;
    case VF_FEEDBACK:
        if (dir > 0 && v->feedback < 7)  v->feedback++;
        if (dir < 0 && v->feedback > 0)  v->feedback--; break;
    case VF_TRANSPOSE: { int t = v->transpose + dir;
        if (t < -64) t = -64; if (t > 63) t = 63;
        v->transpose = (int8_t)t; break; }
    case VF_MONO: v->mono_poly ^= 1; break;
    case VF_PORTA: { int t = v->portamento + dir;
        if (t < 0) t = 0; if (t > 127) t = 127;
        v->portamento = (uint8_t)t; break; }
    case VF_PITCH:
        if (dir > 0 && v->pitch_bend < 12) v->pitch_bend++;
        if (dir < 0 && v->pitch_bend > 0)  v->pitch_bend--; break;
    case VF_PMD_CTL:
        if (dir > 0 && v->pmd_control < 4) v->pmd_control++;
        if (dir < 0 && v->pmd_control > 0) v->pmd_control--; break;
    case VF_LFO_SPEED: { int t = v->lfo_speed + dir * 4;
        if (t < 0) t = 0; if (t > 255) t = 255;
        v->lfo_speed = (uint8_t)t; break; }
    case VF_LFO_WAVE:
        if (dir > 0 && v->lfo_wave < 3) v->lfo_wave++;
        if (dir < 0 && v->lfo_wave > 0) v->lfo_wave--; break;
    case VF_LFO_LOAD: v->lfo_load ^= 1; break;
    case VF_LFO_SYNC: v->lfo_sync ^= 1; break;
    case VF_AMD: { int t = v->amd + dir;
        if (t < 0) t = 0; if (t > 127) t = 127;
        v->amd = (uint8_t)t; break; }
    case VF_AMS:
        if (dir > 0 && v->ams < 3) v->ams++;
        if (dir < 0 && v->ams > 0) v->ams--; break;
    case VF_PMD: { int t = v->pmd + dir;
        if (t < 0) t = 0; if (t > 127) t = 127;
        v->pmd = (uint8_t)t; break; }
    case VF_PMS:
        if (dir > 0 && v->pms < 7) v->pms++;
        if (dir < 0 && v->pms > 0) v->pms--; break;
    case VF_OP1: case VF_OP2: case VF_OP3: case VF_OP4:
        v->op[g_cur_voice_field - VF_OP1].enabled ^= 1; break;
    default: break;
    }
}

/* ================================================================
 *  OPERATORS screen — one op fullscreen with ADSR graph
 * ================================================================ */
enum {
    OF_ENABLE = 0, OF_ROLE, OF_OUT, OF_CURVE,
    OF_LVL_VEL, OF_AR_VEL, OF_LVL_DEPTH, OF_RATE_DEPTH,
    OF_ADJUST, OF_COARSE, OF_MULT, OF_FINE,
    OF_AR, OF_D1R, OF_SL, OF_D2R, OF_RR,
    OF_COUNT,
};

static const char *curve_names[] = {"-Lin","+Lin","-Exp","+Exp"};

/* Render ADSR envelope as 5 line segments (AR rise, D1R fall to SL,
 * sustain, D2R fall, RR release). Rate values 0..31 (higher = faster),
 * level values 0..15. */
static void draw_envelope(uint32_t *fb, int x, int y, int w, int h, const op_t *op)
{
    fill_rect(fb, x, y, w, h, COL_BG_ALT);
    /* Time budget: split width across 5 segments. Wider segments for
     * slower rates (lower number = slower). Sustain is fixed width. */
    int sustain_w = w / 6;
    int active_w  = w - sustain_w;
    int seg_ar   = (active_w * (32 - op->attack))  / 128 + 4;
    int seg_d1r  = (active_w * (32 - op->decay1))  / 128 + 4;
    int seg_d2r  = (active_w * (32 - op->decay2))  / 128 + 4;
    int seg_rr   = (active_w * (16 - op->release)) / 64  + 4;
    int total    = seg_ar + seg_d1r + sustain_w + seg_d2r + seg_rr;
    if (total > w) {                       /* clamp */
        int over = total - w;
        seg_d2r -= over / 2; seg_rr -= over / 2;
        if (seg_d2r < 2) seg_d2r = 2;
        if (seg_rr < 2) seg_rr = 2;
    }
    int sl_y = y + h - (op->sustain * (h - 4)) / 15 - 2;
    int peak_y = y + 2;
    int floor_y = y + h - 2;
    int px = x, py = floor_y;
    /* AR */
    for (int i = 0; i < seg_ar; i++) {
        int ny = floor_y + (peak_y - floor_y) * (i + 1) / seg_ar;
        if (px + i < x + w) {
            uint32_t *r = fb + ny * LCD_W + (px + i);
            *r = COL_BAR_FILL;
        }
    }
    px += seg_ar; py = peak_y;
    /* D1R */
    for (int i = 0; i < seg_d1r; i++) {
        int ny = peak_y + (sl_y - peak_y) * (i + 1) / seg_d1r;
        if (px + i < x + w) fb[ny * LCD_W + (px + i)] = COL_BAR_FILL;
    }
    px += seg_d1r; py = sl_y;
    /* Sustain */
    for (int i = 0; i < sustain_w; i++) {
        if (px + i < x + w) fb[sl_y * LCD_W + (px + i)] = COL_BAR_FILL;
    }
    px += sustain_w;
    /* D2R */
    for (int i = 0; i < seg_d2r; i++) {
        int ny = sl_y + (floor_y - sl_y) * (i + 1) / seg_d2r;
        if (px + i < x + w) fb[ny * LCD_W + (px + i)] = COL_BAR_FILL;
    }
    px += seg_d2r; py = floor_y;
    /* RR */
    for (int i = 0; i < seg_rr; i++) {
        if (px + i < x + w) fb[floor_y * LCD_W + (px + i)] = COL_TEXT_DIM;
    }
}

static void render_screen_ops(uint32_t *fb)
{
    int y = TAB_BAR_H + 4;
    char buf[16];
    op_t *op = &g_current_voice.op[g_cur_op];

    /* Header: op N of 4 */
    draw_text(fb, "OP ", 6, y, 2, COL_HEADING);
    int_to_str(g_cur_op + 1, buf);
    draw_text(fb, buf, 6 + text_width("OP ", 2), y, 2, COL_VALUE);
    draw_text(fb, " / 4", 6 + text_width("OP ", 2) + text_width(buf, 2), y, 2, COL_HEADING);
    draw_text(fb, op->carrier ? "carrier" : "modulator",
              LCD_W - text_width(op->carrier ? "carrier" : "modulator", 2) - 6, y, 2,
              op->enabled ? COL_VALUE : COL_TEXT_DIM);
    y += 20;
    draw_hline(fb, 6, y, LCD_W - 12, COL_TEXT_DIM);
    y += 6;

    /* Envelope graph */
    draw_envelope(fb, 6, y, LCD_W - 12, 60, op);
    y += 64;

    /* AR / D1R / SL / D2R / RR row */
    {
        int x = 6;
        const char *lbls[] = {"AR","D1R","SL","D2R","RR"};
        int vals[] = {op->attack, op->decay1, op->sustain, op->decay2, op->release};
        int fields[] = {OF_AR, OF_D1R, OF_SL, OF_D2R, OF_RR};
        int col_w = (LCD_W - 12) / 5;
        for (int i = 0; i < 5; i++) {
            int is = (g_cur_op_field == fields[i]);
            if (is) fill_rect(fb, x + i * col_w - 2, y - 2, col_w, 22, COL_CURSOR_BG);
            draw_text(fb, lbls[i], x + i * col_w + 4, y, 1, is ? COL_CURSOR : COL_TEXT_DIM);
            int_to_str(vals[i], buf);
            draw_text(fb, buf, x + i * col_w + 4, y + 9, 1, is ? COL_CURSOR : COL_VALUE);
        }
        y += 28;
    }

    /* Other params, condensed two-column */
    int row_h = 12;
    int xL = 8, xR = LCD_W / 2 + 8;
    int yL = y, yR = y;

    int_to_str(op->output_level, buf);
    draw_text(fb, "OutLvl", xL, yL, 1, g_cur_op_field == OF_OUT ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xL + 60, yL, 1, g_cur_op_field == OF_OUT ? COL_CURSOR : COL_VALUE);
    yL += row_h;

    draw_text(fb, "Curve", xL, yL, 1, g_cur_op_field == OF_CURVE ? COL_CURSOR : COL_TEXT);
    draw_text(fb, curve_names[op->level_curve], xL + 60, yL, 1,
              g_cur_op_field == OF_CURVE ? COL_CURSOR : COL_VALUE);
    yL += row_h;

    int_to_str(op->lvl_velocity, buf);
    draw_text(fb, "LvlVel", xL, yL, 1, g_cur_op_field == OF_LVL_VEL ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xL + 60, yL, 1, g_cur_op_field == OF_LVL_VEL ? COL_CURSOR : COL_VALUE);
    yL += row_h;

    int_to_str(op->ar_velocity, buf);
    draw_text(fb, "ARVel", xL, yL, 1, g_cur_op_field == OF_AR_VEL ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xL + 60, yL, 1, g_cur_op_field == OF_AR_VEL ? COL_CURSOR : COL_VALUE);
    yL += row_h;

    int_to_str(op->lvl_depth, buf);
    draw_text(fb, "LvlDpth", xL, yL, 1, g_cur_op_field == OF_LVL_DEPTH ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xL + 60, yL, 1, g_cur_op_field == OF_LVL_DEPTH ? COL_CURSOR : COL_VALUE);
    yL += row_h;

    int_to_str(op->rate_depth, buf);
    draw_text(fb, "RtDpth", xL, yL, 1, g_cur_op_field == OF_RATE_DEPTH ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xL + 60, yL, 1, g_cur_op_field == OF_RATE_DEPTH ? COL_CURSOR : COL_VALUE);

    int_to_str(op->coarse, buf);
    draw_text(fb, "Coarse", xR, yR, 1, g_cur_op_field == OF_COARSE ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xR + 60, yR, 1, g_cur_op_field == OF_COARSE ? COL_CURSOR : COL_VALUE);
    yR += row_h;

    int_to_str(op->multiple, buf);
    draw_text(fb, "Mult", xR, yR, 1, g_cur_op_field == OF_MULT ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xR + 60, yR, 1, g_cur_op_field == OF_MULT ? COL_CURSOR : COL_VALUE);
    yR += row_h;

    int_to_str(op->fine, buf);
    draw_text(fb, "Fine", xR, yR, 1, g_cur_op_field == OF_FINE ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xR + 60, yR, 1, g_cur_op_field == OF_FINE ? COL_CURSOR : COL_VALUE);
    yR += row_h;

    int_to_str(op->adjust_vol, buf);
    draw_text(fb, "AdjVol", xR, yR, 1, g_cur_op_field == OF_ADJUST ? COL_CURSOR : COL_TEXT);
    draw_text(fb, buf, xR + 60, yR, 1, g_cur_op_field == OF_ADJUST ? COL_CURSOR : COL_VALUE);
    yR += row_h;

    draw_text(fb, op->enabled ? "[X] enabled" : "[ ] enabled",
              xR, yR, 1, g_cur_op_field == OF_ENABLE ? COL_CURSOR : COL_TEXT);
    yR += row_h;

    draw_text(fb, op->carrier ? "Carrier" : "Modulator", xR, yR, 1,
              g_cur_op_field == OF_ROLE ? COL_CURSOR : COL_VALUE);
}

static void ops_handle_input(uint32_t pressed)
{
    op_t *op = &g_current_voice.op[g_cur_op];
    /* L/R cycles operator (overrides tab cycling for ops screen — common
     * cap-letter shortcut). Actually L/R already cycle tabs; use Z+L/R
     * or Cu/Cd for op switching. C-up/down: previous/next op. */
    if (pressed & BTN_C_UP)   g_cur_op = (g_cur_op + 4 - 1) % 4;
    if (pressed & BTN_C_DOWN) g_cur_op = (g_cur_op + 1) % 4;
    if (pressed & BTN_UP)   g_cur_op_field = (g_cur_op_field + OF_COUNT - 1) % OF_COUNT;
    if (pressed & BTN_DOWN) g_cur_op_field = (g_cur_op_field + 1) % OF_COUNT;
    int dir = (pressed & BTN_RIGHT) ? 1 : (pressed & BTN_LEFT) ? -1 : 0;
    if (!dir) return;
    #define CLAMP_U(v, hi) do { int t = (v) + dir; if (t < 0) t = 0; if (t > (hi)) t = (hi); (v) = (uint8_t)t; } while (0)
    switch (g_cur_op_field) {
    case OF_ENABLE: op->enabled ^= 1; break;
    case OF_ROLE:   op->carrier ^= 1; break;
    case OF_OUT:        CLAMP_U(op->output_level, 127); break;
    case OF_CURVE:      CLAMP_U(op->level_curve, 3); break;
    case OF_LVL_VEL:    CLAMP_U(op->lvl_velocity, 31); break;
    case OF_AR_VEL:     CLAMP_U(op->ar_velocity, 3); break;
    case OF_LVL_DEPTH:  CLAMP_U(op->lvl_depth, 15); break;
    case OF_RATE_DEPTH: CLAMP_U(op->rate_depth, 3); break;
    case OF_ADJUST:     CLAMP_U(op->adjust_vol, 15); break;
    case OF_COARSE:     CLAMP_U(op->coarse, 3); break;
    case OF_MULT:       CLAMP_U(op->multiple, 15); break;
    case OF_FINE: { int t = op->fine + dir; if (t < -4) t = -4; if (t > 3) t = 3; op->fine = (int8_t)t; break; }
    case OF_AR:  CLAMP_U(op->attack, 31); break;
    case OF_D1R: CLAMP_U(op->decay1, 31); break;
    case OF_SL:  CLAMP_U(op->sustain, 15); break;
    case OF_D2R: CLAMP_U(op->decay2, 31); break;
    case OF_RR:  CLAMP_U(op->release, 15); break;
    }
    #undef CLAMP_U
}

/* ================================================================
 *  Main
 * ================================================================ */

/* SysEx replies from the FB-01 land here. Phase-1 stub just logs
 * the length; future commits can parse and populate the model. */
static void fb01_sysex_in(const uint8_t *bytes, uint32_t len)
{
    (void)bytes;
    uart_puts("[midi rx] sysex "); uart_putdec(len); uart_puts(" bytes\n");
}

int main(void)
{
    uart_puts("\n\n=== FB-01 Sound Editor ===\n");
    timer_init();
    mmu_init();
    pmu_init();
    video_init();
    input_init(INPUT_N64);
    midi_init();
    midi_sysex_set_handler(fb01_sysex_in);
    irq_global_enable();

    /* Clear UI0 overlay + VI1 sprite buffers — boot DRAM garbage otherwise
     * blends over our VI0 paint. */
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    init_model();

    int active_tab = TAB_CONFIG;
    uint32_t back_fb  = FB1_ADDR;
    uint32_t front_fb = FB0_ADDR;

    while (1) {
        uint32_t t0 = timer_read();

        midi_pump();   /* drain UART1 RX ring → SysEx assembler */

        (void)input_poll();
        uint32_t pressed = input_pressed();

        /* L/R cycles tabs — but NOT while we're inside a modal sub-edit
         * (e.g. SET's instrument sub-edit). Otherwise B-to-exit-sub-edit
         * would be lost the moment you press L. */
        int tabs_locked = (active_tab == TAB_SET && g_set_mode == 1);
        if (!tabs_locked) {
            if (pressed & BTN_L) active_tab = (active_tab + TAB_COUNT - 1) % TAB_COUNT;
            if (pressed & BTN_R) active_tab = (active_tab + 1) % TAB_COUNT;
        }

        const char *hint;
        switch (active_tab) {
        case TAB_BANKS:
            hint = "Dpad nav  L/R tab  A load voice  Z req bank dump"; break;
        case TAB_SET:
            hint = (g_set_mode == 0) ? "Dpad nav inst  A edit  L/R tab"
                                     : "Dpad nav  Dpad-LR adj  B back"; break;
        case TAB_VOICE:
            hint = "Dpad nav  Dpad-LR adj  Z send voice  Cu req  L/R tab"; break;
        case TAB_OPS:
            hint = "Dpad nav  Dpad-LR adj  Cu/Cd op  L/R tab"; break;
        default:
            hint = "Dpad nav  L/R tab  A activate  B back";
        }
        switch (active_tab) {
        case TAB_CONFIG:   config_handle_input(pressed); break;
        case TAB_AUTOMATE: auto_handle_input(pressed); break;
        case TAB_BANKS:    banks_handle_input(pressed); break;
        case TAB_SET:      set_handle_input(pressed); break;
        case TAB_VOICE:    voice_handle_input(pressed); break;
        case TAB_OPS:      ops_handle_input(pressed); break;
        }

        uint32_t *fb = (uint32_t *)back_fb;
        fill_rect(fb, 0, 0, LCD_W, LCD_H, COL_BG);
        render_tab_bar(fb, active_tab, "");

        switch (active_tab) {
        case TAB_CONFIG:   render_screen_config(fb); break;
        case TAB_AUTOMATE: render_screen_auto(fb); break;
        case TAB_BANKS:    render_screen_banks(fb); break;
        case TAB_SET:      render_screen_set(fb); break;
        case TAB_VOICE:    render_screen_voice(fb); break;
        case TAB_OPS:      render_screen_ops(fb); break;
        }
        render_status_bar(fb, hint);

        dcache_clean_fb(back_fb);
        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;
    }
    return 0;
}
