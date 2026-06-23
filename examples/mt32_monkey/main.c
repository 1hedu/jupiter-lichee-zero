/*
 * Jupiter SDK — MT-32 Real-Time Playback (Monkey Island Theme)
 *
 * Streams a Standard MIDI File (SMF) into Munt's MT-32 emulator in
 * real time with NEON-accelerated math. Multi-track SMF parser merges
 * up to 16 tracks by always advancing the track with the earliest
 * next event. Audio pumped into mix_buf at 48 kHz.
 *
 * Build: make GAME=examples/mt32_warcraft/main.c
 */
#include "jupiter.h"
#include "c_interface/c_interface.h"
#include "mt32_control_rom.h"
#include "mt32_pcm_rom.h"
#include "monkey_theme.h"

/* ================================================================
 *  SMF parser — format 0/1, multi-track merge, running status,
 *  tempo, sysex pass-through
 * ================================================================ */

#define MAX_TRACKS 16

typedef struct {
    const uint8_t *cursor;
    const uint8_t *trk_end;
    uint8_t  running_status;
    uint64_t next_event_us;
    uint8_t  done;
} smf_track_t;

typedef struct {
    const uint8_t *data;
    uint32_t size;

    uint16_t format;
    uint16_t ntrks;
    uint16_t division;

    smf_track_t tracks[MAX_TRACKS];
    uint32_t num_tracks;

    uint32_t us_per_tick;
    uint32_t us_per_quarter;

    uint8_t  done;
} smf_t;

static inline uint16_t be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  | p[3];
}

/* Variable-length quantity: 7 bits per byte, high bit = continue. */
static inline uint32_t read_vlq(const uint8_t **p)
{
    uint32_t v = 0;
    uint8_t b;
    do {
        b = **p;
        (*p)++;
        v = (v << 7) | (b & 0x7F);
    } while (b & 0x80);
    return v;
}

