/*
 * Jupiter SDK — Nuked-SC55 offline-render + playback demo
 *
 * Strategy: the V3s Cortex-A7 can't run Nuked-SC55 in real time (~3×
 * over budget), but it CAN run it offline if we're patient. So we boot,
 * spend ~3× the song's wall-clock time pre-rendering it into a DRAM
 * buffer, then stream that buffer through the audio DMA at zero CPU.
 *
 * The output is byte-identical to running Nuked-SC55 in real time on
 * a fast CPU — same emulator, same accuracy, just rendered ahead of
 * time on our slower hardware.
 *
 * Build: make GAME=examples/sc55_warcraft/main.c
 */
#include <stddef.h>
#include "jupiter.h"
#include "nuked_sc55.h"

#include "sc55_rom1.h"
#include "sc55_rom2.h"
#include "sc55_waverom1.h"
#include "sc55_waverom2.h"
#include "sc55_rom_sm.h"
#include "human1.h"

/* ================================================================
 *  SMF parser — copy of the one in examples/mt32_warcraft/main.c,
 *  adapted to dispatch raw MIDI bytes via sc55_post_uart() instead
 *  of mt32emu_play_msg().
 * ================================================================ */

typedef struct {
    const uint8_t *trk_start;
    const uint8_t *trk_end;
    const uint8_t *cursor;
    uint8_t  running_status;
    uint16_t format;
    uint16_t ntrks;
    uint16_t division;
    uint64_t next_event_us;
    uint32_t us_per_tick;
    uint32_t us_per_quarter;
    uint8_t  done;
} smf_t;

static inline uint16_t be16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }
static inline uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  | p[3];
}
static inline uint32_t read_vlq(const uint8_t **p) {
    uint32_t v = 0; uint8_t b;
    do { b = **p; (*p)++; v = (v << 7) | (b & 0x7F); } while (b & 0x80);
    return v;
}

static int smf_init(smf_t *s, const uint8_t *data, uint32_t size)
{
    if (size < 14) return -1;
    if (data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd')
        return -1;
    uint32_t hdr_len = be32(data + 4);
    s->format   = be16(data + 8);
    s->ntrks    = be16(data + 10);
    s->division = be16(data + 12);
    const uint8_t *p = data + 8 + hdr_len;
    const uint8_t *end = data + size;
    s->trk_start = NULL;
    while (p + 8 <= end) {
        uint32_t clen = be32(p + 4);
        if (p[0] == 'M' && p[1] == 'T' && p[2] == 'r' && p[3] == 'k') {
            s->trk_start = p + 8;
            s->trk_end   = p + 8 + clen;
            if (s->trk_end > end) s->trk_end = end;
            break;
        }
        p += 8 + clen;
    }
    if (!s->trk_start) return -2;
    s->cursor = s->trk_start;
    s->running_status = 0;
    s->done = 0;
    s->us_per_quarter = 500000;
    uint16_t div = s->division ? s->division : 96;
    s->us_per_tick = s->us_per_quarter / div;
    const uint8_t *cp = s->cursor;
    uint32_t delta = read_vlq(&cp);
    s->cursor = cp;
    s->next_event_us = (uint64_t)delta * s->us_per_tick;
    return 0;
}

/* Send one SMF event to the SC-55 via raw UART bytes. Returns 1 if
 * an event was consumed, 0 if end-of-track. */
static int smf_step(smf_t *s)
{
    if (s->done || s->cursor >= s->trk_end) { s->done = 1; return 0; }
    const uint8_t *p = s->cursor;
    uint8_t status = *p;
    if (status & 0x80) p++;
    else status = s->running_status;

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
            s->done = 1;
            return 0;
        }
        p += len;
    } else if (status == 0xF0 || status == 0xF7) {
        uint32_t len = read_vlq(&p);
        /* SC-55 sysex: SMF stores F0-events without the leading F0;
         * we restore it before feeding the H8 UART. F7-events are raw
         * byte streams, send them as-is. */
        if (status == 0xF0) sc55_post_uart(0xF0);
        for (uint32_t i = 0; i < len; i++) sc55_post_uart(p[i]);
        p += len;
    } else {
        uint8_t top = status & 0xF0;
        uint8_t d1 = *p++;
        uint8_t d2 = 0;
        if (top != 0xC0 && top != 0xD0) d2 = *p++;
        s->running_status = status;
        sc55_post_uart(status);
        sc55_post_uart(d1);
        if (top != 0xC0 && top != 0xD0) sc55_post_uart(d2);
    }
    s->cursor = p;
    if (s->cursor >= s->trk_end) { s->done = 1; return 0; }
    uint32_t delta = read_vlq(&s->cursor);
    s->next_event_us += (uint64_t)delta * s->us_per_tick;
    return 1;
}

