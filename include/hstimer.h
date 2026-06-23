/*
 * Jupiter SDK — High-Speed Timer (HSTimer)
 *
 * Two independent 56-bit down-counters clocked from AHB (~200MHz, 5ns/tick).
 * Used for scanline-accurate mid-frame interrupts (raster effects).
 *
 * Base: 0x01C60000
 * IRQ: timer0 = GIC SPI 51 (IRQ 83), timer1 = GIC SPI 52 (IRQ 84)
 * Clock gate: BUS_CLK_GATING0 bit 19, reset: BUS_SOFT_RST0 bit 19
 */
#ifndef HSTIMER_H
#define HSTIMER_H

#include <stdint.h>

/* Ticks per scanline at our panel timing:
 * Panel: 525 pixels/line @ 9MHz pixel clock = 58.33μs/line
 * AHB @ 200MHz = 5ns/tick → 11,666 ticks/scanline
 * Adjust if AHB clock differs. */
#define HSTIMER_TICKS_PER_SCANLINE  11666

/* Initialize HSTimer hardware (clock gate + reset) */
void hstimer_init(void);

/* Set a one-shot interrupt to fire after `scanlines` lines from NOW.
 * Calls `callback` in IRQ context when it fires.
 * Timer 0 or 1 (0 or 1). */
void hstimer_set_scanline(int timer, uint32_t scanlines, void (*callback)(void));

/* Set a repeating interrupt every `scanlines` lines.
 * Callback fires each time. Good for continuous raster effects. */
void hstimer_set_repeating(int timer, uint32_t scanlines, void (*callback)(void));

/* Stop a timer */
void hstimer_stop(int timer);

/* Set raw tick interval (for sub-scanline precision) */
void hstimer_set_ticks(int timer, uint32_t ticks, int oneshot, void (*callback)(void));

/* One-shot helper: arm timer 0 to fire every `scanlines` lines and refill
 * mix_buf to a high-water mark. Captures the audio_mix_tick boilerplate
 * the cedar_video_av / opn2_hw_live / mt32_rt examples each wrote by
 * hand (cedar_video_av:55-62 was the canonical version). The callback
 * does the standard depth-driven top-up:
 *   if ((mix_wr - mix_rd) < high_water) audio_mix(samples_per_tick);
 * `samples_per_tick` is in stereo frames (a value of 64 produces 128
 * mix_buf slots, ~1.45 ms at 44.1 kHz). `scanlines = 32` matches the
 * cedar_video_av cadence (~1.87 ms ticks). */
void hstimer_audio_mix_init(uint32_t scanlines, uint32_t samples_per_tick);

#endif
