/*
 * YM3438 + 8 MHz crystal — V3s touches no clock at all.
 *
 * Same hybrid playback as examples/opn2_hw_live (real YM3438 FM + software
 * SN76489 PSG through the V3s codec, playing Comix Zone "Sortie"), but
 * with the YM3438 phi-M pin clocked from an external crystal-oscillator
 * module instead of CCU CSI_MCLK on PE1. The V3s does NO clock setup —
 * no CCU register writes, no I2C, no PE1 mux. PE1 stays free for other
 * peripherals (e.g. Genesis SELECT in the controllers overlay).
 *
 * Hardware:
 *   * 8 MHz TTL crystal-oscillator module (DIP-8 or DIP-14, 5 V).
 *     Common parts: ECS-2200B-080, SG-8002JA, generic AliExpress 8 MHz.
 *   * 100 nF decoupling cap between osc VCC and GND pins.
 *   * Wire osc OUTPUT directly to YM3438 phi-M (pin 24).
 *   * YM3438 data + control bus stays on PB0-PB7 / PG0-PG4 (same as the
 *     CCU-clocked overlay).
 *
 * Wiring:
 *   +5V ──┬───────── osc VCC                YM3438:
 *       100nF                                 PB0-PB7 ── D0-D7
 *   GND ──┴───────── osc GND                  PG0-PG4 ── A0,A1,/WR,/CS,/IC
 *                                             pin 24 (phi-M) ─── osc OUT
 *
 * Controls (N64 pad):
 *   Start              Pause / resume
 *   A                  Restart song
 *   B                  Reset defaults
 *   D-pad Up/Down      Tempo +/- 10 %
 *   C-buttons          Toggle FM channels 1-4
 *   L/R shoulder       Toggle FM channels 5/6
 *
 * Build: make GAME=examples/opn2_hw_xtal/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include "sortie.h"

/* SN76489 emulator (libvgm) */
extern void *sn76489_jupiter_init(uint32_t clock, uint32_t rate);
extern void  sn76489_jupiter_reset(void *chip);
extern void  sn76489_jupiter_write(void *chip, uint8_t data);
extern void  sn76489_jupiter_update(void *chip, uint32_t len, int32_t *left, int32_t *right);
extern void  sn76489_jupiter_unmute(void *chip);

/* Audio ring buffer (shared with DMA ISR) */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;

static void *psg_chip;
#define PSG_RATE     48000
#define TARGET_DEPTH 3800

/* ================================================================== */
/* VGM parser — direct hardware writes                                  */
/* ================================================================== */
static const uint8_t *vgm_data;
static uint32_t vgm_len;
static uint32_t vgm_pos;
static uint32_t vgm_data_offset;
static int      vgm_done;
static int      vgm_paused;
static uint8_t  channel_mute;

