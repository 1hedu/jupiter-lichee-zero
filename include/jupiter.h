/*
 * Jupiter SDK — Bare-metal game development kit for Lichee Pi Zero
 *
 * Single include file. Pull in hardware registers and all library functions.
 */
#ifndef JUPITER_H
#define JUPITER_H

#include "v3s.h"

/* ---- uart.c ---- */
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(uint32_t val);
void uart_putdec(uint32_t val);

/* ---- timer.c ---- */
void     timer_init(void);
uint32_t timer_read(void);
uint32_t timer_elapsed(uint32_t start, uint32_t end);
uint32_t ticks_to_us(uint32_t ticks);
uint32_t ticks_to_ms(uint32_t ticks);

/* ---- mem.c ---- */
void memset32(uint32_t addr, uint32_t val, uint32_t count);
void memset32_neon(uint32_t addr, uint32_t val, uint32_t bytes);
void memcpy_neon(void *dst, const void *src, uint32_t bytes);

/* ---- video.c ---- */
void video_init(void);                     /* full CCU+GPIO+DE2+TCON init */
void video_swap(uint32_t fb_addr);         /* swap VI0 framebuffer + commit */
void video_set_overlay(uint32_t ovl_addr); /* swap UI0 overlay + commit */
void video_commit(void);                   /* just commit the double buffer */
void video_vi1_init(uint32_t x, uint32_t y, uint32_t w, uint32_t h); /* PIP overlay */
void video_wait_vblank(void);  /* Poll until TCON0 vblank */

/* ---- tiles.c ---- */
typedef uint32_t (*tile_color_fn)(uint8_t tile_id, uint32_t px, uint32_t py);

void tiles_render(volatile uint32_t *fb, uint32_t fb_pitch,
                  const uint8_t *map, uint32_t map_w,
                  uint32_t y_start, uint32_t rows,
                  int scroll_x, tile_color_fn color_fn);

void tiles_render_fast(volatile uint32_t *fb, uint32_t fb_pitch,
                       const uint8_t *map, uint32_t map_w,
                       const uint32_t *color_lut,
                       uint32_t y_start, uint32_t rows,
                       int scroll_x);

/* ---- mmu.c ---- */
void mmu_init(void);                            /* identity-map MMU + enable D-cache */
void dcache_clean_range(uint32_t addr, uint32_t size); /* flush dirty lines to DRAM */

/* ---- irq.c ---- */
void irq_init(void);
void irq_register(uint32_t irq_id, void (*handler)(void));
void irq_enable(uint32_t irq_id);
void irq_disable(uint32_t irq_id);
void irq_global_enable(void);

/* Flush a full framebuffer so DE2 can see it */
#define dcache_clean_fb(fb_addr) dcache_clean_range((fb_addr), LCD_FB_BYTES)

/* ---- sprite.c + sprite_neon.S ---- */
void sprite_blit(volatile uint32_t *dst, uint32_t dst_pitch,
                 const uint32_t *src, uint32_t src_w, uint32_t src_h,
                 int dx, int dy);

void sprite_blit_flip(volatile uint32_t *dst, uint32_t dst_pitch,
                      const uint32_t *src, uint32_t src_w, uint32_t src_h,
                      int dx, int dy);

void sprite_blit_blend(volatile uint32_t *dst, uint32_t dst_pitch,
                       const uint32_t *src, uint32_t src_w, uint32_t src_h,
                       int dx, int dy);

void sprite_blit_scaled(volatile uint32_t *dst, uint32_t dst_pitch,
                        const uint32_t *src, uint32_t src_w, uint32_t src_h,
                        int dx, int dy,
                        uint32_t dst_w, uint32_t dst_h);

void sprite_blit_indexed(volatile uint32_t *dst, uint32_t dst_pitch,
                         const uint8_t *src, uint32_t src_w, uint32_t src_h,
                         const uint32_t *palette,
                         int dx, int dy);

void sprite_blit_rotscale(volatile uint32_t *dst, uint32_t dst_pitch,
                          const uint32_t *src, uint32_t src_w, uint32_t src_h,
                          int cx, int cy,
                          uint8_t angle, uint32_t scale_fp);