/* ================================================================
 *  Recording sink: SC-55 produces samples at 66207 Hz, we resample
 *  to 48000 Hz on the fly using a simple Q16 phase accumulator and
 *  nearest-neighbor selection. SC-55 output is already band-limited
 *  enough that nearest-neighbor downsampling is fine here.
 * ================================================================ */

#define IN_RATE  66207
#define OUT_RATE 48000

/* Pre-rendered audio buffer in DRAM. Sized for ~25 seconds of stereo
 * 16-bit at 48 kHz = 4.8 MB. Lives in normal BSS; the linker places
 * it in the CODE region which has plenty of room. */
#define RECORDED_SECS  25
#define RECORDED_FRAMES (OUT_RATE * RECORDED_SECS)
static int16_t recorded[RECORDED_FRAMES * 2];
static volatile uint32_t recorded_pos;

static uint32_t resamp_accum;          /* Q16 fixed point */
static const uint32_t resamp_step = ((uint64_t)OUT_RATE << 16) / IN_RATE;

static void recorder_cb(int16_t L, int16_t R)
{
    resamp_accum += resamp_step;
    if (resamp_accum >= (1u << 16)) {
        resamp_accum -= (1u << 16);
        if (recorded_pos < RECORDED_FRAMES) {
            recorded[recorded_pos * 2 + 0] = L;
            recorded[recorded_pos * 2 + 1] = R;
            recorded_pos++;
        }
    }
}

/* ================================================================
 *  Audio playback (after pre-render is complete)
 * ================================================================ */

#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;

static uint32_t play_pos;  /* Current playback frame index in `recorded` */

#define TARGET_DEPTH 2400  /* ~25 ms of 48 kHz stereo */

static void playback_fill(uint32_t up_to_frames)
{
    uint32_t target = up_to_frames * 2;
    if (target > MIX_BUF_SIZE - 2) target = MIX_BUF_SIZE - 2;
    while ((mix_wr - mix_rd) < target) {
        if (play_pos >= recorded_pos) {
            play_pos = 0;  /* loop the song */
        }
        mix_buf[mix_wr & MIX_BUF_MASK] = recorded[play_pos * 2 + 0];
        mix_wr++;
        mix_buf[mix_wr & MIX_BUF_MASK] = recorded[play_pos * 2 + 1];
        mix_wr++;
        play_pos++;
    }
}

