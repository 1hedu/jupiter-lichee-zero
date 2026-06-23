/*
 * Jupiter SDK — MT-32 Real-Time MIDI Player (fresh attempt)
 *
 * Based on the opn2_rt pattern (frame-paced real-time synth) but with
 * MT-32 (Munt) as the synth engine. Embedded ROMs + MIDI file.
 *
 * ROMs:  MT-32 v1.07 legacy (Control 64KB + PCM 512KB)
 * Song:  Monkey Island — Complete Soundtrack (.mid, SMF format 1)
 * Rate:  48 kHz codec output; munt resamples from native 32 kHz internally.
 *
 * Build: make GAME=examples/mt32_rt/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include <math.h>
#include "c_interface/c_interface.h"

/* ================================================================
 *  On-screen state display (5x7 pixel font, reused from menu)
 * ================================================================ */
#define FNT_W 5
#define FNT_H 7
typedef struct { char ch; uint8_t cols[FNT_W]; } disp_glyph_t;
static const disp_glyph_t disp_font[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}}, {'A', {0x7E,0x09,0x09,0x09,0x7E}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}}, {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}}, {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}}, {'G', {0x3E,0x41,0x49,0x49,0x7A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}}, {'I', {0x00,0x41,0x7F,0x41,0x00}},
    {'J', {0x20,0x40,0x41,0x3F,0x01}}, {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}}, {'M', {0x7F,0x02,0x0C,0x02,0x7F}},
    {'N', {0x7F,0x04,0x08,0x10,0x7F}}, {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}}, {'Q', {0x3E,0x41,0x51,0x21,0x5E}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}}, {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}}, {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'V', {0x1F,0x20,0x40,0x20,0x1F}}, {'W', {0x7F,0x20,0x18,0x20,0x7F}},
    {'X', {0x63,0x14,0x08,0x14,0x63}}, {'Y', {0x07,0x08,0x70,0x08,0x07}},
    {'Z', {0x61,0x51,0x49,0x45,0x43}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}}, {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}}, {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}}, {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}}, {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}}, {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'-', {0x08,0x08,0x08,0x08,0x08}}, {':', {0x00,0x36,0x36,0x00,0x00}},
    {'.', {0x00,0x60,0x60,0x00,0x00}}, {'/', {0x20,0x10,0x08,0x04,0x02}},
    {'+', {0x08,0x08,0x3E,0x08,0x08}}, {'=', {0x14,0x14,0x14,0x14,0x14}},
    {'(', {0x00,0x1C,0x22,0x41,0x00}}, {')', {0x00,0x41,0x22,0x1C,0x00}},
    {'%', {0x23,0x13,0x08,0x64,0x62}}, {'>', {0x00,0x41,0x22,0x14,0x08}},
    {'<', {0x00,0x08,0x14,0x22,0x41}}, {'_', {0x40,0x40,0x40,0x40,0x40}},
    {',', {0x00,0x50,0x30,0x00,0x00}}, {'\'',{0x00,0x00,0x07,0x00,0x00}},
    {'!', {0x00,0x00,0x5F,0x00,0x00}}, {'?', {0x02,0x01,0x51,0x09,0x06}},
    {'*', {0x22,0x14,0x7F,0x14,0x22}},
};
#define DISP_FONT_N (sizeof(disp_font)/sizeof(disp_font[0]))

static const uint8_t *disp_glyph_for(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (uint32_t i = 0; i < DISP_FONT_N; i++)
        if (disp_font[i].ch == c) return disp_font[i].cols;
    return disp_font[0].cols;
}

static void disp_text(uint32_t *fb, const char *s, int x, int y, int sz, uint32_t color) {
    while (*s) {
        const uint8_t *g = disp_glyph_for(*s);
        for (int col = 0; col < FNT_W; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < FNT_H; row++) {
                if (!(bits & (1 << row))) continue;
                for (int dy = 0; dy < sz; dy++) {
                    int py = y + row * sz + dy;
                    if (py < 0 || py >= (int)LCD_H) continue;
                    uint32_t *line = fb + py * LCD_W;
                    for (int dx = 0; dx < sz; dx++) {
                        int px = x + col * sz + dx;
                        if (px >= 0 && px < (int)LCD_W) line[px] = color;
                    }
                }
            }
        }
        x += (FNT_W + 1) * sz;
        s++;
    }
}

static char* itoa_u(uint32_t v, char *buf) {
    char tmp[12]; int n = 0;
    if (v == 0) { *buf++ = '0'; *buf = 0; return buf; }
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    while (n) *buf++ = tmp[--n];
    *buf = 0; return buf;
}

static void disp_int(uint32_t *fb, uint32_t v, int x, int y, int sz, uint32_t color) {
    char buf[16]; itoa_u(v, buf);
    disp_text(fb, buf, x, y, sz, color);
}

static void disp_fill(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int py = y; py < y + h && py < (int)LCD_H; py++) {
        if (py < 0) continue;
        uint32_t *row = fb + py * LCD_W;
        for (int px = x; px < x + w && px < (int)LCD_W; px++) {
            if (px >= 0) row[px] = color;
        }
    }
}

/* Colors */
#define COL_BG      0xFF000000
#define COL_TITLE   0xFF40C0FF
#define COL_LABEL   0xFF808080
#define COL_VALUE   0xFFFFFFFF
#define COL_HILITE  0xFFFFE040
#define COL_ACT     0xFF40FF80
#define COL_WARN    0xFFFF4040

/* math_neon stubs — used for per-function validation at boot */
extern float powf_neon_hfp(float, float);
extern float expf_neon_hfp(float);
extern float logf_neon_hfp(float);
extern float log10f_neon_hfp(float);
extern float powf_c(float, float);
extern float expf_c(float);
extern float logf_c(float);
extern float log10f_c(float);

/* ---- Embedded binary assets (objcopy) ---- */
extern const uint8_t _binary_examples_mt32_rt_roms_MT32_CONTROL_ROM_start[];
extern const uint8_t _binary_examples_mt32_rt_roms_MT32_CONTROL_ROM_end[];
extern const uint8_t _binary_examples_mt32_rt_roms_MT32_PCM_ROM_start[];
extern const uint8_t _binary_examples_mt32_rt_roms_MT32_PCM_ROM_end[];

#include "midi_catalog.h"

/* ---- Audio ring buffer (shared with DMA ISR) ---- */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;

#define OUT_RATE    48000
#define FRAMES_60   (OUT_RATE / 60)

static mt32emu_context synth;

/* ================================================================
 *  Minimal SMF (Standard MIDI File) parser
 * ================================================================ */

#define MAX_TRACKS 24

typedef struct {
    const uint8_t *data;    /* track start */
    const uint8_t *end;     /* one past last byte */
    const uint8_t *cur;     /* current position */
    uint32_t next_tick;     /* absolute tick of next event */
    uint8_t running_status;
    uint8_t done;
} smf_track_t;

typedef struct {
    uint16_t format;        /* 0, 1, or 2 */
    uint16_t num_tracks;
    uint16_t division;      /* ticks per quarter note */
    smf_track_t tracks[MAX_TRACKS];
    uint32_t tempo_us_per_qn; /* microseconds per quarter note (default 500000 = 120 BPM) */
    uint32_t current_tick;
    uint64_t sample_accum;  /* fractional sample position at current tick */
    uint64_t samples_per_tick_q16; /* Q16 fixed-point: samples per tick */
} smf_t;

static inline uint16_t rd16be(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static inline uint32_t rd32be(const uint8_t *p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* Read variable-length quantity. Advances cur. */
static uint32_t smf_read_vlq(const uint8_t **pp, const uint8_t *end)
{
    uint32_t v = 0;
    while (*pp < end) {
        uint8_t b = *(*pp)++;
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80)) return v;
    }
    return v;
}

static void smf_recalc_rate(smf_t *s)
{
    /* samples_per_tick = (tempo_us_per_qn / division) * OUT_RATE / 1,000,000
     * Use Q16 fixed-point. Q32 overflows uint64_t for typical tempos. */
    uint64_t num = (uint64_t)s->tempo_us_per_qn * OUT_RATE;  /* ~2.4e10 at 120 BPM */
    uint64_t den = (uint64_t)s->division * 1000000ULL;
    s->samples_per_tick_q16 = (num << 16) / den;
}

