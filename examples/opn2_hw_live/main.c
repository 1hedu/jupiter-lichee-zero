/*
 * YM3438 Live — Real FM + Software PSG + N64 Controller
 *
 * Hybrid audio: YM3438 hardware FM + SN76489 software PSG through V3s codec.
 * Playing Comix Zone "Sortie" — a track that uses all 10 Genesis channels.
 *
 *   Start:              Pause / resume
 *   A:                  Restart song
 *   B:                  Reset defaults
 *   D-pad Up/Down:      Tempo +/- 10%
 *   C-buttons:          Toggle FM channels 1-4
 *   L/R shoulder:       Toggle FM channels 5/6
 *
 * Overlay 2: CCU MCLK 8MHz on PE1, N64 on PE20.
 *
 * Build: make GAME=examples/opn2_hw_live/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include "sortie.h"

/* SN76489 emulator (Maxim's, from libvgm) */
extern void *sn76489_jupiter_init(uint32_t clock, uint32_t rate);
extern void  sn76489_jupiter_reset(void *chip);
extern void  sn76489_jupiter_write(void *chip, uint8_t data);
extern void  sn76489_jupiter_update(void *chip, uint32_t len, int32_t *left, int32_t *right);
extern void  sn76489_jupiter_unmute(void *chip);
extern void  sn76489_jupiter_debug(void *chip);

/* Audio ring buffer (shared with DMA ISR) */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;

static void *psg_chip;
#define PSG_RATE 48000
#define TARGET_DEPTH 3800

/* ================================================================== */
/* VGM parser — direct hardware writes                                  */
/* ================================================================== */
static const uint8_t *vgm_data;
static uint32_t vgm_len;
static uint32_t vgm_pos;
static uint32_t vgm_data_offset;
static int vgm_done;
static int vgm_paused;

/* Debug counters (global so vgm_hw_tick can access) */
static uint32_t dbg_psg_writes;
static uint32_t dbg_fm_writes;
static uint32_t dbg_psg_samples;
static int32_t dbg_psg_peak;

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
    /* Silence all channels: key off */
    for (int ch = 0; ch < 3; ch++) {
        ym3438_hw_vgm_write(0, 0x28, ch);        /* key off ch 1-3 */
        ym3438_hw_vgm_write(0, 0x28, ch | 0x04); /* key off ch 4-6 */
    }
    vgm_pos = vgm_data_offset;
    vgm_done = 0;
}

/* Channel mute mask — bit N = channel N+1 muted */
static uint8_t channel_mute = 0;

