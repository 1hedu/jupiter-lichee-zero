/*
 * Jupiter SDK — MT-32 Live Audio POC
 *
 * Full bring-up with audible output:
 *   1. Load ROMs from embedded C arrays
 *   2. Open mt32emu synth at native 32 kHz
 *   3. Configure DAC to 32 kHz (no resampling needed)
 *   4. Play a C major chord via play_msg_on_part (bypasses channel routing)
 *   5. Loop forever, rendering mt32emu output directly into mix_buf
 *
 * Build: make GAME=examples/mt32_poc/main.c
 */
#include "jupiter.h"
#include "c_interface/c_interface.h"
#include "mt32_control_rom.h"
#include "mt32_pcm_rom.h"

/* Audio ring buffer (shared with audio.c DMA ISR) */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;

/* Staging buffer for one chunk of mt32emu render output */
#define CHUNK_FRAMES 128
static int16_t chunk[CHUNK_FRAMES * 2];

static mt32emu_context ctx;

/* Render a chunk from mt32emu and drain it into mix_buf. */
static void mt32_fill(uint32_t frames)
{
    uint32_t done = 0;
    while (done < frames) {
        uint32_t n = frames - done;
        if (n > CHUNK_FRAMES) n = CHUNK_FRAMES;
        mt32emu_render_bit16s(ctx, chunk, n);
        for (uint32_t i = 0; i < n; i++) {
            if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2))
                break;  /* ring buffer full; ISR will catch up */
            mix_buf[mix_wr & MIX_BUF_MASK] = chunk[i * 2];     /* L */
            mix_wr++;
            mix_buf[mix_wr & MIX_BUF_MASK] = chunk[i * 2 + 1]; /* R */
            mix_wr++;
        }
        done += n;
    }
}

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();

    uart_puts("\n\n=== Jupiter SDK — MT-32 Live Audio POC ===\n");
    uart_puts("Real-time LA synthesis through the DAC.\n\n");

    cpp_init();
    audio_init();

    /* ---- Create synth, load ROMs, open ---- */
    mt32emu_report_handler_i null_report = { 0 };
    ctx = mt32emu_create_context(null_report, 0);

    mt32emu_return_code rc;
    rc = mt32emu_add_rom_data(ctx, mt32_control_rom, mt32_control_rom_len, 0);
    if (rc != MT32EMU_RC_ADDED_CONTROL_ROM) {
        uart_puts("FAIL: control ROM rejected\n");
        while (1) ;
    }
    rc = mt32emu_add_rom_data(ctx, mt32_pcm_rom, mt32_pcm_rom_len, 0);
    if (rc != MT32EMU_RC_ADDED_PCM_ROM) {
        uart_puts("FAIL: PCM ROM rejected\n");
        while (1) ;
    }
    /* Request 48 kHz output so we can feed mix_buf directly — our DAC
     * doesn't have a 32 kHz PLL config. Munt's internal resampler
     * handles the 32 -> 48 conversion. */
    mt32emu_set_stereo_output_samplerate(ctx, 48000.0);
    rc = mt32emu_open_synth(ctx);
    if (rc != MT32EMU_RC_OK) {
        uart_puts("FAIL: open_synth\n");
        while (1) ;
    }
    uart_puts("Synth open, rate = ");
    uart_putdec(mt32emu_get_actual_stereo_output_samplerate(ctx));
    uart_puts(" Hz\n");

    /* Crank output gain a bit so it's clearly audible */
    mt32emu_set_output_gain(ctx, 2.0f);

    /* ---- Configure DAC to 48 kHz (mt32emu resamples from 32k for us) ---- */
    audio_set_rate(48000);

    /* ---- Play a C major chord on part 0 (bypass channel routing) ----
     * play_msg_on_part args: (ctx, part, command, data1, data2)
     * command 0x9 = Note On (top nibble of MIDI status),
     * command 0x8 = Note Off. */
    mt32emu_play_msg_on_part(ctx, 0, 0x9, 60, 100);  /* C4 */
    mt32emu_play_msg_on_part(ctx, 0, 0x9, 64, 100);  /* E4 */
    mt32emu_play_msg_on_part(ctx, 0, 0x9, 67, 100);  /* G4 */

    /* ---- Pre-fill the mix buffer, start DMA, unmask IRQs ---- */
    mt32_fill(1024);
    mt32_fill(1024);
    audio_start();
    irq_global_enable();

    uart_puts("Playing C major chord on part 0...\n");
    uart_puts("If you hear LA synth tones, Munt is fully alive on the V3s.\n\n");

    /* ---- Main loop: pump-based, keep ring buffer at TARGET_DEPTH ----
     * Produces samples whenever the ring dips below target. Self-correcting
     * against any clock drift. Uses wall-clock time only for triggering
     * note-on/off events, not for pacing audio. */
    #define TARGET_DEPTH 2400    /* 25 ms of 48 kHz stereo = sturdy margin */

    uint32_t note_state = 1;            /* 0 = off, 1 = on */
    uint32_t next_toggle_us = 5000000;  /* first toggle at t=5s */
    uint32_t start_t = timer_read();
    uint32_t last_print_us = 0;
    uint32_t total_render_us = 0;
    uint32_t total_render_samples = 0;

    while (1) {
        /* Render in small chunks until the ring is at the target depth */
        while ((mix_wr - mix_rd) < TARGET_DEPTH) {
            uint32_t t0 = timer_read();
            mt32_fill(CHUNK_FRAMES);
            total_render_us += ticks_to_us(timer_elapsed(t0, timer_read()));
            total_render_samples += CHUNK_FRAMES;
        }

        uint32_t now_us = ticks_to_us(timer_elapsed(start_t, timer_read()));

        /* Chord on/off every 5 seconds */
        if (now_us >= next_toggle_us) {
            if (note_state) {
                mt32emu_play_msg_on_part(ctx, 0, 0x8, 60, 0);
                mt32emu_play_msg_on_part(ctx, 0, 0x8, 64, 0);
                mt32emu_play_msg_on_part(ctx, 0, 0x8, 67, 0);
                note_state = 0;
            } else {
                mt32emu_play_msg_on_part(ctx, 0, 0x9, 60, 100);
                mt32emu_play_msg_on_part(ctx, 0, 0x9, 64, 100);
                mt32emu_play_msg_on_part(ctx, 0, 0x9, 67, 100);
                note_state = 1;
            }
            next_toggle_us += 5000000;
        }

        /* Stats once per second */
        if (now_us - last_print_us >= 1000000) {
            uint32_t ns_per_sample =
                total_render_samples
                    ? (total_render_us * 1000 / total_render_samples)
                    : 0;
            uart_puts("t=");  uart_putdec(now_us / 1000000);
            uart_puts("s fill="); uart_putdec(mix_wr - mix_rd);
            uart_puts(" ns/sample="); uart_putdec(ns_per_sample);
            uart_puts("\n");
            last_print_us = now_us;
            total_render_us = 0;
            total_render_samples = 0;
        }
    }
}
