/*
 * Jupiter PINE A64+ audio output.
 *
 * A64 has three separate blocks: the codec I2S transport at 0x01c22c00,
 * the AIF/DAC digital mixer at 0x01c22e00, and ADDA_PR at 0x01f015c0.
 * It does NOT have the V3s DAC FIFO/DAP register layout.
 *
 * The hardware runs at exactly 48 kHz, stereo, 24 bits in 32-bit DMA
 * words. The public ring remains interleaved signed 16-bit stereo.
 * A bounded linear resampler supports the examples' source rates,
 * including 53267 and 65536 Hz, without overclocking the codec PLL.
 *
 * Register descriptions checked against Linux v6.12 sun4i-i2s.c
 * (sun50i_a64_codec_i2s_quirks), sun8i-codec.c, sun50i-codec-analog.c,
 * sun6i-dma.c, ccu-sun50i-a64.c; PINE64+ DT and U-Boot v2025.10 RSB
 * and AXP803 regulator register definitions. Hardware sequence is an
 * original bare-metal implementation; none of the Linux framework code
 * is compiled here.
 */
#include "jupiter.h"

#define HW_RATE             48000u
#define RING_SLOTS          4096u
#define RING_MASK           (RING_SLOTS - 1u)
#define HALF_FRAMES         512u
#define HALF_SLOTS          (HALF_FRAMES * 2u)
#define HALF_BYTES          (HALF_SLOTS * 4u)
#define DESC_A              (AUDIO_BUF_ADDR + 0x10000u)
#define DESC_B              (DESC_A + 32u)

/* At the maximum accepted 192 kHz source rate, one 512-frame hardware
 * period consumes exactly the ring's 2048 source frames. Larger periods
 * could not be refilled without guaranteed underruns at high rates. */
_Static_assert(HALF_FRAMES * (192000u / HW_RATE) <= RING_SLOTS / 2u,
               "DMA period exceeds source ring capacity");

#define A64_DAI(off)        REG32(0x01c22c00u + (off))
#define A64_CODEC(off)      REG32(0x01c22e00u + (off))
#define A64_ADDA_PR         REG32(0x01f015c0u)
#define A64_RSB(off)        REG32(0x01f03400u + (off))
#define A64_PRCM(off)       REG32(0x01f01400u + (off))
#define A64_PL(off)         REG32(0x01f02c00u + (off))
#define A64_DMA(off)        REG32(0x01c02000u + (off))

/* H3/A64 DMA burst fields start at bit 6/22, unlike V3s bit 7/23.
 * SDRAM -> codec-I2S DRQ15, linear source / fixed destination,
 * 32-bit words and four-word bursts. */
#define DMA_CONFIG         (1u | (1u << 6) | (2u << 9) | \
                            (15u << 16) | BIT(21) | (1u << 22) | (2u << 25))
#define DMA_PKG            BIT(1)
#define DAI_TX             BIT(2)
#define DAI_GLOBAL         BIT(0)
#define DAI_SDO0           BIT(8)
#define DAI_TX_DRQ         BIT(7)
#define DAI_TX_FLUSH       BIT(25)

extern int16_t mix_buf[RING_SLOTS];
extern volatile uint32_t mix_wr, mix_rd, audio_underruns, audio_transitions;
extern uint32_t current_audio_rate, current_audio_rate_q8;
extern void pine64_audio_shared_reset(void);
extern int pine64_audio_pcm_active(void);

/* Kept for existing demo diagnostics. */
volatile int a_filled, b_filled;
volatile uint32_t audio_dbg_refill_a, audio_dbg_refill_b;
volatile uint32_t audio_dbg_alt_break_a, audio_dbg_alt_break_b;
volatile uint32_t audio_dbg_isr_entries, audio_dbg_no_pkg;
volatile uint32_t audio_dbg_cursrc_disagree;

static uint8_t codec_ready, dma_running;
static uint8_t hp_volume = 43;
static uint8_t next_refill, last_refill = 2;
static uint64_t source_step = (uint64_t)1 << 32;
static uint32_t source_fraction;

static uint32_t lock_irq(void)
{
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr\n\tcpsid i" : "=r"(cpsr) :: "memory");
    return cpsr;
}

static void unlock_irq(uint32_t cpsr)
{
    __asm__ volatile("dmb ish" ::: "memory");
    if (!(cpsr & BIT(7))) __asm__ volatile("cpsie i" ::: "memory");
}

