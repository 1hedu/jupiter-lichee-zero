/*
 * Jupiter SDK — Audio System
 * V3s codec, 48 kHz, DMA ping-pong.
 * Supports 16-bit and 24-bit (S32_LE) DAC output.
 *
 * 24-bit mode: V3s DAC is internally 24-bit. FIFO_MODE=00 reads
 * TXDATA[31:8] as FIFO_I[23:0]. DMA transfers 32-bit words with
 * the sample left-shifted into the upper bits.
 *
 * KEY: CODEC_BASE + 0x60 = 0x60000000 enables DAC DAP signal path.
 */
#include "jupiter.h"

#define SAMPLE_RATE     48000
#define MIX_BUF_SIZE    4096
#define MIX_BUF_MASK    (MIX_BUF_SIZE - 1)
#define DMA_DESC_A_ADDR (AUDIO_BUF_ADDR + 0x10000)
#define DMA_DESC_B_ADDR (AUDIO_BUF_ADDR + 0x10000 + 32)
#define DMA_HALF_BYTES_16  (AUDIO_BUF_HALF * 2)      /* 16-bit: 2 bytes/sample */
#define DMA_HALF_BYTES_32  (AUDIO_BUF_HALF * 4)      /* 32-bit: 4 bytes/sample */

int16_t  mix_buf[MIX_BUF_SIZE];
volatile uint32_t mix_wr = 0;
volatile uint32_t mix_rd = 0;
volatile uint32_t audio_underruns = 0;
volatile uint32_t audio_transitions = 0;

/* 16-bit DMA buffers (default) */
static int16_t *dma_buf_a_16 = (int16_t *)AUDIO_BUF_ADDR;
static int16_t *dma_buf_b_16 = (int16_t *)(AUDIO_BUF_ADDR + DMA_HALF_BYTES_16);

/* 32-bit DMA buffers (24-bit mode, same base address, larger) */
static int32_t *dma_buf_a_32 = (int32_t *)AUDIO_BUF_ADDR;
static int32_t *dma_buf_b_32 = (int32_t *)(AUDIO_BUF_ADDR + DMA_HALF_BYTES_32);

static volatile uint32_t dma_running = 0;
static uint8_t audio_use_24bit = 0;
volatile int a_filled = 0, b_filled = 0;

/* Forward declaration for ISR */
static void audio_dma_refill_a(void);
static void audio_dma_refill_b(void);
void audio_dma_isr(void);

#define AUDIO_MAX_CHANNELS 4
/* Per-channel position is split: pos_int is the integer sample index
 * (full 32-bit range, supports buffers up to 2^32 samples), pos_frac is
 * the Q16 fractional accumulator (only low 16 bits used). Each output
 * sample: pos_frac += pos_step; pos_int += pos_frac >> 16; pos_frac &= 0xFFFF.
 * native-rate playback keeps pos_step = 1<<16 (every other sample == native).
 * The previous "single uint32_t Q16 pos" capped buffers at 65535 samples
 * (~6 s at 11 kHz) — fine for SFX, broken for music tracks. */
typedef struct {
    const int16_t *samples; uint32_t length;
    uint32_t pos_int;
    uint32_t pos_frac;
    uint32_t pos_step;
    uint8_t volume, loop, active;
} pcm_channel_t;
static pcm_channel_t channels[AUDIO_MAX_CHANNELS];
uint8_t master_volume = 200;

#define SINE_LEN 64
static const int16_t sine_table[SINE_LEN] = {
        0,  3211,  6392,  9511, 12539, 15446, 18204, 20787,
    23169, 25329, 27244, 28897, 30272, 31356, 32137, 32609,
    32767, 32609, 32137, 31356, 30272, 28897, 27244, 25329,
    23169, 20787, 18204, 15446, 12539,  9511,  6392,  3211,
        0, -3211, -6392, -9511,-12539,-15446,-18204,-20787,
   -23169,-25329,-27244,-28897,-30272,-31356,-32137,-32609,
   -32767,-32609,-32137,-31356,-30272,-28897,-27244,-25329,
   -23169,-20787,-18204,-15446,-12539, -9511, -6392, -3211,
};

/* ADDA_PR indirect access */
static void aw(uint32_t reg, uint32_t val) {
    volatile uint32_t *pr = (volatile uint32_t *)0x01C23000;
    uint32_t r;
    r = *pr; r |= (1U<<28); *pr = r;
    r = *pr; r &= ~(0x1FU<<16); r |= ((reg&0x1F)<<16); *pr = r;
    r = *pr; r &= ~(0xFFU<<8); r |= ((val&0xFF)<<8); *pr = r;
    r = *pr; r |= (1U<<24); *pr = r;
    r = *pr; r &= ~(1U<<24); *pr = r;
    for (volatile int d=0;d<200;d++);
}
static uint32_t ar(uint32_t reg) {
    volatile uint32_t *pr = (volatile uint32_t *)0x01C23000;
    uint32_t r;
    r = *pr; r |= (1U<<28); *pr = r;
    r = *pr; r &= ~(1U<<24); *pr = r;
    r = *pr; r &= ~(0x1FU<<16); r |= ((reg&0x1F)<<16); *pr = r;
    for (volatile int d=0;d<200;d++);
    return *pr & 0xFF;
}
static void ms(uint32_t v) {
    uint32_t t0 = timer_read();
    while (ticks_to_us(timer_elapsed(t0, timer_read())) < v * 1000);
}

/* Runtime HP PA output gain control. vol = 0..63 → -63..0 dB (1 dB/step).
 * ADDA_PR reg 0x00 bits[5:0] = HP_VOL per sun8i-codec-analog.c. Other bits in
 * that reg aren't used in our init (current value 0x2B = vol 43 = -20 dB),
 * so we just overwrite with the new value. */
void audio_set_hp_vol(uint8_t vol_0_63) {
    if (vol_0_63 > 63) vol_0_63 = 63;
    aw(0x00, vol_0_63 & 0x3F);
}