/* Parse SMF header + track headers. Does NOT advance through events yet. */
static int smf_load(smf_t *s, const uint8_t *data, uint32_t size)
{
    memset(s, 0, sizeof(*s));

    if (size < 14 || memcmp(data, "MThd", 4) != 0) return -1;
    uint32_t hdr_len = rd32be(data + 4);
    if (hdr_len < 6) return -2;
    s->format     = rd16be(data + 8);
    s->num_tracks = rd16be(data + 10);
    s->division   = rd16be(data + 12);
    if (s->division & 0x8000) return -3;  /* SMPTE not supported */
    if (s->num_tracks > MAX_TRACKS) s->num_tracks = MAX_TRACKS;

    /* Default tempo: 120 BPM */
    s->tempo_us_per_qn = 500000;
    smf_recalc_rate(s);

    const uint8_t *p = data + 8 + hdr_len;
    const uint8_t *end = data + size;

    for (int i = 0; i < s->num_tracks && p + 8 <= end; i++) {
        if (memcmp(p, "MTrk", 4) != 0) {
            /* Skip unknown chunk */
            uint32_t clen = rd32be(p + 4);
            p += 8 + clen;
            i--; continue;
        }
        uint32_t clen = rd32be(p + 4);
        s->tracks[i].data = p + 8;
        s->tracks[i].end  = p + 8 + clen;
        s->tracks[i].cur  = p + 8;
        s->tracks[i].running_status = 0;
        s->tracks[i].done = 0;
        /* Read first delta */
        s->tracks[i].next_tick = smf_read_vlq(&s->tracks[i].cur, s->tracks[i].end);
        p += 8 + clen;
    }
    return 0;
}

/* Process one event from track `t` at absolute_tick = t->next_tick.
 * Returns 0 normally, 1 if track ended, <0 on error. */
static int smf_process_event(smf_t *s, smf_track_t *t)
{
    if (t->cur >= t->end) { t->done = 1; return 1; }

    uint8_t status = *t->cur;
    if (status < 0x80) {
        /* Running status */
        status = t->running_status;
    } else {
        t->cur++;
        if (status < 0xF0) t->running_status = status;
    }

    if (status >= 0x80 && status < 0xF0) {
        /* Channel message */
        uint8_t d1 = 0, d2 = 0;
        uint8_t type = status & 0xF0;
        /* 1-byte: program change (0xC0), channel pressure (0xD0). All others 2-byte. */
        int two = !(type == 0xC0 || type == 0xD0);
        if (t->cur < t->end) d1 = *t->cur++;
        if (two && t->cur < t->end) d2 = *t->cur++;

        uint32_t msg = status | (d1 << 8) | (d2 << 16);
        mt32emu_play_msg(synth, msg);
    } else if (status == 0xFF) {
        /* Meta event */
        if (t->cur >= t->end) { t->done = 1; return 1; }
        uint8_t meta_type = *t->cur++;
        uint32_t meta_len = smf_read_vlq(&t->cur, t->end);
        const uint8_t *meta_data = t->cur;
        if (t->cur + meta_len > t->end) { t->done = 1; return 1; }
        t->cur += meta_len;

        if (meta_type == 0x51 && meta_len == 3) {
            /* Set Tempo */
            s->tempo_us_per_qn = (meta_data[0] << 16) | (meta_data[1] << 8) | meta_data[2];
            smf_recalc_rate(s);
        } else if (meta_type == 0x2F) {
            /* End of Track */
            t->done = 1;
            return 1;
        }
        /* Ignore other meta events */
    } else if (status == 0xF0 || status == 0xF7) {
        /* SysEx */
        uint32_t sx_len = smf_read_vlq(&t->cur, t->end);
        if (t->cur + sx_len > t->end) { t->done = 1; return 1; }
        if (status == 0xF0) {
            /* Full sysex starting with F0. munt expects data starting with F0. */
            /* Build a buffer with F0 prepended (max typical MT-32 sysex is ~300 bytes). */
            static uint8_t sx_buf[4096];
            if (sx_len < sizeof(sx_buf) - 1) {
                sx_buf[0] = 0xF0;
                memcpy(sx_buf + 1, t->cur, sx_len);
                mt32emu_play_sysex(synth, sx_buf, sx_len + 1);
            }
        } else {
            /* F7 escape — raw MIDI bytes; pass to munt as-is (rare in SMF) */
            mt32emu_play_sysex(synth, t->cur, sx_len);
        }
        t->cur += sx_len;
    }

    /* Read delta for NEXT event */
    if (t->cur < t->end) {
        uint32_t delta = smf_read_vlq(&t->cur, t->end);
        t->next_tick += delta;
    } else {
        t->done = 1;
    }
    return 0;
}

/* Advance SMF up to `target_tick`, dispatching all events. */
static void smf_advance_to_tick(smf_t *s, uint32_t target_tick)
{
    int any;
    do {
        any = 0;
        for (int i = 0; i < s->num_tracks; i++) {
            smf_track_t *t = &s->tracks[i];
            while (!t->done && t->next_tick <= target_tick) {
                smf_process_event(s, t);
                any = 1;
            }
        }
    } while (any);  /* Loop in case events unblock others at same tick */
    s->current_tick = target_tick;
}

static int smf_all_done(const smf_t *s)
{
    for (int i = 0; i < s->num_tracks; i++)
        if (!s->tracks[i].done) return 0;
    return 1;
}

/* ================================================================
 *  Main
 * ================================================================ */

static smf_t smf;
static uint32_t midi_size;       /* size of the currently loaded MIDI */
static uint64_t total_samples;   /* samples rendered so far */
static uint64_t tick_in_samples; /* current MIDI tick position in samples (Q32) */

/* ---- Song catalog state ---- */
static int current_midi_idx = -1;     /* index into MIDI_CATALOG */
static int current_section_idx = 0;   /* MI only: MI_SECTIONS index */
static int dune_init_applied = 0;     /* once-per-session guard */

/* Default startup song: MI Introduction */
static int default_midi_idx(void) {
    for (int i = 0; i < MIDI_COUNT; i++)
        if (MIDI_CATALOG[i].game == GAME_MI) return i;
    return 0;
}

static int find_dune_init_idx(void) {
    for (int i = 0; i < MIDI_COUNT; i++)
        if (MIDI_CATALOG[i].game == GAME_INIT) return i;
    return -1;
}

/* Silence any ringing notes before re-loading/seeking. */
static void all_notes_off(void) {
    for (int ch = 0; ch < 16; ch++) {
        /* CC 123 (All Notes Off) — packed: status | (cc<<8) | (0<<16) */
        uint32_t msg = (uint32_t)(0xB0 | ch) | (123u << 8) | (0u << 16);
        mt32emu_play_msg(synth, msg);
        /* CC 120 (All Sound Off) too, for good measure */
        msg = (uint32_t)(0xB0 | ch) | (120u << 8) | (0u << 16);
        mt32emu_play_msg(synth, msg);
    }
}

/* Blast a Dune_init.mid through the synth synchronously (no audio rendered).
 * All sysex + any note events get applied to synth state at "tick infinity". */
static void apply_dune_init(void) {
    int idx = find_dune_init_idx();
    if (idx < 0) return;
    smf_t tmp;
    uint32_t isz = (uint32_t)(MIDI_CATALOG[idx].data_end - MIDI_CATALOG[idx].data);
    if (smf_load(&tmp, MIDI_CATALOG[idx].data, isz) != 0) {
        uart_puts("[init] Dune_init load failed\n"); return;
    }
    smf_advance_to_tick(&tmp, 0xFFFFFFFFu);
    dune_init_applied = 1;
    uart_puts("[init] Dune_init applied\n");
}

static void load_and_start_song(int midi_idx, int section_idx)
{
    if (midi_idx < 0 || midi_idx >= MIDI_COUNT) return;
    const midi_entry_t *e = &MIDI_CATALOG[midi_idx];
    all_notes_off();

    /* For Dune II tracks, make sure Dune_init has been applied at least once. */
    if (e->game == GAME_DUNE2 && !dune_init_applied) {
        apply_dune_init();
    }

    midi_size = (uint32_t)(e->data_end - e->data);
    if (smf_load(&smf, e->data, midi_size) != 0) {
        uart_puts("[smf] load failed for "); uart_puts(e->title); uart_puts("\n");
        return;
    }
    current_midi_idx = midi_idx;

    /* MI section seek */
    uint32_t seek_tick = 0;
    if (e->game == GAME_MI) {
        if (section_idx < 0) section_idx = 0;
        if (section_idx >= MI_SECTION_COUNT) section_idx = MI_SECTION_COUNT - 1;
        current_section_idx = section_idx;
        seek_tick = MI_SECTIONS[section_idx].tick;
    } else {
        current_section_idx = 0;
    }
    if (seek_tick > 0) smf_advance_to_tick(&smf, seek_tick);

    total_samples = 0;
    tick_in_samples = (uint64_t)seek_tick * smf.samples_per_tick_q16;

    uart_puts("[song] "); uart_puts(e->title);
    if (e->game == GAME_MI) {
        uart_puts(" / "); uart_puts(MI_SECTIONS[current_section_idx].title);
    }
    uart_puts("\n");
}