static void barrier(void) { __asm__ volatile("dsb sy" ::: "memory"); }

/* Every peripheral wait has both a timer deadline and a finite poll
 * bound, including the early-start case where timer0 has not advanced. */
static int wait_mask(uint32_t addr, uint32_t mask, uint32_t value, uint32_t us)
{
    uint32_t start = timer_read();
    for (uint32_t polls = 0; polls < 2000000u; ++polls) {
        if ((REG32(addr) & mask) == value) return 0;
        if (ticks_to_us(timer_elapsed(start, timer_read())) >= us) break;
    }
    return -1;
}

static void delay_us_bounded(uint32_t us)
{
    uint32_t start = timer_read();
    for (uint32_t polls = 0; polls < 4000000u; ++polls)
        if (ticks_to_us(timer_elapsed(start, timer_read())) >= us) break;
}

static uint8_t analog_read(uint8_t reg)
{
    uint32_t val = A64_ADDA_PR;
    val &= ~(BIT(24) | (0x1fu << 16));
    val |= BIT(28) | ((uint32_t)(reg & 0x1f) << 16);
    A64_ADDA_PR = val;
    barrier();
    return (uint8_t)A64_ADDA_PR;
}

static void analog_write(uint8_t reg, uint8_t value)
{
    uint32_t val = A64_ADDA_PR;
    val &= ~(BIT(24) | (0x1fu << 16) | (0xffu << 8));
    val |= BIT(28) | ((uint32_t)(reg & 0x1f) << 16) | ((uint32_t)value << 8);
    A64_ADDA_PR = val;
    A64_ADDA_PR = val | BIT(24);
    barrier();
    A64_ADDA_PR = val;
    barrier();
}

static int rsb_complete(void)
{
    uint32_t start = timer_read();
    uint32_t stat = 0;
    for (uint32_t polls = 0; polls < 200000u; ++polls) {
        stat = A64_RSB(0x0c);
        if (stat & 7u) break;
        if (ticks_to_us(timer_elapsed(start, timer_read())) >= 20000u) break;
    }
    A64_RSB(0x0c) = stat & 7u;
    return ((stat & 7u) == 1u) ? 0 : -1;
}

static int rsb_transfer(uint8_t command, uint8_t reg, uint8_t *value)
{
    if (A64_RSB(0x00) & BIT(7)) return -1;
    A64_RSB(0x08) = 0;  /* polling only */
    A64_RSB(0x0c) = 7;
    A64_RSB(0x30) = 0x2du << 16;
    A64_RSB(0x10) = reg;
    if (command == 0x4e) A64_RSB(0x1c) = *value;
    A64_RSB(0x2c) = command;
    barrier();
    A64_RSB(0x00) |= BIT(7);
    if (rsb_complete() < 0) return -1;
    if (command == 0x8b) *value = (uint8_t)A64_RSB(0x1c);
    return 0;
}

static int headphone_supply(void)
{
    uint8_t id, voltage, enable;

    /* PL0/PL1 are the board's dedicated PMIC RSB pins. Preserve all
     * unrelated pins, PRCM clock gates, PMIC supplies and rail settings. */
    A64_PRCM(0x28) |= BIT(0) | BIT(3);
    A64_PRCM(0xb0) |= BIT(0) | BIT(3);
    A64_PL(0x00) = (A64_PL(0x00) & ~0xffu) | 0x22u;
    A64_PL(0x14) = (A64_PL(0x14) & ~0x0fu) | 0x0au;
    A64_PL(0x1c) = (A64_PL(0x1c) & ~0x0fu) | 0x05u;
    barrier();

    /* Reuse a firmware-established runtime address when present. */
    if (rsb_transfer(0x8b, 0x03, &id) < 0) {
        A64_RSB(0x00) = BIT(0);
        if (wait_mask(0x01f03400u, BIT(0), 0, 10000) < 0) return -1;
        A64_RSB(0x04) = (1u << 8) | 3u;  /* 24 MHz / 2 / 4 = 3 MHz */
        A64_RSB(0x08) = 0;
        A64_RSB(0x0c) = 7;
        A64_RSB(0x28) = BIT(31) | 0x007c3e00u;
        if (wait_mask(0x01f03428u, BIT(31), 0, 20000) < 0 ||
            rsb_complete() < 0) return -1;
        A64_RSB(0x0c) = 7;
        A64_RSB(0x30) = (0x2du << 16) | 0x3a3u;
        A64_RSB(0x2c) = 0xe8;  /* SET_RUNTIME_ADDRESS */
        A64_RSB(0x00) |= BIT(7);
        if (rsb_complete() < 0 || rsb_transfer(0x8b, 0x03, &id) < 0) return -1;
    }

    if ((id & 0xcfu) != 0x41u) {
        uart_puts("[audio/pine64] unexpected PMIC ID "); uart_puthex(id);
        uart_puts("; headphone supply left unchanged\n");
        return -1;
    }
    if (rsb_transfer(0x8b, 0x19, &voltage) < 0 ||
        rsb_transfer(0x8b, 0x12, &enable) < 0) return -1;
    voltage = (voltage & ~0x1fu) | 22u;  /* 700 + 22*50 = 1800 mV */
    enable |= BIT(0);
    if (rsb_transfer(0x4e, 0x19, &voltage) < 0 ||
        rsb_transfer(0x4e, 0x12, &enable) < 0 ||
        rsb_transfer(0x8b, 0x19, &voltage) < 0 ||
        rsb_transfer(0x8b, 0x12, &enable) < 0) return -1;
    if ((voltage & 0x1f) != 22 || !(enable & BIT(0))) return -1;
    delay_us_bounded(10000);
    return 0;
}

