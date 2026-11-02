#ifndef JUPITER_PINE64_VIDEO_H
#define JUPITER_PINE64_VIDEO_H

#include <stdint.h>

/* The logical canvas stays 480x272 inside U-Boot's progressive HDMI mode. */
int pine64_video_is_ready(void);
uint32_t pine64_video_output_width(void);
uint32_t pine64_video_output_height(void);
uint32_t pine64_video_total_lines(void);
/* Measured against the board's 24 MHz timer. Zero means no measurement. */
uint32_t pine64_video_frame_ticks(void);
uint32_t pine64_video_line_ticks(void);
int pine64_video_measure_timing(void);

/* XRGB8888 source -> 480x272 game canvas using VI0's real DE2 VSU.
 * Only bounded upscaling is supported. Overlay and PIP remain unscaled.
 * video_init() restores the ordinary 480x272 framebuffer layout. */
int pine64_video_scale_rgb(uint32_t address, uint32_t width, uint32_t height);

/* Board HSTimer extensions. A64 has one timer (index 0). */
int pine64_hstimer_ready(void);
uint32_t pine64_hstimer_hz(void);
uint32_t pine64_hstimer_ticks_from_us(uint32_t us);
void pine64_hstimer_set_us(uint32_t us, int oneshot, void (*callback)(void));

#endif