void sprite_clear(volatile uint32_t *dst, uint32_t dst_pitch,
                  uint32_t src_w, uint32_t src_h,
                  int dx, int dy);

/* ---- mode7_neon.S ---- */
void mode7_scanline(uint32_t *row, const uint8_t *map, const uint32_t *lut,
                    int32_t u, int32_t v, int32_t du, int32_t dv,
                    uint32_t count, uint32_t map_w_bits, uint32_t map_mask);

void iso_scanline(uint32_t *row, const uint8_t *map, const uint32_t *lut,
                  int32_t u, int32_t v, int32_t du, int32_t dv,
                  uint32_t count);

/* ---- audio.c ---- */

/* FM Synthesizer types */
#define FM_NUM_CH 6

typedef struct {
    uint32_t phase;
    uint32_t phase_inc;
    uint8_t  mul;
    int8_t   detune;
    uint8_t  tl;
    uint8_t  ar, dr, sr, rr, sl;
    uint16_t env;
    uint8_t  env_state;
} fm_op_t;

typedef struct {
    fm_op_t  op[4];
    uint32_t freq_step;
    uint8_t  algorithm;
    uint8_t  feedback;
    int32_t  fb_out[2];
    uint8_t  active;
} fm_ch_t;

/* Run all C++ static constructors (from .init_array). Must be called
 * once at startup before any C++ library (e.g. mt32emu) is used. */
#ifdef __cplusplus
extern "C" {
#endif
void cpp_init(void);
#ifdef __cplusplus
}
#endif

void audio_init(void);
void audio_init_24bit(void);  /* 24-bit DAC (S32_LE) — call INSTEAD of audio_init */
void audio_start(void);
void audio_update(void);
void audio_set_rate(uint32_t rate);
void audio_set_hp_vol(uint8_t vol_0_63);  /* analog HP PA gain, -63..0 dB */

/* One-shot bring-up: audio_init + audio_set_rate + analog gain to max +
 * digital master to max + pre-fill mix_buf with 2048 silence frames so
 * audio_start's pre-fill of both DMA halves sees valid samples + audio_
 * start. Captures the boilerplate that 18+ examples (opn2_rt, mt32_rt,
 * cedar_video_av, sdmmc_music, war1...) repeated verbatim. Caller is
 * still responsible for irq_global_enable() — that has whole-system
 * side effects beyond audio so we don't fold it in. */
void audio_quickstart(uint32_t sample_rate);
void audio_mix(uint32_t num_samples);
void audio_pcm_play(uint32_t channel, const int16_t *samples,
                    uint32_t length, uint8_t volume, uint8_t loop);
/* Same as audio_pcm_play but plays at src_rate (Hz); the mixer steps
 * through samples at (src_rate / SAMPLE_RATE) per output frame. Lets us
 * play 11025 Hz War1gus WAVs without pre-resampling to 48 kHz. */
void audio_pcm_play_rate(uint32_t channel, const int16_t *samples,
                         uint32_t length, uint8_t volume, uint8_t loop,
                         uint32_t src_rate);
int  audio_pcm_channel_busy(uint32_t channel);
void audio_pcm_stop(uint32_t channel);
void audio_set_master_volume(uint8_t volume);
const int16_t *audio_generate_test_tone(uint32_t freq_hz, uint32_t duration_ms,
                                        uint32_t *out_length);
void audio_apu_mix(uint32_t num_samples);
void audio_apu_note_on(uint32_t channel, uint32_t frequency,
                       uint8_t volume, uint8_t duty);
void audio_apu_note_on_env(uint32_t channel, uint32_t frequency,
                           uint8_t volume, uint8_t duty,
                           int8_t env_dir, uint8_t env_period);
void audio_apu_note_off(uint32_t channel);
void audio_apu_wave_set(const uint8_t wave[32]);
void audio_apu_wave_on(uint32_t frequency, uint8_t volume_shift);
void audio_apu_wave_off(void);
void audio_apu_noise_on(uint8_t volume, uint8_t period_code, uint8_t width_mode);
void audio_apu_noise_on_env(uint8_t volume, uint8_t period_code,
                            uint8_t width_mode, int8_t env_dir,
                            uint8_t env_period);
