/*
 * YM3438 Live — Real FM + GB DMG PSG + N64 Controller
 *
 * Same as opn2_hw_live but with the SN76489 PSG replaced by a
 * Game Boy DMG sound emulator.  SN76489 VGM commands (0x50) are
 * translated to GB register writes on the fly:
 *
 *   SN76489 CH0  →  GB CH1 (square, sweep disabled)
 *   SN76489 CH1  →  GB CH2 (square)
 *   SN76489 CH2  →  GB CH3 (wave — square wave loaded into WaveRAM)
 *   SN76489 CH3  →  GB CH4 (noise)
 *
 * Frequency:  gb_freq = 2048 - (4194304 * N) / 3579545
 * Volume:     gb_vol  = 15 - SN76489_atten
 *
 * Build: make GAME=examples/opn2_hw_gb/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include "../opn2_hw_live/sortie.h"

/* GB DMG sound emulator (MAME, from libvgm) */
extern void *gb_jupiter_init(uint32_t clock, uint32_t sample_rate);
extern void  gb_jupiter_reset(void *chip);
extern void  gb_jupiter_write(void *chip, uint8_t reg, uint8_t val);
extern void  gb_jupiter_wave_write(void *chip, uint8_t offset, uint8_t val);
extern void  gb_jupiter_update(void *chip, uint32_t samples, int32_t *left, int32_t *right);

/* Audio ring buffer (shared with DMA ISR) */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;

#define GB_CLK      4194304
#define PSG_RATE    48000
#define TARGET_DEPTH 3800

/* GB register offsets */
#define NR10 0x00
#define NR11 0x01
#define NR12 0x02
#define NR13 0x03
#define NR14 0x04
#define NR21 0x06
#define NR22 0x07
#define NR23 0x08
#define NR24 0x09
#define NR30 0x0A
#define NR31 0x0B
#define NR32 0x0C
#define NR33 0x0D
#define NR34 0x0E
#define NR41 0x10
#define NR42 0x11
#define NR43 0x12
#define NR44 0x13
#define NR50 0x14
#define NR51 0x15
#define NR52 0x16

static void *gb_chip;

/* ================================================================== */
/* SN76489 → GB DMG translator                                         */
/* ================================================================== */

static uint8_t  sn_latch_ch;
static uint8_t  sn_latch_type;
static uint16_t sn_tone[4];
static uint8_t  sn_vol[4];
static uint8_t  sn_noise_ctrl;

/* SN76489 freq → GB 11-bit freq register.
 * SN76489:  Hz = 3579545 / (32 * N)
 * GB:       Hz = 131072 / (2048 - gb_freq)   [square]
 *           Hz = 65536 / (2048 - gb_freq)    [wave, one octave lower]
 *
 * gb_freq = 2048 - 131072 * 32 * N / 3579545
 *         = 2048 - (4194304 * N) / 3579545
 */
static uint16_t sn_to_gb_freq(uint16_t tone_n)
{
    if (tone_n == 0) return 2047;
    uint32_t sub = ((uint32_t)4194304 * tone_n) / 3579545;
    if (sub >= 2048) return 0;
    return (uint16_t)(2048 - sub);
}

/* Wave channel is one octave lower for same register value,
 * so we double the frequency: gb_freq_wave = 2048 - (2097152 * N) / 3579545 */
static uint16_t sn_to_gb_freq_wave(uint16_t tone_n)
{
    if (tone_n == 0) return 2047;
    uint32_t sub = ((uint32_t)2097152 * tone_n) / 3579545;
    if (sub >= 2048) return 0;
    return (uint16_t)(2048 - sub);
}

static void gb_set_ch1_freq(uint16_t tone_n)
{
    uint16_t f = sn_to_gb_freq(tone_n);
    gb_jupiter_write(gb_chip, NR13, (uint8_t)(f & 0xFF));
    gb_jupiter_write(gb_chip, NR14, 0x80 | (uint8_t)((f >> 8) & 0x07));  /* trigger + freq hi */
}

static void gb_set_ch1_vol(uint8_t atten)
{
    uint8_t vol = 15 - atten;
    gb_jupiter_write(gb_chip, NR12, (vol << 4) | 0x08);  /* vol, increase, period=0 */
    /* Re-trigger to apply new envelope */
    uint16_t f = sn_to_gb_freq(sn_tone[0]);
    gb_jupiter_write(gb_chip, NR14, 0x80 | (uint8_t)((f >> 8) & 0x07));
}