/* ---- Menu state machine ---- */
typedef enum { MENU_NONE=0, MENU_TOP, MENU_DUNE, MENU_MI, MENU_CODEC } menu_state_t;
static menu_state_t menu_state = MENU_NONE;
#define MENU_TOP_COUNT 4
static int menu_sel_top = 0;   /* 0=Restart, 1=MI, 2=Dune II, 3=HP vol */
static int menu_sel_dune = 0;  /* index into filtered Dune list */
static int menu_sel_mi = 0;    /* index into MI_SECTIONS */
static int menu_scroll_dune = 0; /* scroll offset for Dune list */

/* Codec HP PA volume cycler: 0..63 → -63..0 dB. Default 43 = -20 dB = init value. */
static const uint8_t hp_vol_choices[] = { 5, 15, 25, 35, 43, 50, 55, 60, 63 };
static const char *hp_vol_labels[] = {
    "-58 dB","-48 dB","-38 dB","-28 dB","-20 dB (default)","-13 dB","-8 dB","-3 dB","0 dB"
};
#define HP_VOL_COUNT (sizeof(hp_vol_choices)/sizeof(hp_vol_choices[0]))
static int menu_sel_codec = 4;  /* start at default */

/* Filtered list of Dune II entries (indices into MIDI_CATALOG) */
static int dune_indices[MIDI_COUNT];
static int dune_count = 0;
static void build_dune_list(void) {
    dune_count = 0;
    for (int i = 0; i < MIDI_COUNT; i++)
        if (MIDI_CATALOG[i].game == GAME_DUNE2)
            dune_indices[dune_count++] = i;
}

static void dispatch_midi_for_samples(uint32_t nsamples)
{
    /* Track sample position in Q16. Convert to integer ticks on each update. */
    tick_in_samples += ((uint64_t)nsamples << 16);
    uint32_t new_tick = (uint32_t)(tick_in_samples / smf.samples_per_tick_q16);
    smf_advance_to_tick(&smf, new_tick);
    total_samples += nsamples;
}

/* One-shot sample capture — dumps 512 stereo samples to UART as hex.
 * Compare against Linux smf_render output at same tick to prove bit-identity
 * or find divergence. Main loop sets dump_capture_armed=1 at the right time. */
#define DUMP_SAMPLES 512
static int16_t sample_capture[DUMP_SAMPLES * 2];
static int dump_sample_count = 0;
static int dump_done = 0;
static int dump_capture_armed = 0;
static uint32_t dump_start_tick = 0;  /* the MIDI tick at which capture began */

/* Stream-separated render buffers. MT-32 produces 3 internal stereo streams:
 *   - nonReverb (dry):  instruments with reverb-bypass routing
 *   - reverbDry:        instruments going INTO the reverb stage
 *   - reverbWet:        output OF the reverb stage
 * Real hardware mixes dry + reverbDry(bypass_feedback) + reverbWet.
 * By splitting and inspecting each, we can see which stream has the fuzz.
 */
static int16_t ren_nr_l[FRAMES_60], ren_nr_r[FRAMES_60];
static int16_t ren_rd_l[FRAMES_60], ren_rd_r[FRAMES_60];
static int16_t ren_rw_l[FRAMES_60], ren_rw_r[FRAMES_60];

/* Heap monitoring (exposed by libc_shim.c) */
extern uint32_t heap_used(void);

/* Render mode for live isolation */
enum { MODE_FULL = 0, MODE_DRY_ONLY, MODE_REVERB_DRY_ONLY, MODE_REVERB_WET_ONLY, MODE_NO_REVERB, MODE_COUNT };
static int render_mode = MODE_FULL;
static const char *mode_names[] = {
    "FULL(nr+rd+rw)", "DRY-ONLY(nr)", "REVDRY-ONLY(rd)", "REVWET-ONLY(rw)", "NO-REVERB(nr+rd)"
};

/* Analog output modes (cycle via C-right) */
static int analog_mode_idx = 2; /* default: ACCURATE */
static const mt32emu_analog_output_mode analog_modes[] = {
    MT32EMU_AOM_DIGITAL_ONLY, MT32EMU_AOM_COARSE,
    MT32EMU_AOM_ACCURATE, MT32EMU_AOM_OVERSAMPLED
};
static const char *analog_mode_names[] = {
    "DIGITAL_ONLY", "COARSE", "ACCURATE", "OVERSAMPLED"
};
#define NUM_ANALOG_MODES 4

/* "Nice" mode combinations (cycle via C-up) — user-wanted comparison */
enum { NICE_NONE = 0, NICE_AMPRAMP, NICE_PANNING, NICE_PARTIALMIX, NICE_ALL, NICE_COUNT };
static int nice_mode_idx = 1; /* default: AMPRAMP (munt default) */
static const char *nice_mode_names[] = {
    "none", "amp-ramp", "amp+panning", "amp+partial-mix", "all-nice"
};

/* SRC quality (cycle via C-left) */
static int srcq_idx = 3; /* default: BEST */
static const mt32emu_samplerate_conversion_quality srcq_modes[] = {
    MT32EMU_SRCQ_FASTEST, MT32EMU_SRCQ_FAST, MT32EMU_SRCQ_GOOD, MT32EMU_SRCQ_BEST
};
static const char *srcq_names[] = {
    "FASTEST", "FAST", "GOOD", "BEST"
};
#define NUM_SRCQ 4

/* Math path (cycle via C-down) — routes munt's sinf/powf/expf/logf/log10f */
extern int g_math_path;  /* from lib/mt32_math_shim.c */
static const char *math_path_names[] = { "C-poly", "NEON-asm", "libm" };
#define NUM_MATH_PATHS 3

/* DAC input mode (cycle via D-Up). Changes LA32 output bit routing. */
static int dac_mode_idx = 0; /* default: NICE */
static const mt32emu_dac_input_mode dac_modes[] = {
    MT32EMU_DAC_NICE, MT32EMU_DAC_PURE,
    MT32EMU_DAC_GENERATION1, MT32EMU_DAC_GENERATION2
};
static const char *dac_mode_names[] = {
    "NICE", "PURE", "GEN1", "GEN2"
};
#define NUM_DAC_MODES 4

/* Partial count limit (cycle via D-Down). Default 32. */
static int partial_count_idx = 0;
static const uint32_t partial_counts[] = { 32, 24, 16, 8 };
static const char *partial_count_names[] = { "32", "24", "16", "8" };
#define NUM_PARTIAL_COUNTS 4

/* Output gain (L shoulder cycles). */
static int gain_idx = 4; /* default: 1.0 */
static const float gain_values[] = { 0.25f, 0.5f, 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 2.0f };
static const char *gain_names[]  = { "0.25", "0.5", "0.75", "0.9", "1.0", "1.25", "1.5", "2.0" };
#define NUM_GAINS 8

/* Reverb output gain (R shoulder cycles). */
static int rev_gain_idx = 4; /* default: 1.0 */
/* reuses gain_values + gain_names */

/* Master MIDI volume override (D-Right cycles). Scales ALL partial amplitudes
 * BEFORE they're summed into int16 Part buffers. Lower values reduce the
 * intermediate-clipping distortion in Partial.cpp:366-367. */
static int master_vol_idx = 5;  /* default: 100 (no attenuation) */
static const uint8_t master_vols[] = { 25, 40, 50, 60, 75, 100 };  /* 100=normal */
static const char *master_vol_names[] = { "25", "40", "50", "60", "75", "100" };
#define NUM_MASTER_VOLS 6

/* Part isolation state.
 *   solo_part != -1 → play only that part, mute everything else (Z button)
 *   mute_part != -1 → mute that part, play everything else  (D-Left button)
 * Solo and mute are mutually exclusive. */
static int solo_part = -1;
static int mute_part = -1;
#define NUM_ISO_STATES 10  /* -1 (off) + 9 parts */

static void apply_solo(void) {
    for (int p = 0; p < 9; p++) {
        if (solo_part != -1) {
            /* Solo mode: only chosen part plays */
            if (p == solo_part) mt32emu_set_part_volume_override(synth, p, 0xFF);
            else                mt32emu_set_part_volume_override(synth, p, 0);
        } else if (mute_part != -1) {
            /* Mute-one mode: chosen part silent, rest plays */
            if (p == mute_part) mt32emu_set_part_volume_override(synth, p, 0);
            else                mt32emu_set_part_volume_override(synth, p, 0xFF);
        } else {
            /* Off: no overrides */
            mt32emu_set_part_volume_override(synth, p, 0xFF);
        }
    }
}

static void apply_nice_modes(int idx)
{
    int amp = (idx == NICE_AMPRAMP || idx == NICE_PANNING || idx == NICE_PARTIALMIX || idx == NICE_ALL);
    int pan = (idx == NICE_PANNING || idx == NICE_ALL);
    int mix = (idx == NICE_PARTIALMIX || idx == NICE_ALL);
    mt32emu_set_nice_amp_ramp_enabled(synth, amp ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE);
    mt32emu_set_nice_panning_enabled(synth, pan ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE);
    mt32emu_set_nice_partial_mixing_enabled(synth, mix ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE);
}