/* ================================================================== */
void audio_init(void)
{
    /*
     * EXACT sequence from working audio_clone test.
     * Same order, same values, same delays.
     */
    uart_puts("Audio: clone init...\n");

    /* 1. Bus gate + assert reset */
    REG32(0x01C20000+0x0068) |= BIT(0);
    REG32(0x01C20000+0x02D0) &= ~BIT(0);
    ms(10);

    /* 2. PLL: no SDM, N=85, M=20, P=3 (12.29MHz — actual Linux value 0x90035514) */
    REG32(0x01C20000+0x0008) = 0;
    REG32(0x01C20000+0x0284) = 0x00000000;
    REG32(0x01C20000+0x0008) = BIT(31) | (3U<<16) | (85U<<8) | (20U<<0);
    ms(200);
    uart_puts("  PLL="); uart_puthex(REG32(0x01C20000+0x0008)); uart_puts("\n");

    /* 3. De-assert reset + module clock */
    REG32(0x01C20000+0x02D0) |= BIT(0);
    ms(10);
    REG32(0x01C20000+0x0140) = BIT(31);
    ms(10);

    /* 4. DMA clocks */
    REG32(0x01C20000+0x0060) |= BIT(6);
    REG32(0x01C20000+0x02C0) |= BIT(6);
    ms(1);
    REG32(0x01C02000+0x20) = 0x04;

    /* 5. DAC digital: DPC, FIFOC, DAP */
    REG32(0x01C22C00+0x00) = 0x80012000;  /* EN_DA + DVOL=18 */
    REG32(0x01C22C00+0x04) = BIT(0);       /* flush */
    ms(1);
    if (audio_use_24bit) {
        /* FIFO_MODE[25:24]=00: FIFO_I[23:0] = TXDATA[31:8] (24-bit from 32-bit word)
         * Same trigger level, DRQ, sample rate as 16-bit config */
        REG32(0x01C22C00+0x04) = 0x00600F10;
        uart_puts("  FIFOC=24bit(S32_LE) ");
    } else {
        /* FIFO_MODE[25:24]=01: FIFO_I[23:0] = {TXDATA[15:0], 8'b0} (16-bit compact) */
        REG32(0x01C22C00+0x04) = 0x01600F10;
        uart_puts("  FIFOC=16bit ");
    }
    uart_puthex(REG32(0x01C22C00+0x04)); uart_puts("\n");
    REG32(0x01C22C00+0x60) = 0x60000000;  /* DAP enable! */
    uart_puts("  DAP="); uart_puthex(REG32(0x01C22C00+0x60)); uart_puts("\n");

    /* 6. Analog (exact Linux values, exact clone delays) */
    aw(0x01, 0x02);
    aw(0x02, 0x02);
    aw(0x03, 0xCC); ms(100);
    aw(0x00, 0x2B); ms(50);
    aw(0x07, 0xF4); ms(700);

    uart_puts("  ADDA:");
    for (uint32_t r = 0; r < 8; r++) { uart_puts(" "); uart_puthex(ar(r)); }
    uart_puts("\n");

    /* 7. State init */
    mix_wr = mix_rd = 0;
    for (int i = 0; i < AUDIO_MAX_CHANNELS; i++) channels[i].active = 0;

    /* 8. DMA ping-pong */
    uint32_t dma_half, dma_cfg;
    if (audio_use_24bit) {
        dma_half = DMA_HALF_BYTES_32;
        /* DMA cfg: src=DRAM(1), src_width=32bit(2), src_burst=8(2), src_linear
         *          dst=CODEC(15), dst_width=32bit(2), dst_burst=8(2), dst_IO */
        dma_cfg = 0x052F0501;
        for (uint32_t i = 0; i < AUDIO_BUF_HALF * 2; i++)
            ((int32_t *)AUDIO_BUF_ADDR)[i] = 0;
    } else {
        dma_half = DMA_HALF_BYTES_16;
        /* DMA cfg: src_width=16bit(1), dst_width=16bit(1) */
        dma_cfg = 0x032F0301;
        for (uint32_t i = 0; i < AUDIO_BUF_HALF * 2; i++)
            ((int16_t *)AUDIO_BUF_ADDR)[i] = 0;
    }
    dcache_clean_range(AUDIO_BUF_ADDR, dma_half * 2);

    dma_desc_t *da = (dma_desc_t *)DMA_DESC_A_ADDR;
    dma_desc_t *db = (dma_desc_t *)DMA_DESC_B_ADDR;

    da->cfg = dma_cfg;
    da->src_addr = AUDIO_BUF_ADDR;
    da->dst_addr = 0x01C22C20;
    da->byte_cnt = dma_half;
    da->param = 0;
    da->next = DMA_DESC_B_ADDR;

    db->cfg = dma_cfg;
    db->src_addr = AUDIO_BUF_ADDR + dma_half;
    db->dst_addr = 0x01C22C20;
    db->byte_cnt = dma_half;
    db->param = 0;
    db->next = DMA_DESC_A_ADDR;

    dcache_clean_range(DMA_DESC_A_ADDR, 64);

    DMA_CH_DESC(0) = DMA_DESC_A_ADDR;
    /* DMA NOT started here — call audio_start() after pre-filling mix_buf */

    uart_puts("Audio: ready (DMA armed, call audio_start).\n");
}

void audio_init_24bit(void)
{
    audio_use_24bit = 1;
    audio_init();
}

/* Boilerplate compressor — see jupiter.h's comment. Mirrors the sequence
 * almost every audio example wrote by hand:
 *   audio_init();
 *   audio_set_rate(rate);
 *   audio_set_hp_vol(63);
 *   audio_set_master_volume(255);
 *   for (int i = 0; i < 2048; i++) { mix_buf[i] = 0; mix_wr++; }
 *   audio_start();
 * Existing examples can adopt this incrementally without breaking; the
 * direct-call form still works. */
void audio_quickstart(uint32_t sample_rate)
{
    audio_init();
    audio_set_rate(sample_rate);
    audio_set_hp_vol(63);          /* max analog HP PA gain (0 dB) */
    audio_set_master_volume(255);  /* max digital mix gain */
    for (int i = 0; i < 2048; i++) { mix_buf[i] = 0; mix_wr++; }
    audio_start();
}

void audio_start(void)
{
    /* Fill both DMA halves from mix_buf before starting */
    if (audio_use_24bit) {
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            int16_t s = (mix_rd != mix_wr) ? mix_buf[mix_rd++ & MIX_BUF_MASK] : 0;
            dma_buf_a_32[i] = (int32_t)s << 16;
        }
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            int16_t s = (mix_rd != mix_wr) ? mix_buf[mix_rd++ & MIX_BUF_MASK] : 0;
            dma_buf_b_32[i] = (int32_t)s << 16;
        }
        dcache_clean_range(AUDIO_BUF_ADDR, DMA_HALF_BYTES_32 * 2);
    } else {
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            dma_buf_a_16[i] = (mix_rd != mix_wr) ?
                mix_buf[mix_rd++ & MIX_BUF_MASK] : 0;
        }
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            dma_buf_b_16[i] = (mix_rd != mix_wr) ?
                mix_buf[mix_rd++ & MIX_BUF_MASK] : 0;
        }
        dcache_clean_range(AUDIO_BUF_ADDR, DMA_HALF_BYTES_16 * 2);
    }
    a_filled = 1;
    b_filled = 1;

    /* Enable DMA PKG interrupt for channel 0 (fires each descriptor completion) */
    DMA_IRQ_EN(0) |= (DMA_IRQ_PKG << 0);  /* Channel 0 PKG = bit 1 */
    DMA_IRQ_STAT(0) = 0xF;                 /* Clear any pending for ch0 */

    /* Register ISR and enable in GIC */
    irq_register(IRQ_DMA, audio_dma_isr);
    irq_enable(IRQ_DMA);

    DMA_CH_EN(0) = 1;
    dma_running = 1;
}

/* ---- DMA ISR: called from IRQ vector on each descriptor completion ---- */
static void audio_dma_refill_a(void)
{
    if (audio_use_24bit) {
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            int16_t s = (mix_rd != mix_wr) ?
                mix_buf[mix_rd++ & MIX_BUF_MASK] : (audio_underruns++, (int16_t)0);
            dma_buf_a_32[i] = (int32_t)s << 16;  /* sample in TXDATA[31:16] */
        }
        dcache_clean_range(AUDIO_BUF_ADDR, DMA_HALF_BYTES_32);
    } else {
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            dma_buf_a_16[i] = (mix_rd != mix_wr) ?
                mix_buf[mix_rd++ & MIX_BUF_MASK] : (audio_underruns++, (int16_t)0);
        }
        dcache_clean_range(AUDIO_BUF_ADDR, DMA_HALF_BYTES_16);
    }
}

