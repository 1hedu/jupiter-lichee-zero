/*
 * Jupiter SDK — SD music-pack playback smoke test.
 *
 * Brings up SDMMC0, reads the WC1 music pack header at LBA 524288,
 * dumps the 45 track entries, then loads track 0 (title music) into a
 * heap buffer and plays it on PCM channel 0 via audio_pcm_play_rate.
 * Loops forever — power-cycle to stop. UART prints status; audio comes
 * out the headphone jack.
 *
 * Build: make GAME=examples/sdmmc_music/main.c
 * Prereq: scripts/sd_pack_music.py output dd'd onto the SD at LBA 524288.
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "sdmmc.h"
#include "hstimer.h"
#include <string.h>
#include <stddef.h>

#define D(s) uart_puts("[ex] " s "\n")

#define MUSIC_LBA       524288u
#define HDR_SECT        4u
#define MAGIC_LE        0x4D314357u  /* 'WC1M' little-endian */
#define MAX_TRACKS      64
#define PLAY_TRACK      0   /* which track to play */

typedef struct {
    char     name[16];
    uint32_t lba_offset;
    uint32_t length_bytes;
    uint32_t sample_rate;
    uint32_t flags;
} track_t;

typedef struct {
    char     magic[4];
    uint16_t version;
    uint16_t num_tracks;
    uint32_t hdr_sectors;
    uint32_t reserved;
    track_t  tracks[MAX_TRACKS];
} pack_hdr_t;

static uint8_t   hdr_raw[HDR_SECT * 512] __attribute__((aligned(4)));
static pack_hdr_t pack;

extern void *malloc(size_t);