static void gb_set_ch2_freq(uint16_t tone_n)
{
    uint16_t f = sn_to_gb_freq(tone_n);
    gb_jupiter_write(gb_chip, NR23, (uint8_t)(f & 0xFF));
    gb_jupiter_write(gb_chip, NR24, 0x80 | (uint8_t)((f >> 8) & 0x07));
}

static void gb_set_ch2_vol(uint8_t atten)
{
    uint8_t vol = 15 - atten;
    gb_jupiter_write(gb_chip, NR22, (vol << 4) | 0x08);
    uint16_t f = sn_to_gb_freq(sn_tone[1]);
    gb_jupiter_write(gb_chip, NR24, 0x80 | (uint8_t)((f >> 8) & 0x07));
}

static void gb_set_ch3_freq(uint16_t tone_n)
{
    uint16_t f = sn_to_gb_freq_wave(tone_n);
    gb_jupiter_write(gb_chip, NR33, (uint8_t)(f & 0xFF));
    gb_jupiter_write(gb_chip, NR34, 0x80 | (uint8_t)((f >> 8) & 0x07));
}

static void gb_set_ch3_vol(uint8_t atten)
{
    /* Wave channel has 4 output levels: 00=mute, 01=100%, 10=50%, 11=25% */
    uint8_t level;
    if (atten >= 15)     level = 0x00;  /* mute */
    else if (atten >= 10) level = 0x60;  /* 25% */
    else if (atten >= 5)  level = 0x40;  /* 50% */
    else                  level = 0x20;  /* 100% */
    gb_jupiter_write(gb_chip, NR32, level);
}

static void gb_set_noise(uint8_t ctrl, uint8_t atten)
{
    uint8_t vol = 15 - atten;
    gb_jupiter_write(gb_chip, NR42, (vol << 4) | 0x08);

    /* SN76489 noise: bit2=white/periodic, bits1:0=rate
     * GB noise NR43: bits7:4=shift, bit3=width(0=15bit/1=7bit), bits2:0=divisor
     * Periodic→7-bit LFSR (more tonal), White→15-bit LFSR */
    uint8_t width = (ctrl & 0x04) ? 0x00 : 0x08;  /* SN periodic → GB 7-bit */
    uint8_t nr43;
    switch (ctrl & 0x03) {
    case 0: nr43 = 0x00 | width; break;  /* high freq */
    case 1: nr43 = 0x30 | width; break;  /* med freq */
    case 2: nr43 = 0x60 | width; break;  /* low freq */
    default: nr43 = 0x40 | width; break; /* rate 3 = use CH2 tone (approx) */
    }
    gb_jupiter_write(gb_chip, NR43, nr43);
    gb_jupiter_write(gb_chip, NR44, 0x80);  /* trigger */
}

static uint32_t dbg_psg_writes;
static uint32_t dbg_fm_writes;
static uint32_t dbg_psg_samples;
static int32_t  dbg_psg_peak;

static void sn76489_to_gb(uint8_t data)
{
    if (data & 0x80) {
        sn_latch_ch   = (data >> 5) & 0x03;
        sn_latch_type = (data >> 4) & 0x01;
        if (sn_latch_type) {
            sn_vol[sn_latch_ch] = data & 0x0F;
        } else {
            if (sn_latch_ch == 3)
                sn_noise_ctrl = data & 0x07;
            else
                sn_tone[sn_latch_ch] = (sn_tone[sn_latch_ch] & 0x3F0) | (data & 0x0F);
        }
    } else {
        if (sn_latch_type) {
            sn_vol[sn_latch_ch] = data & 0x0F;
        } else {
            if (sn_latch_ch == 3)
                sn_noise_ctrl = data & 0x07;
            else
                sn_tone[sn_latch_ch] = (sn_tone[sn_latch_ch] & 0x00F) | ((data & 0x3F) << 4);
        }
    }

    switch (sn_latch_ch) {
    case 0:
        if (sn_latch_type) gb_set_ch1_vol(sn_vol[0]);
        else               gb_set_ch1_freq(sn_tone[0]);
        break;
    case 1:
        if (sn_latch_type) gb_set_ch2_vol(sn_vol[1]);
        else               gb_set_ch2_freq(sn_tone[1]);
        break;
    case 2:
        if (sn_latch_type) gb_set_ch3_vol(sn_vol[2]);
        else               gb_set_ch3_freq(sn_tone[2]);
        break;
    case 3:
        gb_set_noise(sn_noise_ctrl, sn_vol[3]);
        break;
    }
}