extern void heap_rewind(uint32_t snapshot);
static uint32_t heap_checkpoint = 0;  /* set after first open_synth */

static void reopen_synth_with_mode(void)
{
    mt32emu_close_synth(synth);
    /* Reclaim munt's allocations — our free() is a no-op. */
    if (heap_checkpoint) heap_rewind(heap_checkpoint);

    mt32emu_set_analog_output_mode(synth, analog_modes[analog_mode_idx]);
    mt32emu_select_renderer_type(synth, MT32EMU_RT_BIT16S);
    mt32emu_set_samplerate_conversion_quality(synth, srcq_modes[srcq_idx]);
    mt32emu_set_stereo_output_samplerate(synth, (double)OUT_RATE);
    mt32emu_set_partial_count(synth, partial_counts[partial_count_idx]);
    mt32emu_return_code rc = mt32emu_open_synth(synth);
    apply_nice_modes(nice_mode_idx);
    mt32emu_set_dac_input_mode(synth, dac_modes[dac_mode_idx]);
    mt32emu_set_output_gain(synth, gain_values[gain_idx]);
    mt32emu_set_reverb_output_gain(synth, gain_values[rev_gain_idx]);
    mt32emu_set_master_volume_override(synth, master_vols[master_vol_idx]);
    apply_solo();  /* restore solo after reopen resets overrides */

    uart_puts("[reopen] analog="); uart_puts(analog_mode_names[analog_mode_idx]);
    uart_puts(" nice="); uart_puts(nice_mode_names[nice_mode_idx]);
    uart_puts(" srcq="); uart_puts(srcq_names[srcq_idx]);
    uart_puts(" math="); uart_puts(math_path_names[g_math_path]);
    uart_puts(" dac="); uart_puts(dac_mode_names[dac_mode_idx]);
    uart_puts(" partials="); uart_puts(partial_count_names[partial_count_idx]);
    uart_puts(" gain="); uart_puts(gain_names[gain_idx]);
    uart_puts(" rev_gain="); uart_puts(gain_names[rev_gain_idx]);
    uart_puts(" master_vol="); uart_puts(master_vol_names[master_vol_idx]);
    uart_puts(" rc="); uart_putdec((uint32_t)rc);
    uart_puts(" rate="); uart_putdec(mt32emu_get_actual_stereo_output_samplerate(synth));
    uart_puts(" heap="); uart_putdec(heap_used() / 1024); uart_puts("KB");
    uart_puts("\n");
    /* Restart song at current position — partial/patch state was reset */
    if (current_midi_idx >= 0)
        load_and_start_song(current_midi_idx, current_section_idx);
}

/* Fuzz diagnostics (combined output) */
static int32_t dbg_min_l = 0, dbg_max_l = 0, dbg_min_r = 0, dbg_max_r = 0;
static uint32_t dbg_clip_l = 0, dbg_clip_r = 0;
static int64_t dbg_sum_l = 0, dbg_sum_r = 0;
static uint32_t dbg_samples = 0;
static int16_t dbg_last_l = 0, dbg_last_r = 0;
static uint32_t dbg_max_jump_l = 0, dbg_max_jump_r = 0;
static uint32_t dbg_big_jumps = 0;

/* Per-stream peak tracking — shows which stream is contributing the fuzz */
static uint32_t dbg_nr_peak = 0, dbg_rd_peak = 0, dbg_rw_peak = 0;
static uint32_t dbg_nr_jump = 0, dbg_rd_jump = 0, dbg_rw_jump = 0;
static int16_t dbg_nr_last_l = 0, dbg_rd_last_l = 0, dbg_rw_last_l = 0;
static uint32_t dbg_nr_nonzero = 0, dbg_rd_nonzero = 0, dbg_rw_nonzero = 0;

static uint32_t dbg_buf_min = 0xFFFFFFFF;
static uint32_t dbg_buf_max = 0;
static uint32_t dbg_buf_samples = 0;

static inline uint32_t u32_abs(int32_t v) { return v < 0 ? -v : v; }

static void render_frame(uint32_t frames)
{
    /* Render all three streams separately. */
    mt32emu_dac_output_bit16s_streams streams = {
        .nonReverbLeft  = ren_nr_l, .nonReverbRight = ren_nr_r,
        .reverbDryLeft  = ren_rd_l, .reverbDryRight = ren_rd_r,
        .reverbWetLeft  = ren_rw_l, .reverbWetRight = ren_rw_r,
    };
    mt32emu_render_bit16s_streams(synth, &streams, frames);

    for (uint32_t i = 0; i < frames; i++) {
        int16_t nr_l = ren_nr_l[i], nr_r = ren_nr_r[i];
        int16_t rd_l = ren_rd_l[i], rd_r = ren_rd_r[i];
        int16_t rw_l = ren_rw_l[i], rw_r = ren_rw_r[i];

        uint32_t anr = u32_abs(nr_l), ard = u32_abs(rd_l), arw = u32_abs(rw_l);
        if (anr > dbg_nr_peak) dbg_nr_peak = anr;
        if (ard > dbg_rd_peak) dbg_rd_peak = ard;
        if (arw > dbg_rw_peak) dbg_rw_peak = arw;
        if (nr_l != 0) dbg_nr_nonzero++;
        if (rd_l != 0) dbg_rd_nonzero++;
        if (rw_l != 0) dbg_rw_nonzero++;
        uint32_t nr_jmp = u32_abs((int32_t)nr_l - dbg_nr_last_l);
        uint32_t rd_jmp = u32_abs((int32_t)rd_l - dbg_rd_last_l);
        uint32_t rw_jmp = u32_abs((int32_t)rw_l - dbg_rw_last_l);
        if (nr_jmp > dbg_nr_jump) dbg_nr_jump = nr_jmp;
        if (rd_jmp > dbg_rd_jump) dbg_rd_jump = rd_jmp;
        if (rw_jmp > dbg_rw_jump) dbg_rw_jump = rw_jmp;
        dbg_nr_last_l = nr_l; dbg_rd_last_l = rd_l; dbg_rw_last_l = rw_l;

        int32_t out_l, out_r;
        switch (render_mode) {
        case MODE_DRY_ONLY:        out_l = nr_l;               out_r = nr_r;               break;
        case MODE_REVERB_DRY_ONLY: out_l = rd_l;               out_r = rd_r;               break;
        case MODE_REVERB_WET_ONLY: out_l = rw_l;               out_r = rw_r;               break;
        case MODE_NO_REVERB:       out_l = nr_l + rd_l;        out_r = nr_r + rd_r;        break;
        case MODE_FULL: default:   out_l = nr_l + rd_l + rw_l; out_r = nr_r + rd_r + rw_r; break;
        }
        if (out_l > 32767) out_l = 32767; else if (out_l < -32768) out_l = -32768;
        if (out_r > 32767) out_r = 32767; else if (out_r < -32768) out_r = -32768;
        int16_t l = (int16_t)out_l, r = (int16_t)out_r;

        /* Combined stats */
        if (l > dbg_max_l) dbg_max_l = l;
        if (l < dbg_min_l) dbg_min_l = l;
        if (r > dbg_max_r) dbg_max_r = r;
        if (r < dbg_min_r) dbg_min_r = r;
        if (l == 32767 || l == -32768) dbg_clip_l++;
        if (r == 32767 || r == -32768) dbg_clip_r++;
        dbg_sum_l += l; dbg_sum_r += r;
        uint32_t al = u32_abs((int32_t)l - dbg_last_l);
        uint32_t ar = u32_abs((int32_t)r - dbg_last_r);
        if (al > dbg_max_jump_l) dbg_max_jump_l = al;
        if (ar > dbg_max_jump_r) dbg_max_jump_r = ar;
        if (al > 10000 || ar > 10000) dbg_big_jumps++;
        dbg_last_l = l; dbg_last_r = r;
        dbg_samples++;

        /* Capture 512 stereo samples once armed (by main at the right moment) */
        if (dump_capture_armed && !dump_done && dump_sample_count < DUMP_SAMPLES) {
            sample_capture[dump_sample_count * 2] = l;
            sample_capture[dump_sample_count * 2 + 1] = r;
            dump_sample_count++;
        }

        if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2))
            continue;
        mix_buf[mix_wr & MIX_BUF_MASK] = l; mix_wr++;
        mix_buf[mix_wr & MIX_BUF_MASK] = r; mix_wr++;
    }
}

static void dump_samples_hex(void)
{
    uart_puts("\n\n=== SAMPLE CAPTURE START ===\n");
    uart_puts("Format: L R L R ... (int16 signed, hex, "); uart_putdec(DUMP_SAMPLES); uart_puts(" stereo pairs)\n");
    for (int i = 0; i < DUMP_SAMPLES * 2; i++) {
        uint16_t v = (uint16_t)sample_capture[i];
        uart_puthex(v);
        if ((i & 15) == 15) uart_puts("\n");
        else uart_puts(" ");
    }
    uart_puts("\n=== SAMPLE CAPTURE END ===\n\n");
}

