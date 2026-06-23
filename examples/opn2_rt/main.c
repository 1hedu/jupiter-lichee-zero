/*
 * Jupiter SDK — Nuked-OPN2 Real-Time Playback
 *
 * Cycle-accurate YM2612 emulation running in real time on the
 * Cortex-A7 at 1.2 GHz. No display rendering — all CPU goes to
 * the emulator. UART prints timing stats.
 *
 * Build: make GAME=examples/opn2_rt/main.c
 */
#include "jupiter.h"
#include "pmu.h"
#include "../../libvgm/vgm_player.h"
#include "fighting_back.h"

/* ---- Audio plumbing ---- */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;

static vgm_player_t vgm;
static int16_t stereo[2048];

static void vgm_fill(uint32_t n)
{
    uint32_t done = 0;
    while (done < n) {
        uint32_t chunk = n - done;
        if (chunk > 128) chunk = 128;
        vgm_render(&vgm, stereo + done * 2, chunk);
        for (uint32_t i = 0; i < chunk; i++) {
            if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2))
                continue;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2];
            mix_wr++;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2 + 1];
            mix_wr++;
        }
        audio_update();
        done += chunk;
    }
}

/* ---- Main ---- */

void main(void)
{
    uart_puts("\n\n=== Nuked-OPN2 Real-Time — Fighting Back ===\n");
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();

    /* Report CPU clock */
    {
        uint32_t pll = *(volatile uint32_t *)0x01C20000;
        uint32_t n = ((pll >> 8) & 0x1F) + 1;
        uint32_t k = ((pll >> 4) & 0x3) + 1;
        uint32_t m = (pll & 0x3) + 1;
        uart_puts("[cpu] "); uart_putdec(24 * n * k / m); uart_puts(" MHz\n");
    }

    audio_init();

    uart_puts("Loading Fighting Back...\n");
    vgm_load(&vgm, vgm_fighting_back, vgm_fighting_back_len, 0);
    audio_set_rate(vgm.sample_rate);
    vgm_play(&vgm);

    uint32_t spf = vgm.sample_rate / 60;
    uart_puts("Rate="); uart_putdec(vgm.sample_rate);
    uart_puts(" Hz, spf="); uart_putdec(spf); uart_puts("\n");

    /* Pre-fill then start */
    vgm_fill(spf);
    vgm_fill(spf);
    audio_start();

    uart_puts("Playing...\n\n");

    uint32_t frame = 0;
    uint64_t pmu_total = 0;
    uint32_t pmu_frames = 0;
    uint32_t pmu_min = 0xFFFFFFFF;
    uint32_t pmu_max = 0;
    uint32_t last_underruns = 0;

    while (1) {
        uint32_t t0 = timer_read();

        /* Render one frame's worth of audio */
        uint32_t c0 = pmu_cycles();
        vgm_fill(spf);
        uint32_t c1 = pmu_cycles();

        uint32_t elapsed = c1 - c0;
        uint32_t per_sample = elapsed / spf;
        pmu_total += elapsed;
        pmu_frames++;
        if (per_sample < pmu_min) pmu_min = per_sample;
        if (per_sample > pmu_max) pmu_max = per_sample;

        /* Pace to 60fps, keep DMA fed */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            audio_update();

        /* Loop song */
        if (vgm.cur_loop >= 1) {
            vgm_stop(&vgm);
            vgm_unload(&vgm);
            vgm_load(&vgm, vgm_fighting_back, vgm_fighting_back_len, 0);
            vgm_play(&vgm);
        }

        /* Stats every second */
        if ((frame % 60) == 0 && frame > 0) {
            uint32_t fill = mix_wr - mix_rd;
            uint32_t underruns = audio_underruns - last_underruns;
            last_underruns = audio_underruns;

            uint32_t pll = *(volatile uint32_t *)0x01C20000;
            uint32_t n = ((pll >> 8) & 0x1F) + 1;
            uint32_t k = ((pll >> 4) & 0x3) + 1;
            uint32_t m = (pll & 0x3) + 1;
            uint32_t budget = (24000000 * n * k / m) / vgm.sample_rate;

            uint32_t avg = (uint32_t)(pmu_total / (pmu_frames * spf));

            uart_puts("t="); uart_putdec(frame / 60);
            uart_puts("s avg="); uart_putdec(avg);
            uart_puts(" min="); uart_putdec(pmu_min);
            uart_puts(" max="); uart_putdec(pmu_max);
            uart_puts(" budget="); uart_putdec(budget);
            uart_puts(" ("); uart_putdec(avg * 100 / budget); uart_puts("%)");
            uart_puts(" fill="); uart_putdec(fill);
            if (underruns) {
                uart_puts(" UNDERRUNS="); uart_putdec(underruns);
            }
            uart_puts("\n");

            pmu_total = 0;
            pmu_frames = 0;
            pmu_min = 0xFFFFFFFF;
            pmu_max = 0;
        }
        frame++;
    }
}
