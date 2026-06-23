/*
 * YM3438 Live — Real FM + NES APU PSG + N64 Controller
 *
 * Same as opn2_hw_live but with the SN76489 PSG replaced by a
 * NES 2A03 APU emulator.  SN76489 VGM commands (0x50) are translated
 * to NES APU register writes on the fly:
 *
 *   SN76489 CH0  →  NES Pulse 1      (period = N-1)
 *   SN76489 CH1  →  NES Pulse 2      (period = N-1)
 *   SN76489 CH2  →  NES Triangle     (period = N/2-1)
 *   SN76489 CH3  →  NES Noise
 *   Volume:  NES_vol = 15 - SN76489_atten
 *
 * The SN76489 clock (3.579545 MHz) is exactly 2× the NES CPU clock
 * (1.789773 MHz), so period conversion is trivial integer math.
 *
 * Build: make GAME=examples/opn2_hw_nes/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include "../opn2_hw_live/sortie.h"

/* NES APU emulator (NSFPlay, from libvgm) */
extern void *NES_APU_np_Create(uint32_t clock, uint32_t rate);
extern void  NES_APU_np_Reset(void *chip);
extern uint32_t NES_APU_np_Render(void *chip, int32_t b[2]);
extern int   NES_APU_np_Write(void *chip, uint16_t adr, uint8_t val);

extern void *NES_DMC_np_Create(uint32_t clock, uint32_t rate);
extern void  NES_DMC_np_Reset(void *chip);
extern void  NES_DMC_np_SetAPU(void *chip, void *apu);
extern uint32_t NES_DMC_np_Render(void *chip, int32_t b[2]);
extern int   NES_DMC_np_Write(void *chip, uint16_t adr, uint8_t val);

/* Audio ring buffer (shared with DMA ISR) */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;

#define NES_CPU_CLK 1789773
#define PSG_RATE    48000
#define TARGET_DEPTH 3800

static void *nes_apu;
static void *nes_dmc;
static volatile uint32_t dbg_psg_peak    = 0;
static volatile uint32_t dbg_psg_samples = 0;

#include "hstimer.h"

/* ISR-driven NES APU producer. Same self-pacing pattern jupiter_moon
 * uses — main-loop production is vulnerable to icache pressure in
 * larger binaries (menu) because NES_APU+NES_DMC are heavier per-
 * sample than other PSGs. Moving the render to a fixed-cadence
 * hstimer keeps production deterministic. */
#define AUDIO_TICK_SCANLINES   256u                  /* ~66 Hz */
#define AUDIO_TICK_TARGET      ((MIX_BUF_SIZE * 3) / 4)
#define AUDIO_TICK_MAX         800u
static volatile int audio_isr_armed = 0;

static void nes_render_frames(uint32_t frames)
{
    for (uint32_t i = 0; i < frames; i++) {
        if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2)) return;
        int32_t apu_out[2] = {0, 0};
        int32_t dmc_out[2] = {0, 0};
        if (nes_apu) NES_APU_np_Render(nes_apu, apu_out);
        if (nes_dmc) NES_DMC_np_Render(nes_dmc, dmc_out);
        int32_t left  = apu_out[0] + dmc_out[0];
        int32_t right = apu_out[1] + dmc_out[1];
        int32_t abs_l = left  < 0 ? -left  : left;
        int32_t abs_r = right < 0 ? -right : right;
        if (abs_l > (int32_t)dbg_psg_peak) dbg_psg_peak = (uint32_t)abs_l;
        if (abs_r > (int32_t)dbg_psg_peak) dbg_psg_peak = (uint32_t)abs_r;
        int32_t out_l = left  * 4;
        int32_t out_r = right * 4;
        if (out_l >  32767) out_l =  32767;
        if (out_l < -32768) out_l = -32768;
        if (out_r >  32767) out_r =  32767;
        if (out_r < -32768) out_r = -32768;
        mix_buf[mix_wr++ & MIX_BUF_MASK] = (int16_t)out_l;
        mix_buf[mix_wr++ & MIX_BUF_MASK] = (int16_t)out_r;
        dbg_psg_samples++;
    }
}

static void nes_audio_tick(void)
{
    if (!audio_isr_armed) return;
    uint32_t depth = mix_wr - mix_rd;
    if (depth >= AUDIO_TICK_TARGET) return;
    uint32_t deficit_frames = (AUDIO_TICK_TARGET - depth) / 2;
    if (deficit_frames > AUDIO_TICK_MAX) deficit_frames = AUDIO_TICK_MAX;
    if (deficit_frames == 0) return;
    nes_render_frames(deficit_frames);
}

/* ================================================================== */
/* SN76489 → NES APU translator                                        */
/* ================================================================== */

/* SN76489 latch state */
static uint8_t  sn_latch_ch;    /* 0-3 */
static uint8_t  sn_latch_type;  /* 0=tone, 1=volume */
static uint16_t sn_tone[4];     /* 10-bit tone registers */
static uint8_t  sn_vol[4];      /* 4-bit attenuation (0=loud, 15=off) */
static uint8_t  sn_noise_ctrl;  /* noise control register */