static uint32_t part_states_for_display = 0;

/* Render full state to LCD at FB0. Called once per stats update (~1s). */
static void render_display(uint32_t sec, uint32_t cpu_pct, uint32_t fill,
                           uint32_t xrun, uint32_t notes, uint32_t partials,
                           int reverb_on)
{
    uint32_t *fb = (uint32_t *)FB0_ADDR;
    /* Clear */
    memset32_neon(FB0_ADDR, COL_BG, LCD_W * LCD_H * 4);

    /* Title bar */
    disp_text(fb, "MT-32 RT PLAYER", 8, 4, 2, COL_TITLE);
    char tbuf[16];
    tbuf[0] = '0' + (sec / 600);
    tbuf[1] = '0' + ((sec / 60) % 10);
    tbuf[2] = ':';
    tbuf[3] = '0' + ((sec / 10) % 6);
    tbuf[4] = '0' + (sec % 10);
    tbuf[5] = 0;
    disp_text(fb, tbuf, LCD_W - 80, 4, 2, COL_VALUE);

    /* Row 1: CPU / BUF / XRUN */
    int y = 28;
    disp_text(fb, "CPU", 8, y, 1, COL_LABEL);
    disp_int(fb, cpu_pct, 30, y, 1, cpu_pct > 80 ? COL_WARN : COL_VALUE);
    disp_text(fb, "% BUF", 50, y, 1, COL_LABEL);
    disp_int(fb, fill, 82, y, 1, fill < 1500 ? COL_WARN : COL_VALUE);
    disp_text(fb, "XRUN", 120, y, 1, COL_LABEL);
    disp_int(fb, xrun, 148, y, 1, xrun > 1000 ? COL_WARN : COL_VALUE);
    disp_text(fb, "NOTES", 180, y, 1, COL_LABEL);
    disp_int(fb, notes, 212, y, 1, COL_VALUE);
    disp_text(fb, "PART", 235, y, 1, COL_LABEL);
    disp_int(fb, partials, 265, y, 1, partials > 28 ? COL_HILITE : COL_VALUE);

    /* Control state grid - 2 columns */
    y = 50;
    int col1 = 8, col2 = 248, lh = 12;
    uint32_t row_bg_alt = 0xFF0A0A1A;

    /* Render mode */
    disp_fill(fb, 0, y - 2, LCD_W, lh, row_bg_alt);
    disp_text(fb, "A  MODE  ", col1, y, 1, COL_LABEL);
    disp_text(fb, mode_names[render_mode], col1 + 60, y, 1, COL_HILITE);
    disp_text(fb, "B  REVERB", col2, y, 1, COL_LABEL);
    disp_text(fb, reverb_on ? "ON" : "OFF", col2 + 60, y, 1, reverb_on ? COL_ACT : COL_WARN);

    y += lh;
    disp_text(fb, "C-R ANALOG", col1, y, 1, COL_LABEL);
    disp_text(fb, analog_mode_names[analog_mode_idx], col1 + 70, y, 1, COL_HILITE);
    disp_text(fb, "C-U NICE", col2, y, 1, COL_LABEL);
    disp_text(fb, nice_mode_names[nice_mode_idx], col2 + 56, y, 1, COL_HILITE);

    y += lh;
    disp_fill(fb, 0, y - 2, LCD_W, lh, row_bg_alt);
    disp_text(fb, "C-L SRCQ", col1, y, 1, COL_LABEL);
    disp_text(fb, srcq_names[srcq_idx], col1 + 60, y, 1, COL_HILITE);
    disp_text(fb, "C-D MATH", col2, y, 1, COL_LABEL);
    disp_text(fb, math_path_names[g_math_path], col2 + 60, y, 1, COL_HILITE);

    y += lh;
    disp_text(fb, "DU  DAC", col1, y, 1, COL_LABEL);
    disp_text(fb, dac_mode_names[dac_mode_idx], col1 + 60, y, 1, COL_HILITE);
    disp_text(fb, "DD  PCOUNT", col2, y, 1, COL_LABEL);
    disp_text(fb, partial_count_names[partial_count_idx], col2 + 70, y, 1, COL_HILITE);

    y += lh;
    disp_fill(fb, 0, y - 2, LCD_W, lh, row_bg_alt);
    disp_text(fb, "L   GAIN", col1, y, 1, COL_LABEL);
    disp_text(fb, gain_names[gain_idx], col1 + 60, y, 1, COL_HILITE);
    disp_text(fb, "R   REVGAIN", col2, y, 1, COL_LABEL);
    disp_text(fb, gain_names[rev_gain_idx], col2 + 78, y, 1, COL_HILITE);

    y += lh;
    disp_text(fb, "DR  MVOL", col1, y, 1, COL_LABEL);
    disp_text(fb, master_vol_names[master_vol_idx], col1 + 60, y, 1, COL_HILITE);
    disp_text(fb, "Z   SOLO", col2, y, 1, COL_LABEL);
    if (solo_part == -1) disp_text(fb, "OFF", col2 + 60, y, 1, COL_LABEL);
    else if (solo_part == 8) disp_text(fb, "RHYTHM", col2 + 60, y, 1, COL_ACT);
    else { char b[8]="P "; b[1]='1'+solo_part; disp_text(fb, b, col2 + 60, y, 1, COL_ACT); }

    y += lh;
    disp_fill(fb, 0, y - 2, LCD_W, lh, row_bg_alt);
    disp_text(fb, "DL  MUTE", col1, y, 1, COL_LABEL);
    if (mute_part == -1) disp_text(fb, "OFF", col1 + 60, y, 1, COL_LABEL);
    else if (mute_part == 8) disp_text(fb, "RHYTHM", col1 + 60, y, 1, COL_WARN);
    else { char b[8]="P "; b[1]='1'+mute_part; disp_text(fb, b, col1 + 60, y, 1, COL_WARN); }
    disp_text(fb, "STT RESTART", col2, y, 1, COL_LABEL);

    /* Patches list at bottom */
    y = LCD_H - 60;
    disp_text(fb, "ACTIVE PATCHES", 8, y, 1, COL_LABEL);
    y += 10;
    for (uint32_t p = 0; p < 9; p++) {
        if (part_states_for_display & (1u << p)) {
            int px = 8 + (p % 5) * 94;
            int py = y + (p / 5) * 12;
            char pb[4]; pb[0]='P'; pb[1]='1'+p; pb[2]=0;
            disp_text(fb, pb, px, py, 1, COL_LABEL);
            const char *pname = mt32emu_get_patch_name(synth, p);
            disp_text(fb, pname ? pname : "?", px + 16, py, 1, COL_VALUE);
        }
    }

    dcache_clean_fb(FB0_ADDR);
}

/* Draw menu overlay on top of the playback display. */
static void render_menu(void) {
    uint32_t *fb = (uint32_t *)FB0_ADDR;

    /* Semi-opaque dim the whole screen (simple: clear to dark background) */
    int mx = 30, my = 30, mw = LCD_W - 60, mh = LCD_H - 60;
    disp_fill(fb, mx, my, mw, mh, COL_BG);
    disp_fill(fb, mx, my, mw, 2, COL_TITLE);
    disp_fill(fb, mx, my + mh - 2, mw, 2, COL_TITLE);
    disp_fill(fb, mx, my, 2, mh, COL_TITLE);
    disp_fill(fb, mx + mw - 2, my, 2, mh, COL_TITLE);

    int tx = mx + 10, ty = my + 10;

    if (menu_state == MENU_TOP) {
        disp_text(fb, "SELECT  A=PLAY  B=CLOSE", tx, ty, 2, COL_TITLE);
        ty += 26;
        const char *items[MENU_TOP_COUNT] = {
            "Restart current",
            "Monkey Island >",
            "Dune II >",
            "Codec HP vol >"
        };
        for (int i = 0; i < MENU_TOP_COUNT; i++) {
            uint32_t c = (i == menu_sel_top) ? COL_HILITE : COL_VALUE;
            if (i == menu_sel_top) disp_text(fb, ">", tx, ty, 2, COL_HILITE);
            disp_text(fb, items[i], tx + 22, ty, 2, c);
            ty += 22;
        }
    }
    else if (menu_state == MENU_CODEC) {
        disp_text(fb, "CODEC  HP OUTPUT GAIN", tx, ty, 2, COL_TITLE);
        ty += 24;
        disp_text(fb, "A = apply, B = back", tx, ty, 1, COL_LABEL);
        ty += 14;
        for (uint32_t i = 0; i < HP_VOL_COUNT; i++) {
            uint32_t c = ((int)i == menu_sel_codec) ? COL_HILITE : COL_VALUE;
            if ((int)i == menu_sel_codec) disp_text(fb, ">", tx, ty, 2, COL_HILITE);
            disp_text(fb, hp_vol_labels[i], tx + 22, ty, 2, c);
            ty += 22;
        }
    } else if (menu_state == MENU_MI) {
        disp_text(fb, "MONKEY ISLAND  SECTIONS", tx, ty, 2, COL_TITLE);
        ty += 24;
        for (int i = 0; i < MI_SECTION_COUNT; i++) {
            uint32_t c = (i == menu_sel_mi) ? COL_HILITE : COL_VALUE;
            if (i == menu_sel_mi) disp_text(fb, ">", tx, ty, 1, COL_HILITE);
            disp_text(fb, MI_SECTIONS[i].title, tx + 12, ty, 1, c);
            ty += 12;
        }
    } else if (menu_state == MENU_DUNE) {
        disp_text(fb, "DUNE II  TRACKS", tx, ty, 2, COL_TITLE);
        ty += 22;
        /* Two-column flat list with scroll; show up to 24 rows across 2 cols */
        int rows = 16;
        int cols = 2;
        int perpage = rows * cols;
        if (menu_sel_dune < menu_scroll_dune) menu_scroll_dune = menu_sel_dune;
        if (menu_sel_dune >= menu_scroll_dune + perpage)
            menu_scroll_dune = menu_sel_dune - perpage + 1;
        for (int c = 0; c < cols; c++) {
            for (int r = 0; r < rows; r++) {
                int i = menu_scroll_dune + c * rows + r;
                if (i >= dune_count) break;
                int cx = tx + c * (mw / 2 - 10);
                int cy = ty + r * 12;
                uint32_t col = (i == menu_sel_dune) ? COL_HILITE : COL_VALUE;
                if (i == menu_sel_dune) disp_text(fb, ">", cx, cy, 1, COL_HILITE);
                disp_text(fb, MIDI_CATALOG[dune_indices[i]].title, cx + 12, cy, 1, col);
            }
        }
        disp_text(fb, "D-PAD L/R  PAGE", tx, my + mh - 20, 1, COL_LABEL);
    }

    dcache_clean_fb(FB0_ADDR);
}