/* Pitch offset in F-Number units (added to all F-Numbers) */
static int16_t pitch_offset = 0;

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
                if (psg_chip) {
                    sn76489_jupiter_write(psg_chip, d[vgm_pos]);
                    dbg_psg_writes++;
                }
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
    uart_puts("  YM3438 Live — Real FM + N64 Control\n");
    uart_puts("========================================\n");
    uart_puts("  D-pad U/D: tempo    L/R: pitch\n");
    uart_puts("  C-buttons: mute ch1-4  L/R: ch5/6\n");
    uart_puts("  A: restart  B: reset  Start: pause\n");
    uart_puts("  FM: real YM3438  PSG: software SN76489\n");
    uart_puts("========================================\n\n");

    /* ---- GIC + V3s codec for PSG output ---- */
    irq_init();
    audio_init_24bit();

    /* ---- SN76489 software PSG ---- */
    psg_chip = sn76489_jupiter_init(3579545, PSG_RATE);
    if (psg_chip) {
        sn76489_jupiter_reset(psg_chip);
        sn76489_jupiter_unmute(psg_chip);
        uart_puts("[psg] SN76489 emulator init + unmuted (3.58MHz, 44.1kHz)\n");
    }

    /* ---- Pre-fill PSG silence then start codec DMA ---- */
    for (uint32_t i = 0; i < TARGET_DEPTH; i++) {
        mix_buf[mix_wr & MIX_BUF_MASK] = 0;
        mix_wr++;
    }
    audio_start();
    irq_global_enable();

    /* ---- YM3438 hardware init (after video_init not needed here) ---- */
    ym3438_hw_set_clock(YM_CLK_CCU_MCLK);
    ym3438_hw_init();

    /* ---- VGM ---- */
    vgm_hw_init(vgm_sortie, vgm_sortie_len);
    uart_puts("[vgm] Comix Zone - Sortie loaded\n");

    /* ---- N64 controller ---- */
    input_init(INPUT_N64);

    uart_puts("[init] psg_chip=0x"); uart_puthex((uint32_t)psg_chip);
    uart_puts(psg_chip ? " OK" : " FAILED (NULL!)");
    uart_puts("\n[init] mix_wr="); uart_putdec(mix_wr);
    uart_puts(" mix_rd="); uart_putdec(mix_rd);
    uart_puts(" depth="); uart_putdec(mix_wr - mix_rd);
    uart_puts(" xrun="); uart_putdec(audio_underruns);
    uart_puts("\n[init] audio rate="); uart_putdec(PSG_RATE);
    uart_puts("Hz, TARGET_DEPTH="); uart_putdec(TARGET_DEPTH);
    uart_puts("\n[main] playing!\n\n");

    /* ---- Playback state ---- */
    uint32_t vgm_accum = 0;
    int tempo_pct = 100;  /* 50-200% */
    uint32_t frame = 0;
    uint32_t stat_timer = timer_read();

    /* Debug counters are global (see top of file) */

    while (1) {
        uint32_t t0 = timer_read();

        /* ---- VGM playback: process ~1 frame of commands ---- */
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
        input_state_t pad = input_poll();

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
            pitch_offset = 0;
            channel_mute = 0;
            uart_puts("[live] defaults restored\n");
        }

        /* Tempo */
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

        /* Channel mute toggles */
        if (input_pressed() & BTN_C_UP) {
            channel_mute ^= (1 << 0);
            uart_puts("[live] ch1 "); uart_puts((channel_mute & 1) ? "MUTE\n" : "ON\n");
        }
        if (input_pressed() & BTN_C_DOWN) {
            channel_mute ^= (1 << 1);
            uart_puts("[live] ch2 "); uart_puts((channel_mute & 2) ? "MUTE\n" : "ON\n");
        }
        if (input_pressed() & BTN_C_LEFT) {
            channel_mute ^= (1 << 2);
            uart_puts("[live] ch3 "); uart_puts((channel_mute & 4) ? "MUTE\n" : "ON\n");
        }
        if (input_pressed() & BTN_C_RIGHT) {
            channel_mute ^= (1 << 3);
            uart_puts("[live] ch4 "); uart_puts((channel_mute & 8) ? "MUTE\n" : "ON\n");
        }
        if (input_pressed() & BTN_L) {
            channel_mute ^= (1 << 4);
            uart_puts("[live] ch5 "); uart_puts((channel_mute & 16) ? "MUTE\n" : "ON\n");
        }
        if (input_pressed() & BTN_R) {
            channel_mute ^= (1 << 5);
            uart_puts("[live] ch6 "); uart_puts((channel_mute & 32) ? "MUTE\n" : "ON\n");
        }

        /* ---- PSG render: fill V3s codec buffer ---- */
        while ((mix_wr - mix_rd) < TARGET_DEPTH) {
            int32_t psg_l = 0, psg_r = 0;
            if (psg_chip)
                sn76489_jupiter_update(psg_chip, 1, &psg_l, &psg_r);
            int32_t abs_l = psg_l < 0 ? -psg_l : psg_l;
            int32_t abs_r = psg_r < 0 ? -psg_r : psg_r;
            if (abs_l > dbg_psg_peak) dbg_psg_peak = abs_l;
            if (abs_r > dbg_psg_peak) dbg_psg_peak = abs_r;
            /* Boost PSG — real FM is loud analog, PSG is quiet digital.
             * The /6 ratio is for all-software mixing. For hybrid
             * (real FM chip + software PSG), PSG needs amplification. */
            int16_t out_l = (int16_t)(psg_l * 32);
            int16_t out_r = (int16_t)(psg_r * 32);
            mix_buf[mix_wr & MIX_BUF_MASK] = out_l; mix_wr++;
            mix_buf[mix_wr & MIX_BUF_MASK] = out_r; mix_wr++;
            dbg_psg_samples++;
        }
        /* ISR handles DMA refill — no audio_update() needed.
         * Calling both causes double-consumption of mix_buf. */

        /* ---- Pace to ~60fps ---- */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667);

        frame++;

        /* Stats every 5 seconds (keep short — UART blocks drain audio buffer) */
        uint32_t elapsed = ticks_to_ms(timer_elapsed(stat_timer, timer_read()));
        if (elapsed >= 5000) {
            uint32_t depth = mix_wr - mix_rd;
            uart_puts("[dbg] fm="); uart_putdec(dbg_fm_writes);
            uart_puts(" psg="); uart_putdec(dbg_psg_writes);
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