void audio_set_hp_vol(uint8_t volume)
{
    if (volume > 63) volume = 63;
    hp_volume = volume;
    if (codec_ready) {
        uint32_t saved = lock_irq();
        uint8_t ctrl = analog_read(0x00);
        analog_write(0x00, (ctrl & 0xc0u) | volume);
        unlock_irq(saved);
    }
}

/* Public board cleanup hook: the menu calls this before leaving a demo.
 * No global DMA reset; channels owned by other subsystems stay intact. */
void pine64_audio_stop(void)
{
    uint32_t saved = lock_irq();
    if (codec_ready) {
        A64_DAI(0x1c) &= ~DAI_TX_DRQ;
        A64_DAI(0x00) &= ~DAI_TX;
        analog_write(0x0a, analog_read(0x0a) & ~0x0cu);
    }
    if (REG32(CCU_BASE + 0x60) & BIT(6)) {
        DMA_IRQ_EN(0) &= ~7u;
        DMA_CH_EN(0) = 0;
        DMA_CH_PAUSE(0) = 0;
        DMA_IRQ_STAT(0) = 7;
        barrier();
        (void)wait_mask(0x01c02100u, 1, 0, 10000);
    }
    irq_disable(IRQ_DMA);
    dma_running = 0;
    a_filled = b_filled = 0;
    unlock_irq(saved);
}