/* ================================================================== */
/* VGM parser — direct hardware writes                                  */
/* ================================================================== */
static const uint8_t *vgm_data;
static uint32_t vgm_len;
static uint32_t vgm_pos;
static uint32_t vgm_data_offset;
static int vgm_done;
static int vgm_paused;

static inline uint32_t rd32le(const uint8_t *p) {
    return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

static void vgm_hw_init(const uint8_t *data, uint32_t len)
{
    vgm_data = data;
    vgm_len = len;
    vgm_data_offset = rd32le(data + 0x34) + 0x34;
    vgm_pos = vgm_data_offset;
    vgm_done = 0;
    vgm_paused = 0;
}

static void vgm_hw_restart(void)
{
    for (int ch = 0; ch < 3; ch++) {
        ym3438_hw_vgm_write(0, 0x28, ch);
        ym3438_hw_vgm_write(0, 0x28, ch | 0x04);
    }
    gb_jupiter_reset(gb_chip);
    /* Re-enable GB sound system */
    gb_jupiter_write(gb_chip, NR52, 0x80);
    gb_jupiter_write(gb_chip, NR50, 0x77);
    gb_jupiter_write(gb_chip, NR51, 0xFF);
    /* Reload square wave into WaveRAM */
    gb_jupiter_write(gb_chip, NR30, 0x00);
    for (int i = 0; i < 8; i++)  gb_jupiter_wave_write(gb_chip, i, 0xFF);
    for (int i = 8; i < 16; i++) gb_jupiter_wave_write(gb_chip, i, 0x00);
    gb_jupiter_write(gb_chip, NR30, 0x80);

    memset(sn_tone, 0, sizeof(sn_tone));
    memset(sn_vol, 0x0F, sizeof(sn_vol));
    sn_noise_ctrl = 0;
    vgm_pos = vgm_data_offset;
    vgm_done = 0;
}

static uint8_t channel_mute = 0;

static uint32_t vgm_hw_tick(void)
{
    if (vgm_paused) return 735;
    const uint8_t *d = vgm_data;

    while (vgm_pos < vgm_len) {
        uint8_t cmd = d[vgm_pos++];

        switch (cmd) {
        case 0x52: {
            uint8_t addr = d[vgm_pos++];
            uint8_t val  = d[vgm_pos++];
            if (addr == 0x28) {
                int ch = val & 0x07;
                if (ch >= 4) ch = (ch - 4) + 3;
                if (channel_mute & (1 << ch)) break;
            }
            ym3438_hw_vgm_write(0, addr, val);
            dbg_fm_writes++;
            break;
        }
        case 0x53: {
            uint8_t addr = d[vgm_pos++];
            uint8_t val  = d[vgm_pos++];
            if (addr == 0x28) {
                int ch = val & 0x07;
                if (ch >= 4) ch = (ch - 4) + 3;
                if (channel_mute & (1 << ch)) break;
            }
            ym3438_hw_vgm_write(1, addr, val);
            break;
        }
        case 0x61: {
            uint16_t n = d[vgm_pos] | (d[vgm_pos+1] << 8);
            vgm_pos += 2;
            return n;
        }
        case 0x62: return 735;
        case 0x63: return 882;
        case 0x66: {
            uint32_t loop_off = rd32le(vgm_data + 0x1C);
            if (loop_off) {
                vgm_pos = loop_off + 0x1C;
            } else {
                vgm_done = 1;
                return 0;
            }
            break;
        }
        default:
            if ((cmd & 0xF0) == 0x70) return (cmd & 0x0F) + 1;
            if ((cmd & 0xF0) == 0x80) {
                ym3438_hw_vgm_write(0, 0x2A, 0x80);
                return cmd & 0x0F;
            }
            if (cmd == 0x50) {
                sn76489_to_gb(d[vgm_pos]);
                dbg_psg_writes++;
                vgm_pos++; break;
            }
            if (cmd == 0x67) {
                vgm_pos++;
                vgm_pos++;
                uint32_t blen = rd32le(d + vgm_pos);
                vgm_pos += 4 + blen;
                break;
            }
            break;
        }
    }
    vgm_done = 1;
    return 0;
}

/* ================================================================== */
/* Main                                                                 */
/* ================================================================== */
int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();

    uart_puts("\n========================================\n");
    uart_puts("  YM3438 Live — Real FM + GB DMG PSG\n");
    uart_puts("========================================\n");
    uart_puts("  SN76489 commands translated to GB DMG\n");
    uart_puts("  CH0→Square1  CH1→Square2  CH2→Wave\n");
    uart_puts("  CH3→Noise    WaveRAM: square wave\n");
    uart_puts("  FM: real YM3438  PSG: software GB DMG\n");
    uart_puts("========================================\n\n");

    /* ---- GIC + V3s codec ---- */
    irq_init();
    audio_init_24bit();

    /* ---- GB DMG sound init ---- */
    gb_chip = gb_jupiter_init(GB_CLK, PSG_RATE);
    if (gb_chip) {
        gb_jupiter_reset(gb_chip);
        /* Enable sound system */
        gb_jupiter_write(gb_chip, NR52, 0x80);  /* master on */
        gb_jupiter_write(gb_chip, NR50, 0x77);  /* both outputs max */
        gb_jupiter_write(gb_chip, NR51, 0xFF);  /* all channels to both outputs */
        /* Disable sweep on CH1 */
        gb_jupiter_write(gb_chip, NR10, 0x00);
        /* Set 50% duty on CH1 and CH2 */
        gb_jupiter_write(gb_chip, NR11, 0x80);  /* 50% duty */
        gb_jupiter_write(gb_chip, NR21, 0x80);  /* 50% duty */
        /* Load square wave into WaveRAM for CH3 */
        gb_jupiter_write(gb_chip, NR30, 0x00);  /* DAC off to write WaveRAM */
        for (int i = 0; i < 8; i++)  gb_jupiter_wave_write(gb_chip, i, 0xFF);
        for (int i = 8; i < 16; i++) gb_jupiter_wave_write(gb_chip, i, 0x00);
        gb_jupiter_write(gb_chip, NR30, 0x80);  /* DAC on */
        uart_puts("[gb] DMG sound init OK (");
        uart_putdec(GB_CLK); uart_puts("Hz, ");
        uart_putdec(PSG_RATE); uart_puts("Hz)\n");
    } else {
        uart_puts("[gb] FAILED to create DMG chip!\n");
    }

    memset(sn_tone, 0, sizeof(sn_tone));
    memset(sn_vol, 0x0F, sizeof(sn_vol));
    sn_noise_ctrl = 0;

    /* ---- Pre-fill silence then start codec DMA ---- */
    for (uint32_t i = 0; i < TARGET_DEPTH; i++) {
        mix_buf[mix_wr & MIX_BUF_MASK] = 0;
        mix_wr++;
    }
    audio_start();
    irq_global_enable();

    /* ---- YM3438 hardware init ---- */
    ym3438_hw_set_clock(YM_CLK_CCU_MCLK);
    ym3438_hw_init();

    /* ---- VGM ---- */
    vgm_hw_init(vgm_sortie, vgm_sortie_len);
    uart_puts("[vgm] Comix Zone - Sortie loaded\n");

    /* ---- N64 controller ---- */
    input_init(INPUT_N64);

    uart_puts("[init] gb_chip=0x"); uart_puthex((uint32_t)gb_chip);
    uart_puts("\n[init] mix_wr="); uart_putdec(mix_wr);
    uart_puts(" mix_rd="); uart_putdec(mix_rd);
    uart_puts(" depth="); uart_putdec(mix_wr - mix_rd);
    uart_puts(" xrun="); uart_putdec(audio_underruns);
    uart_puts("\n[main] playing!\n\n");

    /* ---- Playback state ---- */
    uint32_t vgm_accum = 0;
    int tempo_pct = 100;
    uint32_t frame = 0;
    uint32_t stat_timer = timer_read();

    while (1) {
        uint32_t t0 = timer_read();

        /* ---- VGM playback ---- */
        if (!vgm_done) {
            uint32_t target = (735 * 100) / tempo_pct;
            while (vgm_accum < target) {
                uint32_t wait = vgm_hw_tick();
                if (vgm_done) break;
                vgm_accum += wait;
            }
            vgm_accum -= target;
        }

        /* ---- N64 input ---- */
        input_poll();

        if (input_pressed() & BTN_A) { vgm_hw_restart(); uart_puts("[live] restart\n"); }
        if (input_pressed() & BTN_START) {
            vgm_paused = !vgm_paused;
            uart_puts(vgm_paused ? "[live] paused\n" : "[live] resumed\n");
            if (vgm_paused) {
                for (int ch = 0; ch < 3; ch++) {
                    ym3438_hw_vgm_write(0, 0x28, ch);
                    ym3438_hw_vgm_write(0, 0x28, ch | 0x04);
                }
            }
        }
        if (input_pressed() & BTN_B) { tempo_pct = 100; channel_mute = 0; uart_puts("[live] defaults restored\n"); }
        if (input_pressed() & BTN_UP)   { tempo_pct += 10; if (tempo_pct > 200) tempo_pct = 200; uart_puts("[live] tempo="); uart_putdec(tempo_pct); uart_puts("%\n"); }
        if (input_pressed() & BTN_DOWN) { tempo_pct -= 10; if (tempo_pct < 50)  tempo_pct = 50;  uart_puts("[live] tempo="); uart_putdec(tempo_pct); uart_puts("%\n"); }

        if (input_pressed() & BTN_C_UP)    { channel_mute ^= (1<<0); uart_puts("[live] ch1 "); uart_puts((channel_mute&1)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_C_DOWN)   { channel_mute ^= (1<<1); uart_puts("[live] ch2 "); uart_puts((channel_mute&2)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_C_LEFT)   { channel_mute ^= (1<<2); uart_puts("[live] ch3 "); uart_puts((channel_mute&4)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_C_RIGHT)  { channel_mute ^= (1<<3); uart_puts("[live] ch4 "); uart_puts((channel_mute&8)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_L)        { channel_mute ^= (1<<4); uart_puts("[live] ch5 "); uart_puts((channel_mute&16)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_R)        { channel_mute ^= (1<<5); uart_puts("[live] ch6 "); uart_puts((channel_mute&32)?"MUTE\n":"ON\n"); }

        /* ---- GB DMG render: fill V3s codec buffer ---- */
        while ((mix_wr - mix_rd) < TARGET_DEPTH) {
            int32_t gb_l = 0, gb_r = 0;
            if (gb_chip)
                gb_jupiter_update(gb_chip, 1, &gb_l, &gb_r);

            int32_t abs_l = gb_l < 0 ? -gb_l : gb_l;
            int32_t abs_r = gb_r < 0 ? -gb_r : gb_r;
            if (abs_l > dbg_psg_peak) dbg_psg_peak = abs_l;
            if (abs_r > dbg_psg_peak) dbg_psg_peak = abs_r;

            /* Scale GB output to match real FM volume. */
            int32_t out_l = gb_l * 2;
            int32_t out_r = gb_r * 2;
            if (out_l > 32767) out_l = 32767;
            if (out_l < -32768) out_l = -32768;
            if (out_r > 32767) out_r = 32767;
            if (out_r < -32768) out_r = -32768;

            mix_buf[mix_wr & MIX_BUF_MASK] = (int16_t)out_l; mix_wr++;
            mix_buf[mix_wr & MIX_BUF_MASK] = (int16_t)out_r; mix_wr++;
            dbg_psg_samples++;
        }

        /* ---- Pace to ~60fps ---- */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667);

        frame++;

        /* Stats every 5 seconds */
        uint32_t elapsed = ticks_to_ms(timer_elapsed(stat_timer, timer_read()));
        if (elapsed >= 5000) {
            uint32_t depth = mix_wr - mix_rd;
            uart_puts("[dbg] fm="); uart_putdec(dbg_fm_writes);
            uart_puts(" psg="); uart_putdec(dbg_psg_writes);
            uart_puts(" peak="); uart_putdec(dbg_psg_peak);
            uart_puts(" buf="); uart_putdec(depth);
            uart_puts(" xrun="); uart_putdec(audio_underruns);
            uart_puts("\n");
            dbg_fm_writes = 0;
            dbg_psg_writes = 0;
            dbg_psg_samples = 0;
            dbg_psg_peak = 0;
            stat_timer = timer_read();
        }
    }
    return 0;
}