static void audio_dma_refill_b(void)
{
    uint32_t half_bytes;
    if (audio_use_24bit) {
        half_bytes = DMA_HALF_BYTES_32;
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            int16_t s = (mix_rd != mix_wr) ?
                mix_buf[mix_rd++ & MIX_BUF_MASK] : (audio_underruns++, (int16_t)0);
            dma_buf_b_32[i] = (int32_t)s << 16;
        }
    } else {
        half_bytes = DMA_HALF_BYTES_16;
        for (uint32_t i = 0; i < AUDIO_BUF_HALF; i++) {
            dma_buf_b_16[i] = (mix_rd != mix_wr) ?
                mix_buf[mix_rd++ & MIX_BUF_MASK] : (audio_underruns++, (int16_t)0);
        }
    }
    dcache_clean_range(AUDIO_BUF_ADDR + half_bytes, half_bytes);
}

/* Diagnostics for stutter hunt: per-half refill counts, last-half tracker
 * that flags when A or B is filled twice in a row (sign of a missed
 * opposite-half transition), and a count of "spurious" ISR entries
 * where PKG was set but we'd already refilled the side that looked like
 * "just completed". */
volatile uint32_t audio_dbg_refill_a    = 0;
volatile uint32_t audio_dbg_refill_b    = 0;
volatile uint32_t audio_dbg_alt_break_a = 0;  /* refill_a called twice in a row */
volatile uint32_t audio_dbg_alt_break_b = 0;  /* refill_b called twice in a row */
volatile uint32_t audio_dbg_isr_entries = 0;
volatile uint32_t audio_dbg_no_pkg      = 0;  /* ISR fired but PKG not set */
volatile uint32_t audio_dbg_cursrc_disagree = 0;  /* cursrc said wrong side */
static   int      audio_dbg_last_was_a  = -1; /* -1 = no prior, 0 = b, 1 = a */
/* Strict alternation: which half is the NEXT to refill. DMA descriptors
 * are pre-filled by audio_start; the first PKG comes from A→B transition
 * meaning A just completed — so the first refill is A. */
static   int      audio_next_refill_b   = 0;

void audio_dma_isr(void)
{
    audio_dbg_isr_entries++;
    uint32_t stat = DMA_IRQ_STAT(0);
    DMA_IRQ_STAT(0) = stat;  /* Write-1-to-clear */

    if (!(stat & (DMA_IRQ_PKG << 0))) {
        audio_dbg_no_pkg++;
        return;
    }

    /* STRICT ALTERNATION. The cur_src-based heuristic (which we still
     * record for diagnostics) was producing alt_break_a events on real
     * HW: the cur_src readback occasionally lagged across the half
     * boundary, causing the ISR to think the SAME half just completed
     * twice — so we'd refill A twice in a row, leaving B stale and
     * audibly playing back the previous-cycle B data on the next pass.
     * Strict alternation guarantees each PKG refills the opposite side
     * of the previous one, regardless of cur_src skew. */
    uint32_t cur = DMA_CH_CUR_SRC(0);
    uint32_t half = audio_use_24bit ? DMA_HALF_BYTES_32 : DMA_HALF_BYTES_16;
    int cursrc_says_a_done = (cur >= (AUDIO_BUF_ADDR + half));  /* DMA in B → A done */

    if (audio_next_refill_b) {
        audio_dma_refill_b();
        audio_dbg_refill_b++;
        if (audio_dbg_last_was_a == 0) audio_dbg_alt_break_b++;
        audio_dbg_last_was_a = 0;
        if (cursrc_says_a_done)  audio_dbg_cursrc_disagree++;
    } else {
        audio_dma_refill_a();
        audio_dbg_refill_a++;
        if (audio_dbg_last_was_a == 1) audio_dbg_alt_break_a++;
        audio_dbg_last_was_a = 1;
        if (!cursrc_says_a_done) audio_dbg_cursrc_disagree++;
    }
    audio_next_refill_b ^= 1;
    audio_transitions++;

    /* Top up mix_buf right after the half-buffer drain. The producer side
     * (audio_mix from Invalidate / per-frame loops) only runs while frames
     * are being rendered; long-running click handlers (Lua menu actions,
     * SD-streamed track switches, image bakes) can stall rendering for
     * hundreds of ms, draining mix_buf to empty and producing audible
     * music skips. Refilling from the ISR makes mixing frame-rate-
     * independent — same approach the cedar_video_av example uses.
     *
     * Bounded: refill exactly what was just consumed (AUDIO_BUF_HALF
     * stereo frames = 2*AUDIO_BUF_HALF mix_buf slots). Skipped if the
     * producer already has mix_buf above target depth, so a fast game
     * frame won't double-mix and overflow the ring. */
    {
        uint32_t depth = mix_wr - mix_rd;
        if (depth + (uint32_t)(AUDIO_BUF_HALF * 2) <= MIX_BUF_SIZE) {
            audio_mix(AUDIO_BUF_HALF);
        }
    }
}

/* Legacy polling — still works, but ISR handles it if interrupts are enabled */
void audio_update(void)
{
    if (!dma_running) return;

    uint32_t cur = DMA_CH_CUR_SRC(0);
    uint32_t hb = audio_use_24bit ? DMA_HALF_BYTES_32 : DMA_HALF_BYTES_16;
    int in_b = (cur >= (AUDIO_BUF_ADDR + hb));

    if (in_b) {
        b_filled = 0;
        if (!a_filled) {
            audio_dma_refill_a();
            a_filled = 1;
            audio_transitions++;
        }
    } else {
        a_filled = 0;
        if (!b_filled) {
            audio_dma_refill_b();
            b_filled = 1;
            audio_transitions++;
        }
    }
}

/* ---- PCM Mixer ----
 * num_samples is the count of OUTPUT FRAMES (1 frame = 1 stereo L+R pair,
 * 2 mix_buf slots). Matches opn2_jupiter / jupiter_moon / mt32_rt's mix_buf
 * convention — codec DMA reads mix_buf as 16-bit stereo interleaved, so
 * mono PCM channels are duplicated into L+R. */
void audio_mix(uint32_t num_samples) {
    for (uint32_t s = 0; s < num_samples; s++) {
        int32_t sum = 0;
        for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
            pcm_channel_t *c = &channels[ch];
            if (!c->active) continue;
            sum += ((int32_t)c->samples[c->pos_int] * c->volume) >> 8;
            c->pos_frac += c->pos_step;
            c->pos_int  += c->pos_frac >> 16;
            c->pos_frac &= 0xFFFF;
            if (c->pos_int >= c->length) {
                if (c->loop) c->pos_int -= c->length;
                else c->active = 0;
            }
        }
        sum = (sum * master_volume) >> 8;
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        mix_buf[mix_wr++ & MIX_BUF_MASK] = (int16_t)sum;  /* L */
        mix_buf[mix_wr++ & MIX_BUF_MASK] = (int16_t)sum;  /* R */
    }
}
void audio_pcm_play(uint32_t ch, const int16_t *s, uint32_t len, uint8_t vol, uint8_t loop) {
    if (ch >= AUDIO_MAX_CHANNELS) return;
    pcm_channel_t *c = &channels[ch];
    c->samples=s; c->length=len; c->pos_int=0; c->pos_frac=0;
    c->pos_step = 1u << 16;        /* native-rate playback */
    c->volume=vol; c->loop=loop; c->active=1;
}
/* Forward decls — defined further down (audio_set_rate section). The Q8
 * variant is what the resampler reads so we keep sub-Hz precision in
 * pos_step; current_audio_rate is the rounded integer kept for any
 * legacy caller that wants the human-readable rate. */