static void nes_set_pulse_freq(int ch, uint16_t tone_n)
{
    /* NES pulse period = SN76489_N - 1 */
    uint16_t period = (tone_n > 0) ? tone_n - 1 : 0;
    uint16_t base = (ch == 0) ? 0x4002 : 0x4006;
    NES_APU_np_Write(nes_apu, base, (uint8_t)(period & 0xFF));
    /* High 3 bits + length counter load (0xF8 = max length) */
    NES_APU_np_Write(nes_apu, base + 1, 0xF8 | (uint8_t)((period >> 8) & 0x07));
}

static void nes_set_pulse_vol(int ch, uint8_t atten)
{
    uint8_t vol = 15 - atten;
    uint16_t base = (ch == 0) ? 0x4000 : 0x4004;
    /* Duty 50% (10), loop envelope (1), constant volume (1), volume */
    NES_APU_np_Write(nes_apu, base, 0xB0 | (vol & 0x0F));
}

static void nes_set_tri_freq(uint16_t tone_n)
{
    /* NES triangle period = SN76489_N / 2 - 1 */
    uint16_t period = (tone_n > 1) ? (tone_n / 2) - 1 : 0;
    NES_DMC_np_Write(nes_dmc, 0x400A, (uint8_t)(period & 0xFF));
    NES_DMC_np_Write(nes_dmc, 0x400B, 0xF8 | (uint8_t)((period >> 8) & 0x07));
}

static void nes_set_tri_vol(uint8_t atten)
{
    /* Triangle has no volume control — gate on/off via linear counter */
    if (atten >= 15)
        NES_DMC_np_Write(nes_dmc, 0x4008, 0x00);  /* silence */
    else
        NES_DMC_np_Write(nes_dmc, 0x4008, 0xFF);   /* max linear counter, hold */
}

static void nes_set_noise(uint8_t ctrl, uint8_t atten)
{
    uint8_t vol = 15 - atten;
    /* Volume: loop envelope, constant volume */
    NES_DMC_np_Write(nes_dmc, 0x400C, 0x30 | (vol & 0x0F));

    /* SN76489 noise: bit2=white/periodic, bits1:0=rate
     * NES noise: bit7=mode(short/long), bits3:0=period index
     * Approximate mapping of SN76489 rates to NES period indices */
    static const uint8_t rate_map[4] = { 4, 8, 12, 14 };
    uint8_t mode = (ctrl & 0x04) ? 0x00 : 0x80;  /* SN periodic→NES short loop */
    uint8_t rate = rate_map[ctrl & 0x03];
    NES_DMC_np_Write(nes_dmc, 0x400E, mode | rate);
    NES_DMC_np_Write(nes_dmc, 0x400F, 0xF8);  /* length counter load */
}

static uint32_t dbg_psg_writes;
static uint32_t dbg_fm_writes;
/* dbg_psg_samples / dbg_psg_peak moved earlier (used by nes_render_frames) */