/* Plain CRC32 (poly 0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF) — table-free. */
static uint32_t crc32(const void *data, uint32_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/* MIX_BUF_SIZE matches lib/audio.c. We mix in chunks of 32 stereo frames
 * = 64 slots; cap at MIX_BUF_SIZE - 128 to leave headroom so the IRQ
 * never wraps unread data. */
#define MIX_BUF_SIZE_LOCAL 4096u
#define MIX_HIGH_WATER     (MIX_BUF_SIZE_LOCAL - 128u)
extern volatile uint32_t mix_rd;
extern volatile uint32_t mix_wr;
extern volatile uint32_t audio_underruns;
extern volatile uint32_t audio_transitions;
extern volatile uint32_t audio_dbg_refill_a;
extern volatile uint32_t audio_dbg_refill_b;
extern volatile uint32_t audio_dbg_alt_break_a;
extern volatile uint32_t audio_dbg_alt_break_b;
extern volatile uint32_t audio_dbg_isr_entries;
extern volatile uint32_t audio_dbg_no_pkg;
extern volatile uint32_t audio_dbg_cursrc_disagree;

/* All counters incremented from hstimer IRQ; declared volatile so the
 * main-loop heartbeat reads consistent values. min/max depth tracked
 * across each heartbeat period (reset after print). */
static volatile uint32_t g_tick_count    = 0;   /* total hstimer ticks */
static volatile uint32_t g_tick_skipped  = 0;   /* ticks where buf was full */
static volatile uint32_t g_tick_mix_us   = 0;   /* sum of audio_mix duration */
static volatile uint32_t g_tick_max_us   = 0;   /* slowest single audio_mix */
static volatile uint32_t g_depth_min     = 0xFFFFFFFFu;
static volatile uint32_t g_depth_max     = 0;
static volatile uint32_t g_depth_zero_seen = 0;  /* count of times depth==0 inside ISR */

static inline void track_depth(uint32_t depth) {
    if (depth < g_depth_min) g_depth_min = depth;
    if (depth > g_depth_max) g_depth_max = depth;
    if (depth == 0) g_depth_zero_seen++;
}

static void audio_mix_tick(void) {
    g_tick_count++;
    uint32_t depth = mix_wr - mix_rd;
    track_depth(depth);
    if (depth >= MIX_HIGH_WATER) {
        g_tick_skipped++;
        return;
    }
    uint32_t t0 = timer_read();
    audio_mix(32);
    uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
    g_tick_mix_us += us;
    if (us > g_tick_max_us) g_tick_max_us = us;
}

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();

    uart_puts("\n\n=== Jupiter SD music verifier ===\n");

    D("step 1: sdmmc_init");
    int rc = sdmmc_init();
    if (rc) { uart_puts("[ex] FATAL sdmmc_init rc=-"); uart_putdec((uint32_t)(-rc)); uart_puts("\n"); for (;;) {} }
    const sdmmc_card_t *c = sdmmc_card();
    uart_puts("[ex] card ");
    uart_puts(c->is_sdhc ? "SDHC" : "SDSC");
    uart_puts(" cap=");
    uart_putdec(c->num_blocks / 2048);
    uart_puts(" MiB\n");

    D("step 2: read pack header from LBA 524288");
    rc = sdmmc_read_blocks(MUSIC_LBA, HDR_SECT, hdr_raw);
    if (rc) { uart_puts("[ex] FATAL header read rc=-"); uart_putdec((uint32_t)(-rc)); uart_puts("\n"); for (;;) {} }

    /* Verify magic. */
    uint32_t magic = ((uint32_t)hdr_raw[0])
                   | ((uint32_t)hdr_raw[1] << 8)
                   | ((uint32_t)hdr_raw[2] << 16)
                   | ((uint32_t)hdr_raw[3] << 24);
    if (magic != MAGIC_LE) {
        uart_puts("[ex] FATAL: pack magic mismatch (got 0x");
        uart_puthex(magic);
        uart_puts(", want 0x4D314357 = 'WC1M')\n");
        uart_puts("[ex] hint: did you `dd if=build/wc1_music.img of=/dev/sdX seek=524288`?\n");
        for (;;) {}
    }
    memcpy(&pack, hdr_raw, sizeof(pack));
    uart_puts("[ex] pack OK: version=");
    uart_putdec(pack.version);
    uart_puts(" num_tracks=");
    uart_putdec(pack.num_tracks);
    uart_puts(" hdr_sectors=");
    uart_putdec(pack.hdr_sectors);
    uart_puts("\n");

    D("step 3: track table");
    for (int i = 0; i < pack.num_tracks; i++) {
        track_t *t = &pack.tracks[i];
        uart_puts("  [");
        if (i < 10) uart_putc('0');
        uart_putdec((uint32_t)i);
        uart_puts("] ");
        for (int n = 0; n < 16 && t->name[n]; n++) uart_putc(t->name[n]);
        uart_puts(" lba_off=");
        uart_putdec(t->lba_offset);
        uart_puts(" len=");
        uart_putdec(t->length_bytes);
        uart_puts("B rate=");
        uart_putdec(t->sample_rate);
        uart_puts(" mono=");
        uart_putdec(t->flags & 1);
        uart_puts("\n");
    }

    if (PLAY_TRACK >= pack.num_tracks) {
        uart_puts("[ex] PLAY_TRACK out of range\n");
        for (;;) {}
    }

    track_t *t = &pack.tracks[PLAY_TRACK];

    D("step 4: audio bring-up");
    audio_init();
    audio_set_rate(11025);  /* matches our pre-rendered tracks */
    audio_set_hp_vol(63);
    audio_set_master_volume(255);
    /* Pre-fill silence so audio_start's prefill copies valid samples
     * (matches examples/war1/main.c). */
    extern int16_t mix_buf[];
    extern volatile uint32_t mix_wr;
    for (int i = 0; i < 2048; i++) { mix_buf[i] = 0; mix_wr++; }
    audio_start();
    irq_global_enable();

    /* HDMA-style scanline-driven audio mix: hstimer fires every 32
     * scanlines (~1.87 ms at our 17160 scanlines/sec rate) and produces
     * 32 stereo frames per call (~2.9 ms of audio at 11021 Hz). Net 1.55x
     * production rate keeps mix_buf full even if the main loop stalls
     * for UART prints, SD reads, etc. — a critical isolation since
     * audio underruns are the most audible glitch. */
    hstimer_init();
    hstimer_set_repeating(0, 32, audio_mix_tick);

    D("step 5: allocate track buffer");
    uint32_t pcm_bytes = t->length_bytes;
    uint32_t buf_bytes = (pcm_bytes + 3u) & ~3u;
    int16_t *buf = (int16_t *)malloc(buf_bytes);
    if (!buf) {
        uart_puts("[ex] FATAL: malloc(");
        uart_putdec(buf_bytes);
        uart_puts(") failed\n");
        for (;;) {}
    }
    uart_puts("[ex] alloc ");
    uart_putdec(buf_bytes);
    uart_puts(" bytes at 0x");
    uart_puthex((uint32_t)(uintptr_t)buf);
    uart_puts("\n");

    D("step 6: read track payload from SD");
    uint32_t sectors = (pcm_bytes + 511u) / 512u;
    uint32_t start_lba = MUSIC_LBA + t->lba_offset;
    uint32_t t0 = timer_read();
    rc = sdmmc_read_blocks(start_lba, sectors, buf);
    uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
    if (rc) {
        uart_puts("[ex] FATAL: track read rc=-");
        uart_putdec((uint32_t)(-rc));
        uart_puts("\n");
        for (;;) {}
    }
    uart_puts("[ex] read ");
    uart_putdec(sectors);
    uart_puts(" sectors in ");
    uart_putdec(us);
    uart_puts(" us = ~");
    uart_putdec((sectors * 512u * 1000u) / (us / 1000u + 1u));
    uart_puts(" KiB/s\n");

    /* Quick sanity: peek first 4 samples (silence at start of most tracks
     * is normal; just confirms data is there). */
    uart_puts("[ex] first samples:");
    for (int i = 0; i < 8; i++) {
        uart_puts(" ");
        int16_t s = buf[i];
        if (s < 0) { uart_putc('-'); uart_putdec((uint32_t)-s); }
        else       { uart_putdec((uint32_t)s); }
    }
    uart_puts("\n");

    /* Snapshot the loaded buffer's CRC32. Any drift over time = something
     * is writing into the buffer in RAM (stack, heap, DMA stray, etc). */
    uint32_t load_crc = crc32(buf, pcm_bytes);
    uart_puts("[ex] load CRC32=0x");
    uart_puthex(load_crc);
    uart_puts(" over ");
    uart_putdec(pcm_bytes);
    uart_puts(" bytes\n");

    D("step 7: kick playback");
    uint32_t samples = pcm_bytes / 2;
    audio_pcm_play_rate(0,
                        buf, samples,
                        180,             /* volume */
                        1,               /* loop */
                        t->sample_rate ? t->sample_rate : 11025);

    uart_puts("[ex] track ");
    uart_putdec(PLAY_TRACK);
    uart_puts(" playing on ch0 (looping). Listen.\n");

    /* Main loop is now passive — hstimer ISR drives audio mixing on
     * its own. Each second print a comprehensive snapshot so we can
     * see exactly what's happening during the audio glitches. */
    uint32_t sec = 0;
    uint32_t last = timer_read();
    /* Capture the load-time CRC for re-verification at every heartbeat.
     * If the buffer in RAM ever drifts from this, something is writing
     * into our music buffer (stack overflow, stray DMA, heap corruption). */
    const uint32_t crc_initial = load_crc;
    const uint32_t crc_len     = pcm_bytes;
    uint32_t crc_changed_seen  = 0;
    uint32_t prev_underruns = 0;
    uint32_t prev_transitions = 0;
    uint32_t prev_ticks = 0;
    uint32_t main_iter = 0;
    for (;;) {
        main_iter++;
        if (ticks_to_us(timer_elapsed(last, timer_read())) > 1000000u) {
            last = timer_read();
            sec++;

            /* Snapshot all counters atomically (close enough — IRQs may
             * tick during the read, but we only care about per-second
             * deltas so a few extra ticks across the boundary don't
             * matter). */
            uint32_t now_under = audio_underruns;
            uint32_t now_trans = audio_transitions;
            uint32_t now_ticks = g_tick_count;
            uint32_t now_skip  = g_tick_skipped;
            uint32_t now_mix_us = g_tick_mix_us;
            uint32_t now_max_us = g_tick_max_us;
            uint32_t now_min_d  = g_depth_min;
            uint32_t now_max_d  = g_depth_max;
            uint32_t now_zero   = g_depth_zero_seen;
            uint32_t cur_depth  = mix_wr - mix_rd;
            uint32_t cur_iter   = main_iter;

            /* Reset min/max/per-period accumulators. */
            g_depth_min = 0xFFFFFFFFu;
            g_depth_max = 0;
            g_tick_mix_us = 0;
            g_tick_max_us = 0;

            uint32_t d_under = now_under - prev_underruns;
            uint32_t d_trans = now_trans - prev_transitions;
            uint32_t d_ticks = now_ticks - prev_ticks;
            prev_underruns   = now_under;
            prev_transitions = now_trans;
            prev_ticks       = now_ticks;
            main_iter        = 0;

            uart_puts("[hb] t=");      uart_putdec(sec);
            uart_puts("s busy=");      uart_putdec((uint32_t)audio_pcm_channel_busy(0));
            uart_puts(" depth_now="); uart_putdec(cur_depth);
            uart_puts(" min=");       uart_putdec(now_min_d);
            uart_puts(" max=");       uart_putdec(now_max_d);
            uart_puts("\n");
            uart_puts("[hb] ticks=");  uart_putdec(d_ticks);
            uart_puts(" skipped=");    uart_putdec(now_skip);
            uart_puts(" mix_us_total="); uart_putdec(now_mix_us);
            uart_puts(" mix_us_max=");   uart_putdec(now_max_us);
            uart_puts("\n");
            uart_puts("[hb] dma_trans="); uart_putdec(d_trans);
            uart_puts(" underruns=");     uart_putdec(now_under);
            uart_puts(" (+"); uart_putdec(d_under); uart_puts(")");
            uart_puts(" depth_zero_seen="); uart_putdec(now_zero);
            uart_puts(" main_iter="); uart_putdec(cur_iter);
            uart_puts("\n");
            uart_puts("[hb] refill_a="); uart_putdec(audio_dbg_refill_a);
            uart_puts(" refill_b="); uart_putdec(audio_dbg_refill_b);
            uart_puts(" alt_break_a="); uart_putdec(audio_dbg_alt_break_a);
            uart_puts(" alt_break_b="); uart_putdec(audio_dbg_alt_break_b);
            uart_puts(" isr_entries="); uart_putdec(audio_dbg_isr_entries);
            uart_puts(" no_pkg="); uart_putdec(audio_dbg_no_pkg);
            uart_puts(" cursrc_dis="); uart_putdec(audio_dbg_cursrc_disagree);
            uart_puts("\n");

            /* Full-buffer CRC re-check. Takes ~80 ms for 6 MB; runs in main
             * thread, doesn't disturb audio (hstimer ISR keeps mixing). */
            uint32_t crc_now = crc32(buf, crc_len);
            uart_puts("[hb] crc_now=0x"); uart_puthex(crc_now);
            uart_puts(" load=0x"); uart_puthex(crc_initial);
            if (crc_now != crc_initial) {
                crc_changed_seen++;
                uart_puts(" !!!!! BUFFER MUTATED in RAM (crc drift count=");
                uart_putdec(crc_changed_seen);
                uart_puts(") !!!!!");
            }
            uart_puts("\n");

            /* Big red banner if we glitched this second. */
            if (d_under > 0) {
                uart_puts("[hb] !!!!! UNDERRUN this period: +");
                uart_putdec(d_under);
                uart_puts(" !!!!!\n");
            }
        }
    }
}