extern uint32_t current_audio_rate;
extern uint32_t current_audio_rate_q8;
/* Play a sample at an arbitrary source rate. Mixer resamples via
 * pos_step = (src_rate << 24) / current_audio_rate_q8, which is
 * mathematically identical to (src_rate * 65536) / actual_rate but
 * computed against the Q8 codec rate so any sub-Hz fractional error in
 * the codec's actual frequency is absorbed (resampler picks up the
 * leftover bits and outputs at the requested src_rate exactly). */
void audio_pcm_play_rate(uint32_t ch, const int16_t *s, uint32_t len,
                         uint8_t vol, uint8_t loop, uint32_t src_rate) {
    if (ch >= AUDIO_MAX_CHANNELS) return;
    if (src_rate == 0) src_rate = current_audio_rate;
    pcm_channel_t *c = &channels[ch];
    /* Mark inactive first so the audio ISR (which now also runs audio_mix)
     * can't observe a half-updated channel mid-rewrite. */
    c->active = 0;
    __sync_synchronize();
    c->samples=s; c->length=len; c->pos_int=0; c->pos_frac=0;
    c->pos_step = (uint32_t)(((uint64_t)src_rate << 24) / current_audio_rate_q8);
    c->volume=vol; c->loop=loop;
    /* Publish all field writes before re-arming. */
    __sync_synchronize();
    c->active=1;
}
int audio_pcm_channel_busy(uint32_t ch) {
    if (ch >= AUDIO_MAX_CHANNELS) return 0;
    return channels[ch].active;
}
void audio_pcm_stop(uint32_t ch) { if (ch<AUDIO_MAX_CHANNELS) channels[ch].active=0; }
void audio_set_master_volume(uint8_t v) { master_volume = v; }

static int16_t test_tone_buf[48000];
const int16_t *audio_generate_test_tone(uint32_t freq, uint32_t dur_ms, uint32_t *out) {
    uint32_t n = (SAMPLE_RATE * dur_ms)/1000; if(n>48000) n=48000;
    uint32_t step = (freq*SINE_LEN*256)/SAMPLE_RATE, phase=0;
    for (uint32_t i=0;i<n;i++) {
        uint32_t idx=(phase>>8)&(SINE_LEN-1), frac=phase&0xFF;
        int32_t s0=sine_table[idx], s1=sine_table[(idx+1)&(SINE_LEN-1)];
        test_tone_buf[i]=(int16_t)((s0+(((s1-s0)*(int32_t)frac)>>8))>>1);
        phase+=step;
    }
    *out=n; return test_tone_buf;
}

/* ================================================================== */
/* APU — unchanged from before                                         */
/* ================================================================== */
static const int8_t duty_table[4][8] = {
    {0,0,0,0,0,0,0,1},{1,0,0,0,0,0,0,1},{1,0,0,0,0,1,1,1},{0,1,1,1,1,1,1,0},
};
typedef struct { uint32_t phase,phase_step; uint8_t duty,volume,env_init_vol;
    int8_t env_dir; uint8_t env_period; uint16_t env_timer; uint8_t active; } apu_pulse_t;
typedef struct { uint32_t phase,phase_step; uint8_t wave[32],volume_shift,active; } apu_wave_t;
typedef struct { uint16_t lfsr; uint32_t timer,period; uint8_t width_mode,volume,env_init_vol;
    int8_t env_dir; uint8_t env_period; uint16_t env_timer; uint8_t active; } apu_noise_t;

static apu_pulse_t apu_pulse[2];
static apu_wave_t  apu_wave;
static apu_noise_t apu_noise;
static uint32_t    apu_env_tick = 0;

static uint32_t freq_to_step(uint32_t f) { /* Must fill full 32-bit range: duty uses top 3 bits, wave uses top 5 */
    return f * (0xFFFFFFFFU / SAMPLE_RATE); /* f * 89478 */ }

static int16_t apu_tick(void) {
    int32_t sum=0;
    for (int i=0;i<2;i++) {
        apu_pulse_t *p=&apu_pulse[i]; if(!p->active) continue;
        uint32_t idx=(p->phase>>29)&7;
        sum += (duty_table[p->duty][idx] ? (int32_t)p->volume : -(int32_t)p->volume) * 546;
        p->phase += p->phase_step;
    }
    if (apu_wave.active) {
        uint32_t idx=(apu_wave.phase>>27)&31;
        int32_t s=((int32_t)apu_wave.wave[idx]-8)>>apu_wave.volume_shift;
        sum += s*1092; apu_wave.phase += apu_wave.phase_step;
    }
    if (apu_noise.active) {
        apu_noise.timer++;
        if (apu_noise.timer>=apu_noise.period) { apu_noise.timer=0;
            uint16_t x=(apu_noise.lfsr^(apu_noise.lfsr>>1))&1;
            apu_noise.lfsr=(apu_noise.lfsr>>1)|(x<<14);
            if(apu_noise.width_mode) apu_noise.lfsr=(apu_noise.lfsr&~(1<<6))|(x<<6);
        }
        sum+=((apu_noise.lfsr&1)?(int32_t)apu_noise.volume:-(int32_t)apu_noise.volume)*546;
    }
    if(sum>32767) sum=32767;
    if(sum<-32768) sum=-32768;
    return (int16_t)sum;
}
#define ENV_INTERVAL 750
static void apu_envelope_tick(void) {
    for(int i=0;i<2;i++){apu_pulse_t*p=&apu_pulse[i];
        if(!p->active||p->env_dir==0||p->env_period==0)continue;
        if(++p->env_timer>=p->env_period){p->env_timer=0;
            int8_t nv=(int8_t)p->volume+p->env_dir;if(nv>=0&&nv<=15)p->volume=nv;}}
    {apu_noise_t*n=&apu_noise;if(!n->active||n->env_dir==0||n->env_period==0)return;
        if(++n->env_timer>=n->env_period){n->env_timer=0;
            int8_t nv=(int8_t)n->volume+n->env_dir;if(nv>=0&&nv<=15)n->volume=nv;}}
}
void audio_apu_mix(uint32_t num) {
    for(uint32_t s=0;s<num;s++){
        int32_t out=((int32_t)apu_tick()*master_volume)>>8;
        mix_buf[mix_wr++&MIX_BUF_MASK]=(int16_t)out;
        if(++apu_env_tick>=ENV_INTERVAL){apu_env_tick=0;apu_envelope_tick();}
    }
}
void audio_apu_note_on(uint32_t ch,uint32_t f,uint8_t v,uint8_t d){
    if(ch>1) return;
    apu_pulse_t*p=&apu_pulse[ch];
    p->phase=0;p->phase_step=freq_to_step(f);p->duty=d&3;p->volume=v&15;
    p->env_init_vol=p->volume;p->env_dir=0;p->env_period=0;p->env_timer=0;p->active=1;}
void audio_apu_note_on_env(uint32_t ch,uint32_t f,uint8_t v,uint8_t d,int8_t ed,uint8_t ep){
    if(ch>1) return;
    apu_pulse_t*p=&apu_pulse[ch];
    p->phase=0;p->phase_step=freq_to_step(f);p->duty=d&3;p->volume=v&15;
    p->env_init_vol=p->volume;p->env_dir=ed;p->env_period=ep;p->env_timer=0;p->active=1;}