static int codec_init(void)
{
    /* The headphone charge pump is a real board power supply, not a
     * codec register. Its verified readback is required before unmute. */
    if (headphone_supply() < 0) {
        uart_puts("[audio/pine64] AXP803 headphone supply initialization failed\n");
        return -1;
    }
    REG32(CCU_BASE + 0x68) |= BIT(0);
    REG32(CCU_BASE + 0x2d0) &= ~BIT(0);
    REG32(CCU_BASE + 0x140) &= ~BIT(31);
    barrier();

    /* A64 SDM settings from ccu-sun50i-a64.c: N=14, M=14,
     * P=1 (register values N-1, M-1, P-1), exact 24.576 MHz. */
    REG32(CCU_BASE + 0x08) = 0;
    REG32(CCU_BASE + 0x284) = 0xc000ac02u;
    REG32(CCU_BASE + 0x08) = BIT(31) | BIT(24) | (13u << 8) | 13u;
    if (wait_mask(CCU_BASE + 0x08, BIT(28), BIT(28), 100000) < 0) {
        uart_puts("[audio/pine64] PLL_AUDIO lock timeout\n");
        return -1;
    }
    REG32(CCU_BASE + 0x140) |= BIT(31);
    REG32(CCU_BASE + 0x2d0) |= BIT(0);
    delay_us_bounded(1000);

    A64_DAI(0x00) = 0;
    A64_DAI(0x1c) = 0;
    A64_DAI(0x04) = (2u << 4) | (3u << 2); /* 24 valid bits, 32-bit slots, I2S */
    A64_DAI(0x08) = 0;
    A64_DAI(0x14) = (32u << 12) | BIT(2) | DAI_TX_FLUSH;
    if (wait_mask(0x01c22c14u, DAI_TX_FLUSH, 0, 10000) < 0) {
        uart_puts("[audio/pine64] codec-I2S FIFO flush timeout\n");
        return -1;
    }
    A64_DAI(0x24) = BIT(7) | 2u; /* MCLK /4=6.144MHz; BCLK /2=3.072MHz */
    A64_DAI(0x28) = 0;
    A64_DAI(0x30) = 1;          /* two channels */
    A64_DAI(0x34) = 0x76543210u;
    A64_DAI(0x00) = DAI_GLOBAL | DAI_SDO0; /* master mode; TX still stopped */

    A64_CODEC(0x010) = 0;
    A64_CODEC(0x014) = 0;
    A64_CODEC(0x00c) = BIT(11) | (2u << 8) | BIT(3); /* AIF1 PLL -> SYSCLK */
    A64_CODEC(0x018) = 8u << 12; /* 48 kHz DAC filtering */
    A64_CODEC(0x040) = BIT(15) | (4u << 9) | (2u << 6) | (3u << 4);
    /* Codec slave, BCLK /8 bookkeeping, LRCK /64, 24-bit I2S, normal polarity. */
    A64_CODEC(0x048) = BIT(15) | BIT(14); /* AIF1 DAC0 stereo, straight L/R */
    A64_CODEC(0x058) = 0xa0a0u;  /* AIF1 DAC0 volume: 0 dB */
    A64_CODEC(0x124) = 0xa0a0u;  /* DAC volume: 0 dB */
    A64_CODEC(0x130) = BIT(15) | BIT(11); /* DAC L/R mixer from AIF1 slot0 */
    A64_CODEC(0x010) = BIT(15) | BIT(2);  /* AIF1 + DAC module clocks */
    A64_CODEC(0x014) = BIT(15) | BIT(2);  /* release their module resets */
    A64_CODEC(0x120) = BIT(15);          /* ENDA */

    analog_write(0x00, hp_volume); /* PA clock ungated, amplifier initially off */
    analog_write(0x0a, 0xc0);      /* left/right analog DAC; HP switches muted */
    delay_us_bounded(10000);
    analog_write(0x00, BIT(6) | hp_volume); /* headphone amplifier on */
    delay_us_bounded(20000);

    REG32(CCU_BASE + 0x60) |= BIT(6);
    REG32(CCU_BASE + 0x2c0) |= BIT(6);
    A64_DMA(0x28) = 4; /* A64 clock auto gate; +0x20 is SECURITY, untouched. */
    DMA_CH_EN(0) = 0;
    DMA_CH_PAUSE(0) = 0;
    DMA_IRQ_EN(0) &= ~7u;
    DMA_IRQ_STAT(0) = 7;
    barrier();
    return 0;
}

static void prepare_descriptors(void)
{
    volatile uint32_t *words = (volatile uint32_t *)AUDIO_BUF_ADDR;
    for (uint32_t i = 0; i < HALF_SLOTS * 2u; ++i) words[i] = 0;
    dma_desc_t *a = (dma_desc_t *)DESC_A, *b = (dma_desc_t *)DESC_B;
    *a = (dma_desc_t){ DMA_CONFIG, AUDIO_BUF_ADDR, 0x01c22c20u,
                       HALF_BYTES, 0, DESC_B };
    *b = (dma_desc_t){ DMA_CONFIG, AUDIO_BUF_ADDR + HALF_BYTES, 0x01c22c20u,
                       HALF_BYTES, 0, DESC_A };
    dcache_clean_range(AUDIO_BUF_ADDR, HALF_BYTES * 2u);
    dcache_clean_range(DESC_A, 64);
    barrier();
    DMA_CH_DESC(0) = DESC_A;
    source_fraction = 0;
    next_refill = 0;
    last_refill = 2;
}

