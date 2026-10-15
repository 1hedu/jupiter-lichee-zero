/*
 * Mercury — PicoDVI Scanout (PIO-serialised DVI; RP2040 or RP2350)
 *
 * Runs on Core 1. Scans out the front framebuffer as DVI (HDMI) at
 * 640×480 @ 60Hz, scaled 2x: PicoDVI pixel-doubles horizontally and
 * repeats each line (DVI_VERTICAL_REPEAT=2), so the 320×224
 * framebuffer appears as 640×448 with 16-line black bars top/bottom.
 *
 * The TC358743 bridge captures this as HDMI and outputs MIPI CSI-2
 * to the V3s. The V3s CSI receiver crops to the active area.
 *
 * Uses PicoDVI library (Wren6991/PicoDVI) for TMDS encoding and
 * PIO serialization. TMDS pins come from the board preset in
 * jupiter32x.h (default: SpotPear Pico HDMI board, GP8-15).
 *
 * Scanout interface: j32x_scanout_preinit() (core 0, first thing in
 * main) + j32x_scanout_start() + j32x_scanout_loop() (core 1).
 */

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"     /* set_sys_clock_khz (SDK 2.x) */
#include "hardware/vreg.h"
#include "jupiter32x.h"
#include "dvi.h"
#include "dvi_timing.h"
#include "tmds_encode.h"

#define DVI_TIMING dvi_timing_640x480p_60hz

/* DVI instance */
static struct dvi_inst dvi0;

/* PicoDVI geometry. With DVI_SYMBOLS_PER_WORD=2 the 8bpp encoder
 * pixel-doubles horizontally: n_pix INPUT pixels become n_pix TMDS
 * words per lane = 2*n_pix screen pixels ("Scanline buffers are
 * half-resolution; the functions take the number of *input* pixels" —
 * libdvi's own _dvi_prepare_scanline_8bpp, which this mirrors). Each
 * TMDS buffer is repeated DVI_VERTICAL_REPEAT lines. So the logical
 * canvas is 320x240 shown at 2x: the 320x224 framebuffer fills the
 * width with an 8-line letterbox top and bottom. */
#define DVI_OUT_W   (J32X_DVI_H_ACTIVE / 2)                    /* 320 */
#define DVI_OUT_H   (J32X_DVI_V_ACTIVE / DVI_VERTICAL_REPEAT)  /* 240 */
#define DVI_WORDS_PER_CHANNEL (J32X_DVI_H_ACTIVE / DVI_SYMBOLS_PER_WORD)
_Static_assert(DVI_SYMBOLS_PER_WORD == 2, "encoder geometry assumes 2");

/* Encoder reads pixels as 32-bit words: buffers must be word-aligned */
static uint8_t black_line[DVI_OUT_W] __attribute__((aligned(4)));
static uint8_t line_buf[DVI_OUT_W]   __attribute__((aligned(4)));

/* PicoDVI pin config from the board preset */
static const struct dvi_serialiser_cfg mercury_dvi_cfg = {
    .pio = pio0,
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {MERCURY_DVI_D0, MERCURY_DVI_D1, MERCURY_DVI_D2},
    .pins_clk = MERCURY_DVI_CLK,
    .invert_diffpairs = MERCURY_DVI_INVERT,
};

/* PicoDVI serialises one TMDS bit per system clock, so clk_sys MUST
 * equal the bit clock (252 MHz for 640x480p60) — the same sequence
 * every PicoDVI demo (incl. SpotPear's) runs first. Without it the
 * link runs at half rate and no monitor syncs. Called on core 0
 * before stdio/SPI init so clk_peri settles before peripherals use it.
 * (On RP2350 this is an overclock from the 150 MHz rating, as in
 * PicoDVI's own RP2350 builds.) */
void j32x_scanout_preinit(void)
{
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);
}

void j32x_scanout_start(void)
{
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = mercury_dvi_cfg;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    memset(black_line, 0, sizeof(black_line));
}

static void __not_in_flash_func(encode_line)(uint32_t *tmdsbuf,
                                             const uint8_t *pix)
{
    const uint32_t *p = (const uint32_t *)pix;
    tmds_encode_data_channel_8bpp(p, tmdsbuf + 0 * DVI_WORDS_PER_CHANNEL,
                                  DVI_OUT_W, DVI_8BPP_BLUE_MSB,  DVI_8BPP_BLUE_LSB);
    tmds_encode_data_channel_8bpp(p, tmdsbuf + 1 * DVI_WORDS_PER_CHANNEL,
                                  DVI_OUT_W, DVI_8BPP_GREEN_MSB, DVI_8BPP_GREEN_LSB);
    tmds_encode_data_channel_8bpp(p, tmdsbuf + 2 * DVI_WORDS_PER_CHANNEL,
                                  DVI_OUT_W, DVI_8BPP_RED_MSB,   DVI_8BPP_RED_LSB);
}

void j32x_scanout_loop(void)
{
    /* Feed one TMDS-encoded line per DVI_VERTICAL_REPEAT output lines;
     * PicoDVI's DMA IRQ handles timing, blanking and sync. */
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);

    while (1) {
        j32x_swap_apply();   /* flip front/back only between frames */
        const uint8_t *fb = g_state.fb[g_state.front];
        uint32_t w = g_state.width  ? g_state.width  : J32X_WIDTH;
        uint32_t h = g_state.height ? g_state.height : J32X_HEIGHT;
        if (w > DVI_OUT_W) w = DVI_OUT_W;
        if (h > DVI_OUT_H) h = DVI_OUT_H;
        uint32_t x_off = (DVI_OUT_W - w) / 2;
        uint32_t y_off = (DVI_OUT_H - h) / 2;

        for (uint32_t y = 0; y < DVI_OUT_H; y++) {
            uint32_t *tmdsbuf;
            queue_remove_blocking(&dvi0.q_tmds_free, &tmdsbuf);

            if (y < y_off || y >= y_off + h) {
                encode_line(tmdsbuf, black_line);       /* letterbox */
            } else {
                /* pillarbox only if the framebuffer is narrower than 320 */
                memset(line_buf, 0, x_off);
                memcpy(line_buf + x_off, &fb[(y - y_off) * w], w);
                memset(line_buf + x_off + w, 0, DVI_OUT_W - x_off - w);
                encode_line(tmdsbuf, line_buf);
            }

            queue_add_blocking(&dvi0.q_tmds_valid, &tmdsbuf);
        }

        g_state.frame_count++;
    }
}