void audio_apu_note_off(uint32_t ch){if(ch<=1)apu_pulse[ch].active=0;}
void audio_apu_wave_set(const uint8_t w[32]){for(int i=0;i<32;i++)apu_wave.wave[i]=w[i]&0xF;}
void audio_apu_wave_on(uint32_t f,uint8_t vs){
    apu_wave.phase=0;apu_wave.phase_step=freq_to_step(f);apu_wave.volume_shift=vs&3;apu_wave.active=1;}
void audio_apu_wave_off(void){apu_wave.active=0;}
void audio_apu_noise_on(uint8_t v,uint8_t pc,uint8_t wm){
    apu_noise.lfsr=0x7FFF;apu_noise.timer=0;
    uint32_t d[]={1,2,4,6,8,10,12,14};apu_noise.period=(pc<8)?d[pc]:14;
    apu_noise.width_mode=wm;apu_noise.volume=v&15;apu_noise.env_init_vol=apu_noise.volume;
    apu_noise.env_dir=0;apu_noise.env_period=0;apu_noise.env_timer=0;apu_noise.active=1;}
void audio_apu_noise_on_env(uint8_t v,uint8_t pc,uint8_t wm,int8_t ed,uint8_t ep){
    audio_apu_noise_on(v,pc,wm);apu_noise.env_dir=ed;apu_noise.env_period=ep;}
void audio_apu_noise_off(void){apu_noise.active=0;}
void audio_apu_all_off(void){apu_pulse[0].active=0;apu_pulse[1].active=0;apu_wave.active=0;apu_noise.active=0;}

/* ================================================================== */
/* Mode 4: Genesis Sound (YM2612 FM + SN76489 PSG)                     */
/*                                                                     */
/* YM2612: 6 channels × 4 operators, 8 algorithms, ADSR envelopes     */
/* PSG:    3 square tones + 1 noise (wraps existing APU code)          */
/* ================================================================== */

/* ---- 256-entry quarter sine table (0 to π/2) ---- */
const int16_t fm_qsine[256] = {
        0,   201,   402,   603,   804,  1005,  1206,  1407,
     1608,  1809,  2009,  2210,  2410,  2611,  2811,  3012,
     3212,  3412,  3612,  3811,  4011,  4210,  4410,  4609,
     4808,  5007,  5205,  5404,  5602,  5800,  5998,  6195,
     6393,  6590,  6786,  6983,  7179,  7375,  7571,  7767,
     7962,  8157,  8351,  8545,  8739,  8933,  9126,  9319,
     9512,  9704,  9896, 10087, 10278, 10469, 10659, 10849,
    11039, 11228, 11417, 11605, 11793, 11980, 12167, 12353,
    12539, 12725, 12910, 13094, 13279, 13462, 13645, 13828,
    14010, 14191, 14372, 14553, 14732, 14912, 15090, 15269,
    15446, 15623, 15800, 15976, 16151, 16325, 16499, 16673,
    16846, 17018, 17189, 17360, 17530, 17700, 17869, 18037,
    18204, 18371, 18537, 18703, 18868, 19032, 19195, 19357,
    19519, 19680, 19841, 20000, 20159, 20317, 20475, 20631,
    20787, 20942, 21096, 21250, 21403, 21554, 21705, 21856,
    22005, 22154, 22301, 22448, 22594, 22739, 22884, 23027,
    23170, 23311, 23452, 23592, 23731, 23870, 24007, 24143,
    24279, 24413, 24547, 24680, 24811, 24942, 25072, 25201,
    25329, 25456, 25582, 25708, 25832, 25955, 26077, 26198,
    26319, 26438, 26556, 26674, 26790, 26905, 27019, 27133,
    27245, 27356, 27466, 27575, 27683, 27790, 27896, 28001,
    28105, 28208, 28310, 28411, 28510, 28609, 28706, 28803,
    28898, 28992, 29085, 29177, 29268, 29358, 29447, 29534,
    29621, 29706, 29791, 29874, 29956, 30037, 30117, 30195,
    30273, 30349, 30424, 30498, 30571, 30643, 30714, 30783,
    30852, 30919, 30985, 31050, 31113, 31176, 31237, 31297,
    31356, 31414, 31470, 31526, 31580, 31633, 31685, 31736,
    31785, 31833, 31880, 31926, 31971, 32014, 32057, 32098,
    32137, 32176, 32213, 32250, 32285, 32318, 32351, 32382,
    32412, 32441, 32469, 32495, 32521, 32545, 32567, 32589,
    32609, 32628, 32646, 32663, 32678, 32692, 32705, 32717,
    32728, 32737, 32745, 32752, 32757, 32761, 32765, 32766,
};

/* Full sine lookup from quarter table: 10-bit phase -> signed 16-bit */
static inline __attribute__((always_inline)) int16_t fm_sine(uint32_t phase10)
{
    uint32_t idx = phase10 & 0x3FF;
    uint32_t pos = idx & 0xFF;
    int16_t val = fm_qsine[(idx & 0x100) ? (255 - pos) : pos];
    return (idx & 0x200) ? -val : val;
}

/* ---- Envelope ---- */
enum { ENV_OFF, ENV_ATK, ENV_DEC, ENV_SUS, ENV_REL };

/* Rate table: maps rate 0-31 to envelope step per sample.
 * Higher rate = faster. Roughly exponential scaling. */
const uint16_t env_rate_tab[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28,
    32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 256, 320, 512, 768, 1023,
};

/* ---- FM Operator ---- */
/* fm_op_t now in jupiter.h */

/* ---- FM Channel ---- */
#define FM_NUM_CH 6
/* fm_ch_t now in jupiter.h */

fm_ch_t fm_ch[FM_NUM_CH];

/* Envelope prescaler: real YM2612 advances envelope every 3rd sample
 * (1/3 the phase generator rate). Without this, envelopes run 3× too fast.
 * fm_env_tick_now is set per-sample by the mix loops; non-zero = tick now.
 * Visible to genesis_asm.S as well. */
uint8_t fm_env_tick_now = 1;
static uint8_t fm_env_div_count = 0;

/* ---- PSG (SN76489 — reuse APU pulse/noise) ---- */
/* PSG tones are just square waves. We reuse APU pulse channels 0-1
 * plus add a third. PSG noise reuses APU noise. */
typedef struct {
    uint32_t phase;
    uint32_t phase_step;
    uint8_t  vol;            /* 0=max, 15=off (Genesis convention) */
    uint8_t  active;
} psg_tone_t;

psg_tone_t psg_tone[3];
apu_noise_t psg_noise_ch;  /* reuse noise struct */

/* ---- Struct layout assertions ----
 * genesis_asm.S hardcodes byte offsets and sizes for the FM/PSG structures.
 * If a struct member is reordered, padding changes, or a field is added,
 * these checks fire at compile time so the ASM doesn't silently corrupt state.
 * Mirrors:
 *   .equ OPSZ, 20  / OP_PH=0 OP_PI=4 OP_MUL=8 OP_DET=9 OP_TL=10
 *                  / OP_AR=11 OP_DR=12 OP_SR=13 OP_RR=14 OP_SL=15
 *                  / OP_ENV=16 OP_EST=18
 *   .equ CHSZ, 100 / CH_ALG=84 CH_FB=85 CH_FO0=88 CH_FO1=92 CH_ACT=96
 *   psg_tone:  +12 stride, vol@8, active@9
 *   psg_noise (apu_noise_t): lfsr@0 timer@4 period@8 volume@13 active@20
 */