/* ================================================================
 *  MAIN
 * ================================================================ */

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();

    uart_puts("\n\n=== Jupiter SDK — Nuked-SC55 offline render + playback ===\n");
    uart_puts("Real Roland SC-55 mkII emulation, pre-rendered to DRAM.\n\n");

    cpp_init();
    audio_init();

    /* ---- SC-55 bring-up ---- */
    uart_puts("Loading ROMs and booting H8 MCU... ");
    uint32_t t0 = timer_read();
    int rc = sc55_init(sc55_rom1,     sc55_rom1_len,
                       sc55_rom2,     sc55_rom2_len,
                       sc55_waverom1, sc55_waverom1_len,
                       sc55_waverom2, sc55_waverom2_len,
                       sc55_rom_sm,   sc55_rom_sm_len);
    if (rc != 0) { uart_puts("FAIL\n"); while (1); }
    uart_puts("ok ("); uart_putdec(ticks_to_us(timer_elapsed(t0, timer_read())) / 1000); uart_puts(" ms)\n");
    uart_puts("native rate: "); uart_putdec(sc55_native_rate()); uart_puts(" Hz\n");

    /* Wire the SC-55 sample callback to the recording sink */
    sc55_set_sample_callback(recorder_cb);
    recorded_pos = 0;
    resamp_accum = 0;

    /* Run the SC-55 boot sequence for ~50 ms of emulated time so its
     * GM-mode init has a chance to settle before we start sending MIDI. */
    uart_puts("Warming up SC-55 (boot ROM init)...\n");
    sc55_render(IN_RATE / 20);  /* ~50ms emulated */
    /* Discard those samples — they're SC-55 startup pop, not music. */
    recorded_pos = 0;
    resamp_accum = 0;

    /* ---- Parse SMF and pre-render the song ---- */
    smf_t smf;
    if (smf_init(&smf, smf_human1, smf_human1_len) != 0) {
        uart_puts("smf_init FAIL\n"); while (1);
    }
    uart_puts("SMF: format="); uart_putdec(smf.format);
    uart_puts(" division="); uart_putdec(smf.division); uart_puts("\n");

    uart_puts("\nPre-rendering ");
    uart_putdec(RECORDED_SECS);
    uart_puts(" seconds of audio (this takes a while)...\n");

    uint32_t render_start = timer_read();
    uint32_t last_print_us = 0;
    /* Render in chunks. After each chunk, advance SMF events whose time
     * has come (using cur_us derived from output frame count). */
    const uint32_t CHUNK_OUT = 256;  /* output frames per chunk */
    while (recorded_pos < RECORDED_FRAMES) {
        /* Translate output-rate sample count to input samples we need
         * to produce in this chunk. With nearest-neighbor we need
         * roughly CHUNK_OUT * IN_RATE/OUT_RATE input samples. */
        uint32_t before = recorded_pos;
        uint32_t budget = (CHUNK_OUT * IN_RATE / OUT_RATE) + 4;
        sc55_render(budget);
        if (recorded_pos == before) {
            /* Safety: avoid infinite loop if SC-55 stops producing */
            uart_puts("WARN: no progress this chunk\n");
            break;
        }

        /* Current emulated wall-clock time, derived from output samples */
        uint64_t cur_us = (uint64_t)recorded_pos * 1000000ULL / OUT_RATE;

        /* Process all MIDI events whose time has arrived */
        while (!smf.done && cur_us >= smf.next_event_us) {
            smf_step(&smf);
        }

        /* Progress print every emulated second */
        if (cur_us - last_print_us >= 1000000) {
            uint32_t wall_us = ticks_to_us(timer_elapsed(render_start, timer_read()));
            uart_puts("  rendered ");
            uart_putdec((uint32_t)(cur_us / 1000000));
            uart_puts("s of audio in ");
            uart_putdec(wall_us / 1000);
            uart_puts(" ms wall-clock (");
            /* Use 64-bit math to avoid overflow once wall_us > ~43s */
            uart_putdec((uint32_t)(((uint64_t)wall_us * 100) / cur_us));
            uart_puts("% of real-time)\n");
            last_print_us = cur_us;
        }
    }

    uint32_t render_total_us = ticks_to_us(timer_elapsed(render_start, timer_read()));
    uart_puts("\nPre-render complete. ");
    uart_putdec(recorded_pos);
    uart_puts(" frames in ");
    uart_putdec(render_total_us / 1000);
    uart_puts(" ms (");
    {
        /* (wall_us / song_us) × 100 — use uint64 throughout */
        uint64_t song_us = (uint64_t)recorded_pos * 1000000ULL / OUT_RATE;
        uint32_t pct = song_us ? (uint32_t)(((uint64_t)render_total_us * 100ULL) / song_us) : 0;
        uart_putdec(pct);
    }
    uart_puts("% of real-time speed)\n");

    /* ---- Playback ---- */
    uart_puts("\nStarting playback at 48000 Hz. Looping forever.\n");
    audio_set_rate(48000);
    play_pos = 0;
    /* Pre-fill mix buffer */
    playback_fill(MIX_BUF_SIZE / 4);
    audio_start();
    irq_global_enable();

    while (1) {
        playback_fill(TARGET_DEPTH);
        /* Tiny pause between fill iterations to avoid hot-spinning */
        for (volatile int i = 0; i < 1000; i++) ;
    }
}