static inline uint32_t rd32le(const uint8_t *p) {
    return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

static void vgm_init(const uint8_t *data, uint32_t len)
{
    vgm_data        = data;
    vgm_len         = len;
    vgm_data_offset = rd32le(data + 0x34) + 0x34;
    vgm_pos         = vgm_data_offset;
    vgm_done        = 0;
    vgm_paused      = 0;
}

static void vgm_restart(void)
{
    /* Key-off everything before reseeking. */
    for (int ch = 0; ch < 3; ch++) {
        ym3438_hw_vgm_write(0, 0x28, ch);
        ym3438_hw_vgm_write(0, 0x28, ch | 0x04);
    }
    vgm_pos  = vgm_data_offset;
    vgm_done = 0;
}

static uint32_t vgm_tick(void)
{
    if (vgm_paused) return 735;
    const uint8_t *d = vgm_data;
    while (vgm_pos < vgm_len) {
        uint8_t cmd = d[vgm_pos++];
        switch (cmd) {
        case 0x52: {  /* YM2612 port 0 write */
            uint8_t addr = d[vgm_pos++];
            uint8_t val  = d[vgm_pos++];
            if (addr == 0x28) {
                int ch = val & 0x07;
                if (ch >= 4) ch = (ch - 4) + 3;
                if (channel_mute & (1 << ch)) break;
            }
            ym3438_hw_vgm_write(0, addr, val);
            break;
        }
        case 0x53: {  /* YM2612 port 1 write */
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
        case 0x61: {  /* explicit wait */
            uint16_t n = d[vgm_pos] | (d[vgm_pos+1] << 8);
            vgm_pos += 2;
            return n;
        }
        case 0x62: return 735;       /* 60 Hz tick */
        case 0x63: return 882;       /* 50 Hz tick */
        case 0x66: {                  /* end / loop */
            uint32_t loop_off = rd32le(vgm_data + 0x1C);
            if (loop_off) { vgm_pos = loop_off + 0x1C; }
            else          { vgm_done = 1; return 0; }
            break;
        }
        default:
            if ((cmd & 0xF0) == 0x70) return (cmd & 0x0F) + 1;  /* short wait */
            if ((cmd & 0xF0) == 0x80) {
                ym3438_hw_vgm_write(0, 0x2A, 0x80);
                return cmd & 0x0F;
            }
            if (cmd == 0x50) {
                if (psg_chip) sn76489_jupiter_write(psg_chip, d[vgm_pos]);
                vgm_pos++; break;
            }
            if (cmd == 0x67) {
                vgm_pos += 2;
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
    uart_puts("  YM3438 + 8 MHz crystal — V3s clock-free\n");
    uart_puts("========================================\n");
    uart_puts("  Start: pause   A: restart  B: reset\n");
    uart_puts("  D-pad U/D: tempo  C/L/R: mute ch 1-6\n");
    uart_puts("========================================\n\n");

    /* GIC + V3s codec for software PSG mix */
    irq_init();
    audio_init_24bit();

    /* SN76489 software PSG (Genesis 4-channel chip) */
    psg_chip = sn76489_jupiter_init(3579545, PSG_RATE);
    if (psg_chip) {
        sn76489_jupiter_reset(psg_chip);
        sn76489_jupiter_unmute(psg_chip);
    }

    /* Pre-fill silence + start codec DMA */
    for (uint32_t i = 0; i < TARGET_DEPTH; i++) {
        mix_buf[mix_wr & MIX_BUF_MASK] = 0;
        mix_wr++;
    }
    audio_start();
    irq_global_enable();

    /* YM3438: external 8 MHz crystal — V3s does NO clock setup. The osc
     * has been free-running since the board powered up. ym3438_hw_init
     * just configures GPIO + pulses /IC to reset the chip. */
    ym3438_hw_set_clock(YM_CLK_EXT_XTAL);
    ym3438_hw_init();

    vgm_init(vgm_sortie, vgm_sortie_len);
    uart_puts("[vgm] Comix Zone — Sortie loaded\n");

    input_init(INPUT_N64);

    uart_puts("[main] playing!\n\n");

    /* ---- Playback loop ---- */
    uint32_t vgm_accum = 0;
    int      tempo_pct = 100;  /* 50..200 */

    while (1) {
        if (!vgm_done) {
            uint32_t target = (735u * 100u) / (uint32_t)tempo_pct;
            while (vgm_accum < target) {
                uint32_t wait = vgm_tick();
                if (vgm_done) break;
                vgm_accum += wait;
            }
            vgm_accum -= target;
        }

        (void)input_poll();
        uint32_t pressed = input_pressed();

        if (pressed & BTN_A) {
            vgm_restart();
            uart_puts("[xtal] restart\n");
        }
        if (pressed & BTN_START) {
            vgm_paused = !vgm_paused;
            uart_puts(vgm_paused ? "[xtal] paused\n" : "[xtal] resumed\n");
            if (vgm_paused) {
                for (int ch = 0; ch < 3; ch++) {
                    ym3438_hw_vgm_write(0, 0x28, ch);
                    ym3438_hw_vgm_write(0, 0x28, ch | 0x04);
                }
            }
        }
        if (pressed & BTN_B) {
            tempo_pct    = 100;
            channel_mute = 0;
            uart_puts("[xtal] defaults restored\n");
        }
        if (pressed & BTN_UP) {
            tempo_pct += 10;
            if (tempo_pct > 200) tempo_pct = 200;
            uart_puts("[xtal] tempo="); uart_putdec(tempo_pct); uart_puts("%\n");
        }
        if (pressed & BTN_DOWN) {
            tempo_pct -= 10;
            if (tempo_pct < 50) tempo_pct = 50;
            uart_puts("[xtal] tempo="); uart_putdec(tempo_pct); uart_puts("%\n");
        }
        if (pressed & BTN_C_UP)    channel_mute ^= (1 << 0);
        if (pressed & BTN_C_DOWN)  channel_mute ^= (1 << 1);
        if (pressed & BTN_C_LEFT)  channel_mute ^= (1 << 2);
        if (pressed & BTN_C_RIGHT) channel_mute ^= (1 << 3);
        if (pressed & BTN_L)       channel_mute ^= (1 << 4);
        if (pressed & BTN_R)       channel_mute ^= (1 << 5);

        /* PSG render → V3s codec mix buffer */
        while ((mix_wr - mix_rd) < TARGET_DEPTH) {
            int32_t l = 0, r = 0;
            if (psg_chip) sn76489_jupiter_update(psg_chip, 1, &l, &r);
            int32_t mono = (l + r) >> 1;
            if (mono >  32767) mono =  32767;
            if (mono < -32768) mono = -32768;
            mix_buf[mix_wr++ & MIX_BUF_MASK] = (int16_t)mono;
            mix_buf[mix_wr++ & MIX_BUF_MASK] = (int16_t)mono;
        }
    }
    return 0;
}