_Static_assert(sizeof(fm_op_t) == 20, "fm_op_t must be 20 bytes (genesis_asm.S OPSZ)");
_Static_assert(__builtin_offsetof(fm_op_t, phase)     == 0,  "fm_op_t.phase offset");
_Static_assert(__builtin_offsetof(fm_op_t, phase_inc) == 4,  "fm_op_t.phase_inc offset");
_Static_assert(__builtin_offsetof(fm_op_t, mul)       == 8,  "fm_op_t.mul offset");
_Static_assert(__builtin_offsetof(fm_op_t, detune)    == 9,  "fm_op_t.detune offset");
_Static_assert(__builtin_offsetof(fm_op_t, tl)        == 10, "fm_op_t.tl offset");
_Static_assert(__builtin_offsetof(fm_op_t, ar)        == 11, "fm_op_t.ar offset");
_Static_assert(__builtin_offsetof(fm_op_t, dr)        == 12, "fm_op_t.dr offset");
_Static_assert(__builtin_offsetof(fm_op_t, sr)        == 13, "fm_op_t.sr offset");
_Static_assert(__builtin_offsetof(fm_op_t, rr)        == 14, "fm_op_t.rr offset");
_Static_assert(__builtin_offsetof(fm_op_t, sl)        == 15, "fm_op_t.sl offset");
_Static_assert(__builtin_offsetof(fm_op_t, env)       == 16, "fm_op_t.env offset");
_Static_assert(__builtin_offsetof(fm_op_t, env_state) == 18, "fm_op_t.env_state offset");

_Static_assert(sizeof(fm_ch_t) == 100, "fm_ch_t must be 100 bytes (genesis_asm.S CHSZ)");
_Static_assert(__builtin_offsetof(fm_ch_t, op)        == 0,  "fm_ch_t.op offset");
_Static_assert(__builtin_offsetof(fm_ch_t, freq_step) == 80, "fm_ch_t.freq_step offset");
_Static_assert(__builtin_offsetof(fm_ch_t, algorithm) == 84, "fm_ch_t.algorithm offset (CH_ALG)");
_Static_assert(__builtin_offsetof(fm_ch_t, feedback)  == 85, "fm_ch_t.feedback offset (CH_FB)");
_Static_assert(__builtin_offsetof(fm_ch_t, fb_out)    == 88, "fm_ch_t.fb_out offset (CH_FO0)");
_Static_assert(__builtin_offsetof(fm_ch_t, active)    == 96, "fm_ch_t.active offset (CH_ACT)");

_Static_assert(sizeof(psg_tone_t) == 12, "psg_tone_t must be 12 bytes (genesis_asm.S stride)");
_Static_assert(__builtin_offsetof(psg_tone_t, phase)      == 0, "psg_tone_t.phase offset");
_Static_assert(__builtin_offsetof(psg_tone_t, phase_step) == 4, "psg_tone_t.phase_step offset");
_Static_assert(__builtin_offsetof(psg_tone_t, vol)        == 8, "psg_tone_t.vol offset");
_Static_assert(__builtin_offsetof(psg_tone_t, active)     == 9, "psg_tone_t.active offset");

_Static_assert(__builtin_offsetof(apu_noise_t, lfsr)   == 0,  "apu_noise_t.lfsr offset");
_Static_assert(__builtin_offsetof(apu_noise_t, timer)  == 4,  "apu_noise_t.timer offset");
_Static_assert(__builtin_offsetof(apu_noise_t, period) == 8,  "apu_noise_t.period offset");
_Static_assert(__builtin_offsetof(apu_noise_t, volume) == 13, "apu_noise_t.volume offset");
_Static_assert(__builtin_offsetof(apu_noise_t, active) == 20, "apu_noise_t.active offset");

/* ---- Envelope tick for one operator ---- */
static inline __attribute__((always_inline)) void fm_env_tick(fm_op_t *op)
{
    if (!fm_env_tick_now) return;  /* prescaler: ~16 kHz at 48 kHz output */
    switch (op->env_state) {
    case ENV_ATK: {
        uint16_t rate = env_rate_tab[op->ar & 31];
        if (rate == 0) break;
        /* YM2612 attack: exponential decrease toward 0 (not linear).
         * dec = rate * (1 + env/16) — fast at top, slows near full volume.
         * Reference: jsgroth.dev/blog/posts/emulating-ym2612-part-3/ */
        uint16_t dec = rate + ((uint16_t)((uint32_t)rate * op->env >> 4));
        if (op->env > dec)
            op->env -= dec;
        else {
            op->env = 0;
            op->env_state = ENV_DEC;
        }
        break;
    }
    case ENV_DEC: {
        uint16_t target = (uint16_t)(op->sl & 15) * 64; /* SL 0-15 → 0-960 */
        uint16_t rate = env_rate_tab[op->dr & 31];
        if (op->env < target) {
            op->env += rate;
            if (op->env >= target) {
                op->env = target;
                op->env_state = ENV_SUS;
            }
        } else {
            op->env_state = ENV_SUS;
        }
        break;
    }
    case ENV_SUS: {
        uint16_t rate = env_rate_tab[op->sr & 31];
        if (op->env < 1023) {
            op->env += rate;
            if (op->env > 1023) op->env = 1023;
        }
        break;
    }
    case ENV_REL: {
        uint16_t rate = env_rate_tab[(op->rr & 15) * 2 + 1];
        if (op->env < 1023) {
            op->env += rate;
            if (op->env > 1023) {
                op->env = 1023;
                op->env_state = ENV_OFF;
            }
        }
        break;
    }
    default:
        op->env = 1023;
        break;
    }
}

/* ---- Compute one FM operator output ---- */
static __attribute__((noinline)) int32_t fm_op_calc(fm_op_t *op, int32_t modulation)
{
    if (op->env_state == ENV_OFF) return 0;

    /* Phase with modulation (modulation is ±32767 range, scale to phase) */
    uint32_t ph = op->phase + (uint32_t)(modulation << 5);
    uint32_t phase10 = (ph >> 22) & 0x3FF;

    /* Sine lookup */
    int32_t out = fm_sine(phase10);

    /* Apply envelope: env 0=full, 1023=silent */
    out = (out * (1023 - (int32_t)op->env)) >> 10;

    /* Apply total level: tl 0=full, 127=silent */
    out = (out * (127 - (int32_t)op->tl)) >> 7;

    /* Advance phase with multiplier */
    uint32_t mul_inc = (op->mul == 0) ? (op->phase_inc >> 1)
                                      : (op->phase_inc * op->mul);
    op->phase += mul_inc + op->detune;

    /* Tick envelope (simplified: every sample, not every N) */
    fm_env_tick(op);

    return out;
}