static int smf_init(smf_t *s, const uint8_t *data, uint32_t size)
{
    if (size < 14) return -1;
    if (data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd')
        return -1;

    uint32_t hdr_len = be32(data + 4);
    s->data     = data;
    s->size     = size;
    s->format   = be16(data + 8);
    s->ntrks    = be16(data + 10);
    s->division = be16(data + 12);
    s->done     = 0;

    /* Default tempo */
    s->us_per_quarter = 500000;
    uint16_t div = s->division ? s->division : 96;
    s->us_per_tick = s->us_per_quarter / div;

    /* Find all MTrk chunks */
    const uint8_t *p = data + 8 + hdr_len;
    const uint8_t *end = data + size;
    s->num_tracks = 0;
    while (p + 8 <= end && s->num_tracks < MAX_TRACKS) {
        uint32_t clen = be32(p + 4);
        if (p[0] == 'M' && p[1] == 'T' && p[2] == 'r' && p[3] == 'k') {
            smf_track_t *t = &s->tracks[s->num_tracks];
            t->cursor = p + 8;
            t->trk_end = p + 8 + clen;
            if (t->trk_end > end) t->trk_end = end;
            t->running_status = 0;
            t->done = 0;
            /* Read initial delta */
            const uint8_t *cp = t->cursor;
            uint32_t delta = read_vlq(&cp);
            t->cursor = cp;
            t->next_event_us = (uint64_t)delta * s->us_per_tick;
            s->num_tracks++;
        }
        p += 8 + clen;
    }
    if (s->num_tracks == 0) return -2;
    return 0;
}

/* Parse and dispatch one event from a single track. Returns 1 if event
 * consumed, 0 if end-of-track. */
static int smf_step_track(smf_t *s, smf_track_t *t, mt32emu_const_context ctx)
{
    if (t->done || t->cursor >= t->trk_end) { t->done = 1; return 0; }

    const uint8_t *p = t->cursor;
    uint8_t status = *p;
    if (status & 0x80) p++;
    else status = t->running_status;

    if (status == 0xFF) {
        uint8_t type = *p++;
        uint32_t len = read_vlq(&p);
        if (type == 0x51 && len == 3) {
            s->us_per_quarter = ((uint32_t)p[0] << 16)
                              | ((uint32_t)p[1] << 8)
                              |  (uint32_t)p[2];
            uint16_t div = s->division ? s->division : 96;
            s->us_per_tick = s->us_per_quarter / div;
        } else if (type == 0x2F) {
            t->done = 1;
            return 0;
        }
        p += len;
    } else if (status == 0xF0 || status == 0xF7) {
        uint32_t len = read_vlq(&p);
        static uint8_t sysex_buf[2048];
        if (status == 0xF0 && len + 1 <= sizeof(sysex_buf)) {
            sysex_buf[0] = 0xF0;
            for (uint32_t i = 0; i < len; i++) sysex_buf[1 + i] = p[i];
            mt32emu_play_sysex(ctx, sysex_buf, len + 1);
        } else if (status == 0xF7 && len <= sizeof(sysex_buf)) {
            for (uint32_t i = 0; i < len; i++) sysex_buf[i] = p[i];
            mt32emu_play_sysex(ctx, sysex_buf, len);
        }
        p += len;
    } else {
        uint8_t top = status & 0xF0;
        uint8_t d1 = *p++;
        uint8_t d2 = 0;
        if (top != 0xC0 && top != 0xD0) d2 = *p++;
        t->running_status = status;
        uint32_t msg = (uint32_t)status | ((uint32_t)d1 << 8)
                     | ((uint32_t)d2 << 16);
        mt32emu_play_msg(ctx, msg);
    }

    t->cursor = p;
    if (t->cursor >= t->trk_end) { t->done = 1; return 0; }
    uint32_t delta = read_vlq(&t->cursor);
    t->next_event_us += (uint64_t)delta * s->us_per_tick;
    return 1;
}

/* Advance all tracks: always process the track with the earliest next event. */
static void smf_advance(smf_t *s, uint64_t cur_us, mt32emu_const_context ctx)
{
    for (;;) {
        /* Find the track with the smallest next_event_us */
        int best = -1;
        uint64_t best_us = ~0ULL;
        for (uint32_t i = 0; i < s->num_tracks; i++) {
            if (!s->tracks[i].done && s->tracks[i].next_event_us < best_us) {
                best_us = s->tracks[i].next_event_us;
                best = (int)i;
            }
        }
        if (best < 0 || best_us > cur_us) break;
        if (!smf_step_track(s, &s->tracks[best], ctx)) continue;
    }
    /* Check if all tracks are done */
    s->done = 1;
    for (uint32_t i = 0; i < s->num_tracks; i++) {
        if (!s->tracks[i].done) { s->done = 0; break; }
    }
}

/* ================================================================
 *  Audio pipeline — pump mt32emu output into mix_buf
 * ================================================================ */

#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;

#define CHUNK_FRAMES 128
static int16_t chunk[CHUNK_FRAMES * 2];

#define OUTPUT_RATE 48000
#define TARGET_DEPTH 2400   /* ~25 ms of stereo 48 kHz */

static mt32emu_context ctx;

static uint64_t total_frames_rendered;

static void mt32_fill_one_chunk(void)
{
    mt32emu_render_bit16s(ctx, chunk, CHUNK_FRAMES);
    for (uint32_t i = 0; i < CHUNK_FRAMES; i++) {
        if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2)) break;
        mix_buf[mix_wr & MIX_BUF_MASK] = chunk[i * 2];     mix_wr++;
        mix_buf[mix_wr & MIX_BUF_MASK] = chunk[i * 2 + 1]; mix_wr++;
    }
    total_frames_rendered += CHUNK_FRAMES;
}