void audio_apu_noise_off(void);
void audio_apu_all_off(void);
void audio_genesis_mix(uint32_t num_samples);
void audio_genesis_mix_asm(uint32_t num_samples);
void audio_fm_set_algorithm(uint32_t ch, uint8_t algorithm, uint8_t feedback);
void audio_fm_set_freq(uint32_t ch, uint32_t freq_hz);
void audio_fm_set_operator(uint32_t ch, uint32_t op,
    uint8_t mul, uint8_t tl, uint8_t ar, uint8_t dr,
    uint8_t sl, uint8_t sr, uint8_t rr);
void audio_fm_key_on(uint32_t ch);
void audio_fm_key_off(uint32_t ch);
void audio_psg_tone_on(uint32_t ch, uint32_t freq_hz, uint8_t vol);
void audio_psg_tone_off(uint32_t ch);
void audio_psg_noise_on(uint8_t vol, uint8_t rate, uint8_t periodic);
void audio_psg_noise_off(void);
void audio_genesis_all_off(void);

/* ---- Drawing helpers (inline, no .c needed) ---- */
static inline void draw_rect(volatile uint32_t *buf, uint32_t pitch,
                              int x, int y, int w, int h, uint32_t color)
{
    for (int r = y; r < y + h; r++)
        for (int c = x; c < x + w; c++)
            buf[r * pitch + c] = color;
}

static inline void clear_rect(volatile uint32_t *buf, uint32_t pitch,
                               int x, int y, int w, int h)
{
    for (int r = y; r < y + h; r++)
        for (int c = x; c < x + w; c++)
            buf[r * pitch + c] = 0x00000000;
}

/* ---- cedar.c ---- */
void cedar_init(void);
int  cedar_h264_decode(const uint8_t *h264_data, uint32_t h264_size,
                       uint32_t w, uint32_t h, uint32_t hdr_bits,
                       int pic_init_qp, int slice_qp_delta,
                       int chroma_qp_off, int disable_deblock);
void cedar_argb_to_nv12(const uint32_t *src, uint32_t src_pitch,
                        uint32_t w, uint32_t h);
int  cedar_h264_encode(uint32_t w, uint32_t h, int qp,
                       const uint8_t *nal_header, uint32_t nal_header_len);
void cedar_nv12_to_argb(uint32_t *dst, uint32_t dst_pitch,
                        uint32_t w, uint32_t h);
void dcache_invalidate_range(uint32_t addr, uint32_t size);

/* ---- sram.c ---- */
void     sram_init(void);    /* Remap SRAM_C to CPU + probe size */
uint32_t sram_probe(void);   /* Walk SRAM_C, return actual size in bytes */
uint32_t sram_size(void);    /* Cached result from sram_init */

/* ---- si5351.c ---- */
void si5351_init(void);  /* Program Si5351 CLK0 = 7.670454 MHz via I2C, then release PB6/PB7 */

/* ---- ym3438_hw.c ---- */
#define YM_CLK_SI5351   0   /* Si5351 at 7.670453 MHz (exact NTSC) */
#define YM_CLK_CCU_MCLK 1   /* CCU MCLK on PE1: 24/3 = 8.0 MHz */
#define YM_CLK_EXT_XTAL 2   /* External 8 MHz crystal-osc module wired
                             * directly to YM3438 pin 24. No V3s GPIO
                             * touched, no CCU register writes — just
                             * GPIO bus init + chip reset. F-Number is
                             * still scaled by 7.670453/8.0 same as the
                             * CCU MCLK path. */
void ym3438_hw_set_clock(int source);                            /* Call before hw_init */
void ym3438_hw_init(void);                                       /* GPIO init + clock + reset */
void ym3438_hw_write(uint8_t addr, uint8_t data);               /* Port 0 register write */
void ym3438_hw_write_port1(uint8_t addr, uint8_t data);         /* Port 1 register write */
void ym3438_hw_busy_wait(void);                                  /* Inter-write delay */
void ym3438_hw_vgm_write(uint8_t port, uint8_t addr, uint8_t data); /* VGM write + F-Num scale */

#endif /* JUPITER_H */