/* ---- Compute one sample from an FM channel (all 8 algorithms) ---- */
static __attribute__((noinline)) int32_t fm_ch_tick(fm_ch_t *ch)
{
    if (!ch->active) return 0;

    fm_op_t *o = ch->op;
    int32_t fb = 0;
    int32_t out = 0;
    int32_t o1, o2, o3, o4;

    /* OP1 self-feedback */
    if (ch->feedback > 0) {
        fb = (ch->fb_out[0] + ch->fb_out[1]) >> (10 - ch->feedback);
    }

    /*
     * YM2612 algorithms (C = carrier → output, M = modulator):
     * 0: [1→2→3→4]          4 is carrier
     * 1: [1+2]→3→4          1,2 parallel into 3→4
     * 2: [2→3]+[1→]→4       1 direct to 4, 2→3 also to 4
     * 3: [1→2]+[3→]→4       1→2 out, 3 direct to 4
     * 4: [1→2]+[3→4]        two serial pairs, both carriers
     * 5: 1→[2]+[3]+[4]      1 modulates all, 2+3+4 are carriers
     * 6: [1→2]+3+4          2,3,4 are carriers
     * 7: 1+2+3+4            all carriers
     */
    switch (ch->algorithm) {
    case 0:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], o1);
        o3 = fm_op_calc(&o[2], o2);
        o4 = fm_op_calc(&o[3], o3);
        out = o4;
        break;
    case 1:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], 0);
        o3 = fm_op_calc(&o[2], o1 + o2);
        o4 = fm_op_calc(&o[3], o3);
        out = o4;
        break;
    case 2:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], 0);
        o3 = fm_op_calc(&o[2], o2);
        o4 = fm_op_calc(&o[3], o1 + o3);
        out = o4;
        break;
    case 3:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], o1);
        o3 = fm_op_calc(&o[2], 0);
        o4 = fm_op_calc(&o[3], o2 + o3);
        out = o4;
        break;
    case 4:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], o1);
        o3 = fm_op_calc(&o[2], 0);
        o4 = fm_op_calc(&o[3], o3);
        out = o2 + o4;
        break;
    case 5:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], o1);
        o3 = fm_op_calc(&o[2], o1);
        o4 = fm_op_calc(&o[3], o1);
        out = o2 + o3 + o4;
        break;
    case 6:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], o1);
        o3 = fm_op_calc(&o[2], 0);
        o4 = fm_op_calc(&o[3], 0);
        out = o2 + o3 + o4;
        break;
    case 7:
        o1 = fm_op_calc(&o[0], fb);
        o2 = fm_op_calc(&o[1], 0);
        o3 = fm_op_calc(&o[2], 0);
        o4 = fm_op_calc(&o[3], 0);
        out = o1 + o2 + o3 + o4;
        break;
    default:
        o1 = fm_op_calc(&o[0], fb);
        (void)o1;
        out = 0;
        break;
    }

    /* Update feedback history (always from OP1) */
    ch->fb_out[1] = ch->fb_out[0];
    ch->fb_out[0] = o1;

    return out;
}

/* ---- PSG tone tick ---- */
static int16_t psg_tone_tick(psg_tone_t *t)
{
    if (!t->active || t->vol >= 15) return 0;
    int32_t out = (t->phase & 0x80000000) ? 1 : -1;
    int32_t atten = 15 - t->vol;  /* 0=silent, 15=max */
    out = out * atten * 546;      /* scale to ±8190 range */
    t->phase += t->phase_step;
    return (int16_t)out;
}

/* ---- Genesis mixer: FM + PSG → ring buffer ---- */
void audio_genesis_mix(uint32_t num_samples)
{
    for (uint32_t s = 0; s < num_samples; s++) {
        /* Envelope prescaler: tick every 3rd sample (matches YM2612 hw) */
        fm_env_tick_now = (fm_env_div_count == 0) ? 1 : 0;
        if (++fm_env_div_count >= 3) fm_env_div_count = 0;

        int32_t sum = 0;

        /* FM channels */
        for (int c = 0; c < FM_NUM_CH; c++) {
            sum += fm_ch_tick(&fm_ch[c]);
        }

        /* PSG tones */
        for (int t = 0; t < 3; t++) {
            sum += psg_tone_tick(&psg_tone[t]);
        }

        /* PSG noise (reuse APU noise) */
        if (psg_noise_ch.active) {
            psg_noise_ch.timer++;
            if (psg_noise_ch.timer >= psg_noise_ch.period) {
                psg_noise_ch.timer = 0;
                uint16_t x = (psg_noise_ch.lfsr ^ (psg_noise_ch.lfsr >> 1)) & 1;
                psg_noise_ch.lfsr = (psg_noise_ch.lfsr >> 1) | (x << 14);
                if (psg_noise_ch.width_mode)
                    psg_noise_ch.lfsr = (psg_noise_ch.lfsr & ~(1<<6)) | (x<<6);
            }
            int32_t nout = (psg_noise_ch.lfsr & 1)
                         ? (int32_t)psg_noise_ch.volume
                         : -(int32_t)psg_noise_ch.volume;
            sum += nout * 546;
        }

        /* Master volume + clip */
        sum = (sum * master_volume) >> 8;
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        mix_buf[mix_wr++ & MIX_BUF_MASK] = (int16_t)sum;
    }
}

/* ---- FM public API ---- */

void audio_fm_set_algorithm(uint32_t ch, uint8_t alg, uint8_t fb)
{
    if (ch >= FM_NUM_CH) return;
    fm_ch[ch].algorithm = alg & 7;
    fm_ch[ch].feedback = fb & 7;
}

void audio_fm_set_freq(uint32_t ch, uint32_t freq_hz)
{
    if (ch >= FM_NUM_CH) return;
    fm_ch[ch].freq_step = freq_hz * (0xFFFFFFFFU / SAMPLE_RATE);
    for (int i = 0; i < 4; i++)
        fm_ch[ch].op[i].phase_inc = fm_ch[ch].freq_step;
}

void audio_fm_set_operator(uint32_t ch, uint32_t op,
    uint8_t mul, uint8_t tl, uint8_t ar, uint8_t dr,
    uint8_t sl, uint8_t sr, uint8_t rr)
{
    if (ch >= FM_NUM_CH || op >= 4) return;
    fm_op_t *o = &fm_ch[ch].op[op];
    o->mul = mul & 15;
    o->tl  = tl & 127;
    o->ar  = ar & 31;
    o->dr  = dr & 31;
    o->sl  = sl & 15;
    o->sr  = sr & 31;
    o->rr  = rr & 15;
    o->detune = 0;
}

void audio_fm_key_on(uint32_t ch)
{
    if (ch >= FM_NUM_CH) return;
    fm_ch[ch].active = 1;
    for (int i = 0; i < 4; i++) {
        fm_ch[ch].op[i].phase = 0;
        fm_ch[ch].op[i].env = 1023;
        fm_ch[ch].op[i].env_state = ENV_ATK;
    }
    fm_ch[ch].fb_out[0] = fm_ch[ch].fb_out[1] = 0;
}

void audio_fm_key_off(uint32_t ch)
{
    if (ch >= FM_NUM_CH) return;
    for (int i = 0; i < 4; i++) {
        if (fm_ch[ch].op[i].env_state != ENV_OFF)
            fm_ch[ch].op[i].env_state = ENV_REL;
    }
}

/* ---- PSG API ---- */

void audio_psg_tone_on(uint32_t ch, uint32_t freq_hz, uint8_t vol)
{
    if (ch >= 3) return;
    psg_tone[ch].phase = 0;
    psg_tone[ch].phase_step = freq_hz * (0xFFFFFFFFU / SAMPLE_RATE);
    psg_tone[ch].vol = vol & 15;
    psg_tone[ch].active = 1;
}

void audio_psg_tone_off(uint32_t ch)
{
    if (ch >= 3) return;
    psg_tone[ch].active = 0;
}

void audio_psg_noise_on(uint8_t vol, uint8_t rate, uint8_t periodic)
{
    psg_noise_ch.lfsr = 0x7FFF;
    psg_noise_ch.timer = 0;
    uint32_t d[] = {1, 2, 4, 8, 16, 32, 64, 128};
    psg_noise_ch.period = (rate < 8) ? d[rate] : 128;
    psg_noise_ch.width_mode = periodic;
    psg_noise_ch.volume = (15 - (vol & 15));  /* invert: 0=off,15=max → internal */
    psg_noise_ch.active = 1;
}