/* ================================================================
 *  MAIN
 * ================================================================ */

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();

    uart_puts("\n\n=== Jupiter SDK — MT-32 Real-Time (NEON) ===\n");
    uart_puts("Monkey Island Theme streaming into NEON-accelerated LA synthesis.\n\n");

    cpp_init();
    audio_init();

    /* ---- MT-32 bring-up ---- */
    mt32emu_report_handler_i null_report = { 0 };
    ctx = mt32emu_create_context(null_report, 0);
    if (!ctx) { uart_puts("create_context FAIL\n"); while (1); }

    if (mt32emu_add_rom_data(ctx, mt32_control_rom, mt32_control_rom_len, 0)
            != MT32EMU_RC_ADDED_CONTROL_ROM) {
        uart_puts("control ROM FAIL\n"); while (1);
    }
    if (mt32emu_add_rom_data(ctx, mt32_pcm_rom, mt32_pcm_rom_len, 0)
            != MT32EMU_RC_ADDED_PCM_ROM) {
        uart_puts("pcm ROM FAIL\n"); while (1);
    }
    mt32emu_set_stereo_output_samplerate(ctx, (double)OUTPUT_RATE);
    if (mt32emu_open_synth(ctx) != MT32EMU_RC_OK) {
        uart_puts("open_synth FAIL\n"); while (1);
    }
    mt32emu_set_output_gain(ctx, 2.0f);

    uart_puts("Synth open, rate=");
    uart_putdec(mt32emu_get_actual_stereo_output_samplerate(ctx));
    uart_puts(" Hz\n");

    /* ---- SMF parse ---- */
    smf_t smf;
    if (smf_init(&smf, smf_monkey_theme, smf_monkey_theme_len) != 0) {
        uart_puts("smf_init FAIL\n"); while (1);
    }
    uart_puts("SMF: format="); uart_putdec(smf.format);
    uart_puts(" tracks="); uart_putdec(smf.num_tracks);
    uart_puts(" division="); uart_putdec(smf.division);
    uart_puts("\n");

    /* ---- Audio pipeline up ---- */
    audio_set_rate(OUTPUT_RATE);

    /* Pre-fill silence so DMA has something to chew while we start. */
    for (int i = 0; i < 16; i++) mt32_fill_one_chunk();
    audio_start();
    irq_global_enable();

    uart_puts("\nPlaying...\n");

    /* ---- Main loop: pump audio, dispatch MIDI events ---- */
    uint64_t last_print_us = 0;

    while (1) {
        /* Render audio until the ring is at target depth */
        while ((mix_wr - mix_rd) < TARGET_DEPTH) {
            mt32_fill_one_chunk();
        }

        /* Current time in microseconds, derived from samples rendered */
        uint64_t cur_us = (total_frames_rendered * 1000000ULL) / OUTPUT_RATE;

        /* Advance the MIDI stream */
        smf_advance(&smf, cur_us, ctx);

        /* Once per second, print stats */
        if (cur_us - last_print_us >= 1000000) {
            uart_puts("t=");  uart_putdec((uint32_t)(cur_us / 1000000));
            uart_puts("s fill="); uart_putdec(mix_wr - mix_rd);
            uart_puts(" bpm="); uart_putdec(60000000 / smf.us_per_quarter);
            if (smf.done) uart_puts(" (EOT)");
            uart_puts("\n");
            last_print_us = cur_us;
        }

        /* Loop the track when it ends */
        if (smf.done) {
            /* Panic: all notes off on all channels */
            for (int ch = 0; ch < 16; ch++) {
                mt32emu_play_msg(ctx, 0xB0 | ch | (0x7B << 8));
            }
            /* Re-init parser from the start */
            uint64_t base_us = cur_us;
            smf_init(&smf, smf_monkey_theme, smf_monkey_theme_len);
            /* Offset all track times to the current audio clock */
            for (uint32_t i = 0; i < smf.num_tracks; i++)
                smf.tracks[i].next_event_us += base_us;
        }
    }
}