void main(void)
{
    uart_puts("\n\n=== MT-32 Real-Time MIDI Player ===\n");
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();
    video_init();

    /* Clear framebuffers + overlay so LCD starts black */
    memset32_neon(FB0_ADDR, 0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    /* CPU clock report */
    {
        uint32_t pll = *(volatile uint32_t *)0x01C20000;
        uint32_t n = ((pll >> 8) & 0x1F) + 1;
        uint32_t k = ((pll >> 4) & 0x3) + 1;
        uint32_t m = (pll & 0x3) + 1;
        uart_puts("[cpu] "); uart_putdec(24 * n * k / m); uart_puts(" MHz\n");
    }

    /* Read CPACR + FPEXC to verify NEON full-32-reg + Advanced SIMD enabled.
     * CPACR bit 30 (D32DIS) should be 0 for d16-d31 access.
     * CPACR bit 31 (ASEDIS) should be 0 for Advanced SIMD (NEON). */
    {
        uint32_t cpacr, fpexc;
        __asm__ volatile("mrc p15, 0, %0, c1, c0, 2" : "=r"(cpacr));
        __asm__ volatile("vmrs %0, fpexc" : "=r"(fpexc));
        uart_puts("[fpu] CPACR=0x"); uart_puthex(cpacr);
        uart_puts(" D32DIS="); uart_putdec((cpacr >> 30) & 1);
        uart_puts(" ASEDIS="); uart_putdec((cpacr >> 31) & 1);
        uart_puts(" CP10="); uart_putdec((cpacr >> 20) & 3);
        uart_puts(" CP11="); uart_putdec((cpacr >> 22) & 3);
        uart_puts(" FPEXC=0x"); uart_puthex(fpexc);
        uart_puts("\n");
    }

    audio_init();

    /* ---- Create MT-32 synth ---- */
    uart_puts("[mt32] creating context...\n");
    synth = mt32emu_create_context((mt32emu_report_handler_i){0}, NULL);
    if (!synth) {
        uart_puts("[mt32] create_context failed!\n");
        while (1);
    }

    uint32_t ctrl_size = (uint32_t)(_binary_examples_mt32_rt_roms_MT32_CONTROL_ROM_end -
                                     _binary_examples_mt32_rt_roms_MT32_CONTROL_ROM_start);
    uint32_t pcm_size  = (uint32_t)(_binary_examples_mt32_rt_roms_MT32_PCM_ROM_end -
                                     _binary_examples_mt32_rt_roms_MT32_PCM_ROM_start);
    uart_puts("[mt32] Control ROM: "); uart_putdec(ctrl_size);
    uart_puts(" bytes, PCM ROM: "); uart_putdec(pcm_size); uart_puts(" bytes\n");

    mt32emu_return_code rc;
    rc = mt32emu_add_rom_data(synth,
        _binary_examples_mt32_rt_roms_MT32_CONTROL_ROM_start, ctrl_size, NULL);
    uart_puts("[mt32] add Control ROM: rc="); uart_putdec((uint32_t)rc); uart_puts("\n");
    rc = mt32emu_add_rom_data(synth,
        _binary_examples_mt32_rt_roms_MT32_PCM_ROM_start, pcm_size, NULL);
    uart_puts("[mt32] add PCM ROM: rc="); uart_putdec((uint32_t)rc); uart_puts("\n");

    mt32emu_set_analog_output_mode(synth, analog_modes[analog_mode_idx]);
    mt32emu_select_renderer_type(synth, MT32EMU_RT_BIT16S);
    mt32emu_set_samplerate_conversion_quality(synth, srcq_modes[srcq_idx]);

    /* Request 48 kHz output (matches ACCURATE mode's native upsampling). */
    mt32emu_set_stereo_output_samplerate(synth, (double)OUT_RATE);

    rc = mt32emu_open_synth(synth);
    uart_puts("[mt32] open_synth: rc="); uart_putdec((uint32_t)rc); uart_puts("\n");
    if ((int32_t)rc < 0) {
        uart_puts("[mt32] open failed!\n");
        while (1);
    }

    uint32_t actual_rate = mt32emu_get_actual_stereo_output_samplerate(synth);
    uart_puts("[mt32] actual output rate: "); uart_putdec(actual_rate); uart_puts(" Hz\n");

    extern void mt32_dump_lut(void);
    mt32_dump_lut();

    /* Save heap position AFTER first successful open so subsequent reopens
     * can rewind and reclaim munt's internal allocations. */
    heap_checkpoint = heap_used();
    uart_puts("[mt32] heap checkpoint: "); uart_putdec(heap_checkpoint / 1024);
    uart_puts(" KB (reopens rewind to here)\n");

    /* ---- Math function sanity: compare neon asm vs pure-C for known inputs ---- */
    uart_puts("\n[math] validate neon asm vs libc reference:\n");
    {
        const float inputs[] = { 0.5f, 1.0f, 2.0f, 10.0f, 100.0f, 0.001f };
        for (uint32_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
            float x = inputs[i];
            float pc = powf_c(x, 2.0f);
            float pn = powf_neon_hfp(x, 2.0f);
            float ec = expf_c(x);
            float en = expf_neon_hfp(x);
            float lc = (x > 0) ? logf_c(x) : 0;
            float ln = (x > 0) ? logf_neon_hfp(x) : 0;
            int pc_i = (int)(pc * 1000);
            int pn_i = (int)(pn * 1000);
            int ec_i = (int)(ec * 1000);
            int en_i = (int)(en * 1000);
            int lc_i = (int)(lc * 1000);
            int ln_i = (int)(ln * 1000);
            uart_puts("  x="); uart_putdec((uint32_t)(x*1000));
            uart_puts(" pow_c="); uart_putdec((uint32_t)(pc_i<0?-pc_i:pc_i));
            uart_puts(" pow_n="); uart_putdec((uint32_t)(pn_i<0?-pn_i:pn_i));
            uart_puts(" exp_c="); uart_putdec((uint32_t)(ec_i<0?-ec_i:ec_i));
            uart_puts(" exp_n="); uart_putdec((uint32_t)(en_i<0?-en_i:en_i));
            uart_puts(" log_c="); uart_putdec((uint32_t)(lc_i<0?-lc_i:lc_i));
            uart_puts(" log_n="); uart_putdec((uint32_t)(ln_i<0?-ln_i:ln_i));
            uart_puts("\n");
        }
    }

    /* ---- Load initial song from catalog (MI Introduction by default) ---- */
    build_dune_list();
    uart_puts("[catalog] "); uart_putdec(MIDI_COUNT);
    uart_puts(" songs, dune="); uart_putdec(dune_count);
    uart_puts(" mi_sections="); uart_putdec(MI_SECTION_COUNT); uart_puts("\n");

    load_and_start_song(default_midi_idx(), 0);
    uart_puts("[smf] format="); uart_putdec(smf.format);
    uart_puts(" tracks="); uart_putdec(smf.num_tracks);
    uart_puts(" div="); uart_putdec(smf.division); uart_puts("\n");

    /* ---- Pre-fill audio buffer then start DMA ---- */

    for (int f = 0; f < 3; f++) {
        dispatch_midi_for_samples(FRAMES_60);
        render_frame(FRAMES_60);
    }
    audio_start();

    input_init(INPUT_N64);
    uart_puts("[main] controls:\n");
    uart_puts("  A: cycle render mode (FULL/DRY/REVDRY/REVWET/NO-REVERB)\n");
    uart_puts("  B: toggle reverb enabled\n");
    uart_puts("  Start: restart song\n");
    uart_puts("  C-Right: cycle analog output (DIGITAL_ONLY/COARSE/ACCURATE/OVERSAMPLED)\n");
    uart_puts("  C-Up: cycle Nice modes (none/amp/amp+pan/amp+mix/all)\n");
    uart_puts("  C-Left: cycle SRCQ (FASTEST/FAST/GOOD/BEST)\n");
    uart_puts("  C-Down: cycle math path (C-poly / NEON-asm / libm)\n");
    uart_puts("  D-Up:   cycle DAC input mode (NICE/PURE/GEN1/GEN2)\n");
    uart_puts("  D-Down: cycle partial count (32/24/16/8)\n");
    uart_puts("  L:      cycle output gain (0.25..2.0)\n");
    uart_puts("  R:      cycle reverb output gain (0.25..2.0)\n");
    uart_puts("  Z:      cycle solo part (OFF / parts 1-8 / rhythm)\n");
    uart_puts("  D-Left: cycle MUTE-one part (OFF / parts 1-8 / rhythm)\n");
    uart_puts("  D-Right: cycle master volume (25/40/50/60/75/100)\n\n");
    uart_puts("[main] playing...\n\n");

    uint32_t frame = 0;
    uint64_t pmu_total = 0;
    uint32_t pmu_frames = 0;
    int reverb_enabled = 1;

    while (1) {
        uint32_t t0 = timer_read();

        /* Input */
        input_poll();
        uint32_t pressed = input_pressed();

        /* ---- Menu navigation (consumes A/B/D-pad/Start when open) ---- */
        if (menu_state != MENU_NONE) {
            if (pressed & BTN_START) { menu_state = MENU_NONE; }
            else if (pressed & BTN_B) {
                if (menu_state == MENU_TOP) menu_state = MENU_NONE;
                else menu_state = MENU_TOP;
            }
            else if (menu_state == MENU_TOP) {
                if (pressed & BTN_UP)   menu_sel_top = (menu_sel_top + MENU_TOP_COUNT - 1) % MENU_TOP_COUNT;
                if (pressed & BTN_DOWN) menu_sel_top = (menu_sel_top + 1) % MENU_TOP_COUNT;
                if (pressed & BTN_A) {
                    if (menu_sel_top == 0) {
                        if (current_midi_idx >= 0)
                            load_and_start_song(current_midi_idx, current_section_idx);
                        menu_state = MENU_NONE;
                    } else if (menu_sel_top == 1) {
                        menu_state = MENU_MI;
                    } else if (menu_sel_top == 2) {
                        menu_state = MENU_DUNE;
                    } else if (menu_sel_top == 3) {
                        menu_state = MENU_CODEC;
                    }
                }
            }
            else if (menu_state == MENU_CODEC) {
                if (pressed & BTN_UP)   menu_sel_codec = (menu_sel_codec + HP_VOL_COUNT - 1) % HP_VOL_COUNT;
                if (pressed & BTN_DOWN) menu_sel_codec = (menu_sel_codec + 1) % HP_VOL_COUNT;
                if (pressed & BTN_A) {
                    audio_set_hp_vol(hp_vol_choices[menu_sel_codec]);
                    uart_puts("[codec] HP_VOL="); uart_puts(hp_vol_labels[menu_sel_codec]); uart_puts("\n");
                    menu_state = MENU_NONE;
                }
            }
            else if (menu_state == MENU_MI) {
                if (pressed & BTN_UP)   menu_sel_mi = (menu_sel_mi + MI_SECTION_COUNT - 1) % MI_SECTION_COUNT;
                if (pressed & BTN_DOWN) menu_sel_mi = (menu_sel_mi + 1) % MI_SECTION_COUNT;
                if (pressed & BTN_A) {
                    int mi_idx = default_midi_idx();
                    load_and_start_song(mi_idx, menu_sel_mi);
                    menu_state = MENU_NONE;
                }
            }
            else if (menu_state == MENU_DUNE) {
                if (dune_count > 0) {
                    if (pressed & BTN_UP)   menu_sel_dune = (menu_sel_dune + dune_count - 1) % dune_count;
                    if (pressed & BTN_DOWN) menu_sel_dune = (menu_sel_dune + 1) % dune_count;
                    if (pressed & BTN_LEFT)  menu_sel_dune = (menu_sel_dune + dune_count - 8) % dune_count;
                    if (pressed & BTN_RIGHT) menu_sel_dune = (menu_sel_dune + 8) % dune_count;
                    if (pressed & BTN_A) {
                        load_and_start_song(dune_indices[menu_sel_dune], 0);
                        menu_state = MENU_NONE;
                    }
                }
            }
            /* menu is open — skip playback-control buttons that share A/B/D-pad/Start */
            pressed &= ~(BTN_A | BTN_B | BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_START);
        } else {
            /* Start opens the menu when not in menu */
            if (pressed & BTN_START) {
                menu_state = MENU_TOP;
                pressed &= ~BTN_START;
            }
        }

        if (pressed & BTN_A) {
            render_mode = (render_mode + 1) % MODE_COUNT;
            uart_puts("[mode] "); uart_puts(mode_names[render_mode]); uart_puts("\n");
        }
        if (pressed & BTN_B) {
            reverb_enabled = !reverb_enabled;
            mt32emu_set_reverb_enabled(synth, reverb_enabled ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE);
            uart_puts("[reverb] "); uart_puts(reverb_enabled ? "ON\n" : "OFF\n");
        }
        if (pressed & BTN_C_RIGHT) {
            analog_mode_idx = (analog_mode_idx + 1) % NUM_ANALOG_MODES;
            reopen_synth_with_mode();
        }
        if (pressed & BTN_C_UP) {
            nice_mode_idx = (nice_mode_idx + 1) % NICE_COUNT;
            reopen_synth_with_mode();
        }
        if (pressed & BTN_C_LEFT) {
            srcq_idx = (srcq_idx + 1) % NUM_SRCQ;
            reopen_synth_with_mode();
        }
        if (pressed & BTN_C_DOWN) {
            g_math_path = (g_math_path + 1) % NUM_MATH_PATHS;
            reopen_synth_with_mode();  /* forces LUT rebuild with new math */
        }
        if (pressed & BTN_UP) {
            dac_mode_idx = (dac_mode_idx + 1) % NUM_DAC_MODES;
            reopen_synth_with_mode();
        }
        if (pressed & BTN_DOWN) {
            partial_count_idx = (partial_count_idx + 1) % NUM_PARTIAL_COUNTS;
            reopen_synth_with_mode();
        }
        if (pressed & BTN_L) {
            gain_idx = (gain_idx + 1) % NUM_GAINS;
            reopen_synth_with_mode();
        }
        if (pressed & BTN_R) {
            rev_gain_idx = (rev_gain_idx + 1) % NUM_GAINS;
            reopen_synth_with_mode();
        }
        if (pressed & BTN_Z) {
            /* Cycle solo: -1 (off) → 0 → 1 → ... → 8 → -1 */
            solo_part = (solo_part == 8) ? -1 : (solo_part + 1);
            mute_part = -1;  /* exclusive with mute */
            apply_solo();
            uart_puts("[solo] ");
            if (solo_part == -1) uart_puts("OFF (all parts)");
            else if (solo_part == 8) uart_puts("RHYTHM (part 9)");
            else { uart_puts("part "); uart_putdec(solo_part + 1); }
            uart_puts("\n");
        }
        if (pressed & BTN_LEFT) {
            /* Cycle mute-one (anti-solo): -1 (off) → 0 → 1 → ... → 8 → -1 */
            mute_part = (mute_part == 8) ? -1 : (mute_part + 1);
            solo_part = -1;  /* exclusive with solo */
            apply_solo();
            uart_puts("[mute] ");
            if (mute_part == -1) uart_puts("OFF (all parts)");
            else if (mute_part == 8) uart_puts("RHYTHM (part 9)");
            else { uart_puts("part "); uart_putdec(mute_part + 1); }
            uart_puts("\n");
        }
        if (pressed & BTN_RIGHT) {
            master_vol_idx = (master_vol_idx + 1) % NUM_MASTER_VOLS;
            mt32emu_set_master_volume_override(synth, master_vols[master_vol_idx]);
            uart_puts("[master_vol] ");
            uart_puts(master_vol_names[master_vol_idx]);
            uart_puts("\n");
        }

        /* Menu overlay — repaint every frame while open so cursor is responsive */
        if (menu_state != MENU_NONE) render_menu();

        uint32_t c0 = pmu_cycles();

        dispatch_midi_for_samples(FRAMES_60);
        render_frame(FRAMES_60);

        uint32_t c1 = pmu_cycles();
        pmu_total += (c1 - c0);
        pmu_frames++;

        /* Arm capture at frame 1200 (~20s) during heavy polyphony — at that
         * point stats showed notes=14, partials=32, which is where you hear
         * the fuzz. At frame 180 only 2 parts played = not representative. */
        if (!dump_capture_armed && !dump_done && frame == 1200) {
            dump_capture_armed = 1;
            dump_start_tick = smf.current_tick;
            dump_sample_count = 0;
            /* Use total_samples (global counter) as the audio-time start index */
            uart_puts("\n[dump] ARMED at frame 180, smf_tick=");
            uart_putdec(smf.current_tick);
            uart_puts(" total_samples=");
            uart_putdec((uint32_t)total_samples);
            uart_puts("\n");
        }
        if (dump_capture_armed && !dump_done && dump_sample_count >= DUMP_SAMPLES) {
            dump_samples_hex();
            dump_done = 1;
        }

        /* Pace to 60fps; keep DMA fed during wait. Also sample buffer depth
         * to detect brief underrun dips that happen between 1-second stat prints. */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) {
            audio_update();
            uint32_t depth = mix_wr - mix_rd;
            if (depth < dbg_buf_min) dbg_buf_min = depth;
            if (depth > dbg_buf_max) dbg_buf_max = depth;
            dbg_buf_samples++;
        }

        /* Loop song if done */
        if (smf_all_done(&smf)) {
            uart_puts("[smf] end of song, restarting\n");
            if (current_midi_idx >= 0)
                load_and_start_song(current_midi_idx, current_section_idx);
        }

        /* Stats every 60 frames (~1s) */
        if (((frame + 1) % 60) == 0) {
            uint32_t fill = mix_wr - mix_rd;
            uint32_t avg = (uint32_t)(pmu_total / pmu_frames);
            /* CPU clock from PLL readback */
            uint32_t pll_reg = *(volatile uint32_t *)0x01C20000;
            uint32_t n = ((pll_reg >> 8) & 0x1F) + 1;
            uint32_t k = ((pll_reg >> 4) & 0x3) + 1;
            uint32_t m = (pll_reg & 0x3) + 1;
            uint32_t cpu_hz = 24000000 * n * k / m;
            uint32_t budget = cpu_hz / 60;
            uint32_t pct = (uint32_t)((uint64_t)avg * 100 / budget);

            /* Runtime info from munt */
            uint32_t partial_count = mt32emu_get_partial_count(synth);
            uint32_t part_states = mt32emu_get_part_states(synth);
            int reverb_on = mt32emu_is_reverb_enabled(synth) == MT32EMU_BOOL_TRUE;

            /* Count playing notes across all 9 parts + gather patch names */
            uint32_t total_notes = 0;
            uint8_t keys[32], vels[32];
            uint32_t notes_per_part[9] = {0};
            for (uint32_t p = 0; p < 9; p++) {
                notes_per_part[p] = mt32emu_get_playing_notes(synth, p, keys, vels);
                total_notes += notes_per_part[p];
            }

            uart_puts("\n========================================\n");
            uart_puts("t="); uart_putdec(frame / 60);
            uart_puts("s cpu="); uart_putdec(pct); uart_puts("%");
            uart_puts(" buf="); uart_putdec(fill);
            uart_puts(" xrun="); uart_putdec(audio_underruns);
            uart_puts(" notes="); uart_putdec(total_notes);
            uart_puts("/"); uart_putdec(partial_count); uart_puts("p");
            uart_puts("\n");
            /* Full state on one line so nothing gets lost */
            uart_puts("[STATE] ");
            uart_puts("mode="); uart_puts(mode_names[render_mode]);
            uart_puts(" rev="); uart_puts(reverb_on ? "on" : "OFF");
            uart_puts(" analog="); uart_puts(analog_mode_names[analog_mode_idx]);
            uart_puts(" nice="); uart_puts(nice_mode_names[nice_mode_idx]);
            uart_puts(" srcq="); uart_puts(srcq_names[srcq_idx]);
            uart_puts(" math="); uart_puts(math_path_names[g_math_path]);
            uart_puts(" dac="); uart_puts(dac_mode_names[dac_mode_idx]);
            uart_puts(" gain="); uart_puts(gain_names[gain_idx]);
            uart_puts(" rev_gain="); uart_puts(gain_names[rev_gain_idx]);
            uart_puts(" mvol="); uart_puts(master_vol_names[master_vol_idx]);
            uart_puts(" solo=");
            if (solo_part == -1) uart_puts("OFF"); else { uart_putdec(solo_part + 1); }
            uart_puts(" mute=");
            if (mute_part == -1) uart_puts("OFF"); else { uart_putdec(mute_part + 1); }
            uart_puts("\n");

            /* Paint on-screen state (only when menu is closed) */
            part_states_for_display = part_states;
            if (menu_state == MENU_NONE) {
                render_display(frame / 60, pct, fill, audio_underruns,
                               total_notes, partial_count, reverb_on);
            }

            /* Per-stream peaks — KEY: shows which stream contributes fuzz.
             * If REV-WET has huge jumps when DRY is small, reverb is the culprit. */
            uart_puts("  stream peaks (abs L):  NR="); uart_putdec(dbg_nr_peak);
            uart_puts(" RD="); uart_putdec(dbg_rd_peak);
            uart_puts(" RW="); uart_putdec(dbg_rw_peak);
            uart_puts("   max-jumps:  NR="); uart_putdec(dbg_nr_jump);
            uart_puts(" RD="); uart_putdec(dbg_rd_jump);
            uart_puts(" RW="); uart_putdec(dbg_rw_jump);
            uart_puts("\n");
            uart_puts("  stream active samples: NR="); uart_putdec(dbg_nr_nonzero);
            uart_puts(" RD="); uart_putdec(dbg_rd_nonzero);
            uart_puts(" RW="); uart_putdec(dbg_rw_nonzero);
            uart_puts("\n");

            /* Combined output stats */
            int32_t mean_l = dbg_samples ? (int32_t)(dbg_sum_l / dbg_samples) : 0;
            int32_t mean_r = dbg_samples ? (int32_t)(dbg_sum_r / dbg_samples) : 0;
            uart_puts("  combined L: min="); uart_putdec((uint32_t)(dbg_min_l < 0 ? -dbg_min_l : dbg_min_l));
            uart_puts((dbg_min_l < 0) ? "(-) " : " ");
            uart_puts("max="); uart_putdec((uint32_t)dbg_max_l);
            uart_puts(" dc="); uart_putdec((uint32_t)(mean_l < 0 ? -mean_l : mean_l));
            uart_puts(" clip="); uart_putdec(dbg_clip_l);
            uart_puts(" jump="); uart_putdec(dbg_max_jump_l);
            uart_puts(" bigjmps="); uart_putdec(dbg_big_jumps);
            uart_puts("\n");
            (void)mean_r;
            uart_puts("  heap="); uart_putdec(heap_used() / 1024);
            uart_puts("KB bufrange="); uart_putdec(dbg_buf_min);
            uart_puts(".."); uart_putdec(dbg_buf_max);
            uart_puts("\n");

            /* Patch names for active parts — shows which instruments are playing */
            uart_puts("  patches: ");
            for (uint32_t p = 0; p < 9; p++) {
                if (notes_per_part[p] > 0 || (part_states & (1u << p))) {
                    const char *name = mt32emu_get_patch_name(synth, p);
                    uart_puts("p"); uart_putdec(p);
                    uart_puts("("); uart_putdec(notes_per_part[p]); uart_puts(")=");
                    if (name) uart_puts(name); else uart_puts("?");
                    uart_puts(" ");
                }
            }
            uart_puts("\n");

            pmu_total = 0;
            pmu_frames = 0;
            dbg_min_l = dbg_min_r = 0;
            dbg_max_l = dbg_max_r = 0;
            dbg_clip_l = dbg_clip_r = 0;
            dbg_sum_l = dbg_sum_r = 0;
            dbg_samples = 0;
            dbg_max_jump_l = dbg_max_jump_r = 0;
            dbg_big_jumps = 0;
            dbg_buf_min = 0xFFFFFFFF;
            dbg_buf_max = 0;
            dbg_buf_samples = 0;
            dbg_nr_peak = dbg_rd_peak = dbg_rw_peak = 0;
            dbg_nr_jump = dbg_rd_jump = dbg_rw_jump = 0;
            dbg_nr_nonzero = dbg_rd_nonzero = dbg_rw_nonzero = 0;
        }
        frame++;
    }
}