void audio_psg_noise_off(void)
{
    psg_noise_ch.active = 0;
}

void audio_genesis_all_off(void)
{
    for (int c = 0; c < FM_NUM_CH; c++) {
        fm_ch[c].active = 0;
        for (int o = 0; o < 4; o++)
            fm_ch[c].op[o].env_state = ENV_OFF;
    }
    for (int t = 0; t < 3; t++)
        psg_tone[t].active = 0;
    psg_noise_ch.active = 0;
}

/* Reprogram PLL_AUDIO for a specific DAC sample rate.
 * PLL register values pre-computed from working 48kHz reference.
 * Non-static so audio_pcm_play_rate (declared above) can read both:
 *   current_audio_rate     — rounded integer Hz (legacy / display)
 *   current_audio_rate_q8  — Q8 (rate × 256) for resampler precision */
uint32_t current_audio_rate    = 48000;
uint32_t current_audio_rate_q8 = 48000 * 256;

void audio_set_rate(uint32_t rate)
{
    /* current_audio_rate is set to the *actual* DAC frequency below
     * (after the table lookup), so callers can request a label rate
     * (e.g. 11025) and the resampler still computes pos_step against
     * what the codec is physically producing. */

    /* `dac_fs` (bits 31:29 of SUN4I_DAC_FIFOC at 0x01C22C04) is the
     * codec's internal MCLK-to-DAC divider:
     *   0=÷512, 1=÷1024, 2=÷2048, 3=÷128, 4=÷256, 5=÷3072, 6=÷6144, 7=÷12288
     * Most rates land via PLL alone (fs=0, ÷512). Sub-22 kHz rates need
     * a small PLL the V3s can't lock — use a higher PLL with a larger
     * fs divider instead. */
    /* actual_rate_q8 is the rate the DAC physically produces, in Q8
     * fixed-point (rate × 256) so we keep sub-Hz precision through the
     * resampler math. Integer-truncated rates accumulate ~0.06% error
     * vs. the real PLL/divisor product, which is audible on sustained
     * voice. Q8 brings the resampler error to <0.001%. */
    static const struct {
        uint32_t label;            /* what callers pass */
        uint32_t actual_rate_q8;   /* DAC rate × 256 */
        uint32_t pll_reg;
        uint8_t  dac_fs;
    } rates[] = {
        { 48000, 47991 * 256,        0x80035514, 0 },  /* P=3 N=85 M=20 ÷512 */
        { 44100, 44085 * 256,        0x80034E14, 0 },  /* P=3 N=78 M=20 ÷512 */
        { 65536, 65519 * 256,        0x80037A15, 0 },  /* P=3 N=122 M=21 ÷512 */
        { 53267, 53267 * 256,        0x80011815, 4 },  /* P=1 N=24 M=21 ÷256 */
        /* 11025: 44100 PLL (22.5714286 MHz exact = 24M·79/(4·21)) at
         * FIFOC FS=4 (÷2048) → 11021.9 Hz (0.03% flat, inaudible).
         * Earlier table used FS=2 (÷1024) → 22050 Hz codec rate, an
         * octave above target — manifested as "everything pitched up" on
         * real HW. Emu masked it because the QEMU codec model uses the
         * 24.576 MHz canonical table where FS=2 = 24 kHz, and icount
         * slowdown brought effective rate near correct accidentally. */
        { 11025, 11021 * 256,        0x80034E14, 4 },
        /* 22050: same 44100-family PLL (PLL_AUDIO = 22.5714 MHz) but
         * FIFOC FS=2 (÷1024) → 22043.6 Hz on real silicon (0.03% flat
         * from 22050, inaudible). Same theoretical-vs-measured rule
         * that applies to 11025: trust divisor=1024 over the textbook
         * "FS=2 = 24 kHz" mapping, which only holds for the 24.576 MHz
         * audio family. */
        { 22050, 22043 * 256,        0x80034E14, 2 },
    };

    uint32_t reg = 0x80035514;
    uint8_t  dac_fs = 0;
    uint32_t actual_q8 = rate * 256;  /* fallback if not in table */
    for (uint32_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        if (rates[i].label == rate) {
            reg = rates[i].pll_reg;
            dac_fs = rates[i].dac_fs;
            actual_q8 = rates[i].actual_rate_q8;
            break;
        }
    }
    if (actual_q8 == current_audio_rate_q8) {
        uart_puts("  rate unchanged ("); uart_putdec(actual_q8 / 256);
        uart_puts("Hz), skipping reinit\n");
        return;
    }
    current_audio_rate_q8 = actual_q8;
    current_audio_rate    = (actual_q8 + 128) / 256;  /* round-to-nearest, for legacy callers */

    /* 1. Stop DMA */
    DMA_CH_EN(0) = 0;
    ms(1);
    dma_running = 0;

    /* 2. PLL */
    REG32(0x01C20008) = 0;
    ms(5);
    REG32(0x01C20008) = reg;
    ms(100);

    /* 3. Module clock re-enable */
    REG32(0x01C20000+0x02D0) |= BIT(0);
    ms(5);
    REG32(0x01C20000+0x0140) = BIT(31);
    ms(5);

    /* 4. FIFO flush + reconfigure (preserve 24-bit mode if active).
     * OR in dac_fs at bits 31:29 — codec internal MCLK divider. fs=0
     * (÷512) is the typical 48k/44.1k path; fs=2 (÷2048) lets us hit
     * 11025 from a 22.571 MHz PLL that's actually within lock range. */
    REG32(0x01C22C00+0x04) = BIT(0);
    ms(1);
    uint32_t fifoc = audio_use_24bit ? 0x00600F10u : 0x01600F10u;
    fifoc |= ((uint32_t)dac_fs) << 29;
    REG32(0x01C22C00+0x04) = fifoc;
    REG32(0x01C22C00+0x00) = 0x80012000;
    REG32(0x01C22C00+0x60) = 0x60000000;

    /* 5. Reset buffer state */
    mix_wr = 0;
    mix_rd = 0;
    a_filled = 0;
    b_filled = 0;
    audio_underruns = 0;

    /* 6. Clear audio buffers */
    {
        uint32_t total = audio_use_24bit ? AUDIO_BUF_HALF * 2 * 4 : AUDIO_BUF_HALF * 2 * 2;
        uint32_t *p = (uint32_t *)AUDIO_BUF_ADDR;
        for (uint32_t i = 0; i < total / 4; i++) p[i] = 0;
        dcache_clean_range(AUDIO_BUF_ADDR, total);
    }

    /* 7. Arm DMA (caller must audio_start() after pre-filling mix_buf) */
    dcache_clean_range(DMA_DESC_A_ADDR, 64);
    DMA_CH_DESC(0) = DMA_DESC_A_ADDR;

    uart_puts("  reinit: label=");
    uart_putdec(rate);
    uart_puts("Hz actual_q8=");
    uart_putdec(actual_q8);
    uart_puts(" (");
    uart_putdec(actual_q8 / 256);
    uart_puts("Hz) PLL=");
    uart_puthex(REG32(0x01C20008));
    uart_puts(" DMA=");
    uart_puthex(DMA_CH_CUR_SRC(0));
    uart_puts(" FIFOC=");
    uart_puthex(REG32(0x01C22C00+0x04));
    uart_puts(" DPC=");
    uart_puthex(REG32(0x01C22C00+0x00));
    uart_puts("\n");
}