static void sn76489_to_nes(uint8_t data)
{
    if (data & 0x80) {
        /* LATCH/DATA byte: 1CCTDDDD */
        sn_latch_ch   = (data >> 5) & 0x03;
        sn_latch_type = (data >> 4) & 0x01;
        if (sn_latch_type) {
            /* Volume */
            sn_vol[sn_latch_ch] = data & 0x0F;
        } else {
            if (sn_latch_ch == 3) {
                /* Noise control */
                sn_noise_ctrl = data & 0x07;
            } else {
                /* Tone: low 4 bits */
                sn_tone[sn_latch_ch] = (sn_tone[sn_latch_ch] & 0x3F0) | (data & 0x0F);
            }
        }
    } else {
        /* DATA byte: 0_DDDDDD — continues latched register */
        if (sn_latch_type) {
            sn_vol[sn_latch_ch] = data & 0x0F;
        } else {
            if (sn_latch_ch == 3) {
                sn_noise_ctrl = data & 0x07;
            } else {
                /* Tone: high 6 bits */
                sn_tone[sn_latch_ch] = (sn_tone[sn_latch_ch] & 0x00F) | ((data & 0x3F) << 4);
            }
        }
    }

    /* Now push the updated state to the NES APU */
    switch (sn_latch_ch) {
    case 0:
        if (sn_latch_type) nes_set_pulse_vol(0, sn_vol[0]);
        else               nes_set_pulse_freq(0, sn_tone[0]);
        break;
    case 1:
        if (sn_latch_type) nes_set_pulse_vol(1, sn_vol[1]);
        else               nes_set_pulse_freq(1, sn_tone[1]);
        break;
    case 2:
        if (sn_latch_type) nes_set_tri_vol(sn_vol[2]);
        else               nes_set_tri_freq(sn_tone[2]);
        break;
    case 3:
        nes_set_noise(sn_noise_ctrl, sn_vol[3]);
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
    /* Reset NES APU state */
    NES_APU_np_Reset(nes_apu);
    NES_DMC_np_Reset(nes_dmc);
    /* Re-enable all channels */
    NES_APU_np_Write(nes_apu, 0x4015, 0x0F);
    NES_DMC_np_Write(nes_dmc, 0x4015, 0x0F);
    memset(sn_tone, 0, sizeof(sn_tone));
    memset(sn_vol, 0x0F, sizeof(sn_vol));  /* all silent */
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
                sn76489_to_nes(d[vgm_pos]);
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
    uart_puts("  YM3438 Live — Real FM + NES APU PSG\n");
    uart_puts("========================================\n");
    uart_puts("  SN76489 commands translated to NES 2A03\n");
    uart_puts("  CH0→Pulse1  CH1→Pulse2  CH2→Triangle\n");
    uart_puts("  CH3→Noise   Volume: 15-atten\n");
    uart_puts("  FM: real YM3438  PSG: software NES APU\n");
    uart_puts("========================================\n\n");

    /* ---- GIC + V3s codec ---- */
    irq_init();
    audio_init_24bit();

    /* ---- NES APU init ---- */
    nes_apu = NES_APU_np_Create(NES_CPU_CLK, PSG_RATE);
    nes_dmc = NES_DMC_np_Create(NES_CPU_CLK, PSG_RATE);
    if (nes_apu && nes_dmc) {
        NES_APU_np_Reset(nes_apu);
        NES_DMC_np_Reset(nes_dmc);
        NES_DMC_np_SetAPU(nes_dmc, nes_apu);
        /* Enable pulse 1+2, triangle, noise */
        NES_APU_np_Write(nes_apu, 0x4015, 0x0F);
        NES_DMC_np_Write(nes_dmc, 0x4015, 0x0F);
        uart_puts("[nes] 2A03 APU init OK (");
        uart_putdec(NES_CPU_CLK); uart_puts("Hz, ");
        uart_putdec(PSG_RATE); uart_puts("Hz)\n");
    } else {
        uart_puts("[nes] FAILED to create APU/DMC!\n");
    }

    /* Init SN76489 translator state */
    memset(sn_tone, 0, sizeof(sn_tone));
    memset(sn_vol, 0x0F, sizeof(sn_vol));
    sn_noise_ctrl = 0;

    /* ---- Pre-fill silence then start codec DMA ---- */
    for (uint32_t i = 0; i < TARGET_DEPTH; i++) {
        mix_buf[mix_wr & MIX_BUF_MASK] = 0;
        mix_wr++;
    }
    audio_start();

    /* Arm the ISR-driven NES APU producer (see nes_audio_tick comments
     * for rationale — moves render off the main loop so menu-binary
     * icache pressure can't cause dropouts). */
    hstimer_init();
    audio_isr_armed = 1;
    hstimer_set_repeating(0, AUDIO_TICK_SCANLINES, nes_audio_tick);

    irq_global_enable();

    /* ---- YM3438 hardware init ---- */
    ym3438_hw_set_clock(YM_CLK_CCU_MCLK);
    ym3438_hw_init();

    /* ---- VGM ---- */
    vgm_hw_init(vgm_sortie, vgm_sortie_len);
    uart_puts("[vgm] Comix Zone - Sortie loaded\n");

    /* ---- N64 controller ---- */
    input_init(INPUT_N64);

    uart_puts("[init] apu=0x"); uart_puthex((uint32_t)nes_apu);
    uart_puts(" dmc=0x"); uart_puthex((uint32_t)nes_dmc);
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

        if (input_pressed() & BTN_A) {
            vgm_hw_restart();
            uart_puts("[live] restart\n");
        }
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
        if (input_pressed() & BTN_B) {
            tempo_pct = 100;
            channel_mute = 0;
            uart_puts("[live] defaults restored\n");
        }

        if (input_pressed() & BTN_UP) {
            tempo_pct += 10;
            if (tempo_pct > 200) tempo_pct = 200;
            uart_puts("[live] tempo="); uart_putdec(tempo_pct); uart_puts("%\n");
        }
        if (input_pressed() & BTN_DOWN) {
            tempo_pct -= 10;
            if (tempo_pct < 50) tempo_pct = 50;
            uart_puts("[live] tempo="); uart_putdec(tempo_pct); uart_puts("%\n");
        }

        if (input_pressed() & BTN_C_UP)    { channel_mute ^= (1<<0); uart_puts("[live] ch1 "); uart_puts((channel_mute&1)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_C_DOWN)   { channel_mute ^= (1<<1); uart_puts("[live] ch2 "); uart_puts((channel_mute&2)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_C_LEFT)   { channel_mute ^= (1<<2); uart_puts("[live] ch3 "); uart_puts((channel_mute&4)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_C_RIGHT)  { channel_mute ^= (1<<3); uart_puts("[live] ch4 "); uart_puts((channel_mute&8)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_L)        { channel_mute ^= (1<<4); uart_puts("[live] ch5 "); uart_puts((channel_mute&16)?"MUTE\n":"ON\n"); }
        if (input_pressed() & BTN_R)        { channel_mute ^= (1<<5); uart_puts("[live] ch6 "); uart_puts((channel_mute&32)?"MUTE\n":"ON\n"); }

        /* ---- NES APU render runs in hstimer ISR ---- */

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