void audio_init(void)
{
    pine64_audio_stop();
    pine64_audio_shared_reset();
    current_audio_rate = HW_RATE;
    current_audio_rate_q8 = HW_RATE * 256u;
    source_step = (uint64_t)1 << 32;
    hp_volume = 43;
    audio_dbg_refill_a = audio_dbg_refill_b = 0;
    audio_dbg_alt_break_a = audio_dbg_alt_break_b = 0;
    audio_dbg_isr_entries = audio_dbg_no_pkg = audio_dbg_cursrc_disagree = 0;
    codec_ready = (codec_init() == 0);
    if (!codec_ready) return;
    prepare_descriptors();
    uart_puts("[audio/pine64] AXP803 1.8V; AIF1 -> DAC -> 3.5mm headphones\n");
    uart_puts("[audio/pine64] 48000 Hz stereo, 24-bit I2S, DMA0; ready\n");
}

void audio_init_24bit(void)
{
    /* A64 always transports 24-bit samples. The shared source ring is
     * still 16-bit, so this API does not invent extra source precision. */
    audio_init();
}

void audio_set_rate(uint32_t rate)
{
    if (rate < 8000 || rate > 192000) {
        uart_puts("[audio/pine64] unsupported source rate; rate unchanged\n");
        return;
    }
    if (current_audio_rate == rate) return;
    pine64_audio_stop();
    current_audio_rate = rate;
    current_audio_rate_q8 = rate * 256u;
    source_step = ((uint64_t)rate << 32) / HW_RATE;
    mix_wr = mix_rd = 0;
    if (codec_ready) prepare_descriptors();
    uart_puts("[audio/pine64] source "); uart_putdec(rate);
    uart_puts(" Hz -> hardware 48000 Hz\n");
}

static void refill(unsigned half, int count_underrun)
{
    int32_t *out = (int32_t *)(AUDIO_BUF_ADDR + half * HALF_BYTES);
    /* Only auto-produce when the PCM API owns active channels. Filling
     * silence with audio_mix when no PCM channels exist would steal the
     * entire ring from OPN2/Munt/SC-55 and other independent producers. */
    if (pine64_audio_pcm_active()) {
        uint32_t depth = mix_wr - mix_rd;
        /* Follow the common driver's low-water policy: replenish one
         * DMA half, not every free ring slot. Account for source-rate
         * resampling (the hardware half always contains 512 frames at
         * 48 kHz). Shared PINE mixers mask IRQs while publishing pairs,
         * so this ISR cannot splice PCM into another producer's L/R. */
        uint32_t needed_frames = (uint32_t)
            (((uint64_t)HALF_FRAMES * current_audio_rate + HW_RATE - 1u) / HW_RATE);
        if (depth <= RING_SLOTS && depth < needed_frames * 4u) {
            uint32_t available = (RING_SLOTS - depth) / 2u;
            if (needed_frames > available) needed_frames = available;
            audio_mix(needed_frames);
        }
    }
    for (uint32_t frame = 0; frame < HALF_FRAMES; ++frame) {
        uint32_t rd = mix_rd;
        uint32_t depth = mix_wr - rd;
        __asm__ volatile("dmb ish" ::: "memory");
        int32_t left = 0, right = 0;
        if (depth >= 2 && depth <= RING_SLOTS) {
            left = mix_buf[rd & RING_MASK];
            right = mix_buf[(rd + 1u) & RING_MASK];
            if (source_fraction && depth >= 4) {
                uint32_t frac = source_fraction >> 16;
                int32_t next_l = mix_buf[(rd + 2u) & RING_MASK];
                int32_t next_r = mix_buf[(rd + 3u) & RING_MASK];
                left += (int32_t)(((int64_t)(next_l - left) * frac) >> 16);
                right += (int32_t)(((int64_t)(next_r - right) * frac) >> 16);
            }
            uint64_t position = source_fraction + source_step;
            uint32_t advance = (uint32_t)(position >> 32);
            source_fraction = (uint32_t)position;
            if (advance > depth / 2u) {
                advance = depth / 2u;
                source_fraction = 0;
                if (count_underrun) audio_underruns++;
            }
            __asm__ volatile("dmb ish" ::: "memory");
            mix_rd = rd + 2u * advance;
        } else {
            source_fraction = 0;
            if (depth > RING_SLOTS) mix_rd = mix_wr; /* recover ring overrun */
            if (count_underrun) audio_underruns++;
        }
        /* TX FIFO mode 1 takes sample LSBs, as in sun4i-i2s.c. A signed
         * 16-bit source becomes S24_LE in bits 23:8; no signed left shift. */
        out[2u * frame] = left * 256;
        out[2u * frame + 1u] = right * 256;
    }
    dcache_clean_range(AUDIO_BUF_ADDR + half * HALF_BYTES, HALF_BYTES);
}

static void service_dma(void)
{
    uint32_t stat = DMA_IRQ_STAT(0) & 7u;
    if (!(stat & DMA_PKG)) {
        if (stat) DMA_IRQ_STAT(0) = stat;
        audio_dbg_no_pkg++;
        return;
    }
    DMA_IRQ_STAT(0) = stat;
    barrier();
    if (!dma_running) return;

    unsigned completed = next_refill;
    /* Prefer the half the DMA has actually left. This also recovers
     * after a long IRQ-disabled operation coalesces package interrupts.
     * Give CUR_SRC a bounded opportunity to cross the descriptor edge. */
    uint32_t cur = DMA_CH_CUR_SRC(0);
    for (unsigned tries = 0; tries < 32u; ++tries) {
        if (cur < AUDIO_BUF_ADDR || cur >= AUDIO_BUF_ADDR + HALF_BYTES * 2u) break;
        unsigned active = (cur - AUDIO_BUF_ADDR) / HALF_BYTES;
        if (active != next_refill) { completed = next_refill; break; }
        barrier();
        cur = DMA_CH_CUR_SRC(0);
    }
    if (cur >= AUDIO_BUF_ADDR && cur < AUDIO_BUF_ADDR + HALF_BYTES * 2u) {
        unsigned inactive = 1u - (cur - AUDIO_BUF_ADDR) / HALF_BYTES;
        if (inactive != completed) audio_dbg_cursrc_disagree++;
        completed = inactive;
    } else if (cur == AUDIO_BUF_ADDR + HALF_BYTES * 2u) {
        /* End-of-B can be visible before the controller loads A's next
         * descriptor. It remains unambiguous after coalesced IRQs. */
        completed = 1;
    } else {
        /* An invalid DMA position cannot identify a safe writable half. */
        audio_dbg_cursrc_disagree++;
        audio_underruns++;
        return;
    }
    refill(completed, 1);
    if (completed) {
        audio_dbg_refill_b++;
        if (last_refill == 1) audio_dbg_alt_break_b++;
        b_filled = 1;
    } else {
        audio_dbg_refill_a++;
        if (last_refill == 0) audio_dbg_alt_break_a++;
        a_filled = 1;
    }
    last_refill = completed;
    next_refill = completed ^ 1u;
    audio_transitions++;
}

void audio_dma_isr(void)
{
    audio_dbg_isr_entries++;
    service_dma();
}

void audio_start(void)
{
    if (!codec_ready || dma_running) return;
    uint32_t saved = lock_irq();
    refill(0, 0);
    refill(1, 0);
    a_filled = b_filled = 1;
    next_refill = 0;
    last_refill = 2;
    DMA_CH_DESC(0) = DESC_A;
    DMA_IRQ_STAT(0) = 7;
    DMA_IRQ_EN(0) = (DMA_IRQ_EN(0) & ~7u) | DMA_PKG;
    irq_register(IRQ_DMA, audio_dma_isr);
    irq_enable(IRQ_DMA);
    A64_DAI(0x14) |= DAI_TX_FLUSH;
    if (wait_mask(0x01c22c14u, DAI_TX_FLUSH, 0, 10000) < 0) {
        unlock_irq(saved);
        pine64_audio_stop();
        uart_puts("[audio/pine64] start: FIFO flush timeout\n");
        return;
    }
    A64_DAI(0x28) = 0;
    barrier();
    dma_running = 1;
    DMA_CH_EN(0) = 1;
    A64_DAI(0x00) |= DAI_TX;
    A64_DAI(0x1c) |= DAI_TX_DRQ;
    analog_write(0x0a, 0xcc); /* DAC L/R -> HP, unmute both, no mixer loopback */
    unlock_irq(saved);
}

void audio_update(void)
{
    pine64_service();
    if (!dma_running) return;
    uint32_t saved = lock_irq();
    if (DMA_IRQ_STAT(0) & DMA_PKG) service_dma();
    unlock_irq(saved);
}

void audio_quickstart(uint32_t rate)
{
    audio_init();
    audio_set_rate(rate);
    audio_set_hp_vol(63);
    audio_set_master_volume(255);
    for (uint32_t i = 0; i < 2048u; ++i) mix_buf[i] = 0;
    __asm__ volatile("dmb ish" ::: "memory");
    mix_wr = 2048;
    audio_start();
}
