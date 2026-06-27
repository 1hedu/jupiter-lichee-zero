/*
 * Jupiter Mars — PicoDVI Scanout
 *
 * Runs on Core 1. Scans out the front framebuffer as DVI (HDMI) at
 * 640×480 @ 60Hz. The active area is J32X_WIDTH × J32X_HEIGHT,
 * centered in the 640×480 frame. Surrounding area is black.
 *
 * The TC358743 bridge captures this as HDMI and outputs MIPI CSI-2
 * to the V3s. The V3s CSI receiver crops to the active area.
 *
 * Uses PicoDVI library (Wren6991/PicoDVI) for TMDS encoding and
 * PIO serialization. Pins GP0-GP7 carry the 4 TMDS differential pairs.
 *
 * Scanout interface: j32x_scanout_start() + j32x_scanout_loop()
 * Swap dvi_out.c for mipi_out.c to switch to direct PIO MIPI TX.
 */

#include "pico/stdlib.h"
#include "jupiter32x.h"
#include "dvi.h"
#include "dvi_timing.h"
#include "common_dvi_pin_configs.h"
#include "tmds_encode.h"

/* DVI instance */
static struct dvi_inst dvi0;

/* Black scanline for letterbox/pillarbox regions */
static uint8_t black_line[J32X_DVI_H_ACTIVE];

/* PicoDVI pin config for GP0-GP7 */
static const struct dvi_serialiser_cfg mars_dvi_cfg = {
    .pio = pio0,
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {PIN_DVI_D0P, PIN_DVI_D1P, PIN_DVI_D2P},
    .pins_clk = PIN_DVI_CLKP,
    .invert_diffpairs = false,
};

void j32x_scanout_start(void)
{
    dvi0.timing = &dvi_timing_640x480p_60hz;
    dvi0.ser_cfg = mars_dvi_cfg;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    memset(black_line, 0, sizeof(black_line));
}

void j32x_scanout_loop(void)
{
    /* DVI main loop: feed one scanline at a time to the TMDS encoder.
     * PicoDVI handles timing, blanking, and sync internally. We just
     * provide pixel data for each active line. */

    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);

    while (1) {
        const uint8_t *fb = g_state.fb[g_state.front];
        uint16_t w = g_state.width ? g_state.width : J32X_WIDTH;
        uint16_t h = g_state.height ? g_state.height : J32X_HEIGHT;
        uint16_t x_off = (J32X_DVI_H_ACTIVE - w) / 2;
        uint16_t y_off = (J32X_DVI_V_ACTIVE - h) / 2;

        for (uint y = 0; y < J32X_DVI_V_ACTIVE; y++) {
            uint32_t *tmdsbuf;
            queue_remove_blocking(&dvi0.q_tmds_free, &tmdsbuf);

            if (y < y_off || y >= y_off + h) {
                /* Letterbox region — black */
                tmds_encode_data_channel_8bpp(
                    (const uint32_t *)black_line, tmdsbuf,
                    J32X_DVI_H_ACTIVE / 2, DVI_8BPP_BLUE_MSB, DVI_8BPP_BLUE_LSB);
                tmds_encode_data_channel_8bpp(
                    (const uint32_t *)black_line, tmdsbuf + J32X_DVI_H_ACTIVE,
                    J32X_DVI_H_ACTIVE / 2, DVI_8BPP_GREEN_MSB, DVI_8BPP_GREEN_LSB);
                tmds_encode_data_channel_8bpp(
                    (const uint32_t *)black_line, tmdsbuf + 2 * J32X_DVI_H_ACTIVE,
                    J32X_DVI_H_ACTIVE / 2, DVI_8BPP_RED_MSB, DVI_8BPP_RED_LSB);
            } else {
                /* Active region — build a padded scanline */
                uint fb_y = y - y_off;
                static uint8_t line_buf[640];

                /* Left pillarbox */
                memset(line_buf, 0, x_off);
                /* Active pixels from framebuffer */
                memcpy(line_buf + x_off, &fb[fb_y * w], w);
                /* Right pillarbox */
                memset(line_buf + x_off + w, 0, J32X_DVI_H_ACTIVE - x_off - w);

                tmds_encode_data_channel_8bpp(
                    (const uint32_t *)line_buf, tmdsbuf,
                    J32X_DVI_H_ACTIVE / 2, DVI_8BPP_BLUE_MSB, DVI_8BPP_BLUE_LSB);
                tmds_encode_data_channel_8bpp(
                    (const uint32_t *)line_buf, tmdsbuf + J32X_DVI_H_ACTIVE,
                    J32X_DVI_H_ACTIVE / 2, DVI_8BPP_GREEN_MSB, DVI_8BPP_GREEN_LSB);
                tmds_encode_data_channel_8bpp(
                    (const uint32_t *)line_buf, tmdsbuf + 2 * J32X_DVI_H_ACTIVE,
                    J32X_DVI_H_ACTIVE / 2, DVI_8BPP_RED_MSB, DVI_8BPP_RED_LSB);
            }

            queue_add_blocking(&dvi0.q_tmds_valid, &tmdsbuf);
        }

        g_state.frame_count++;
    }
}
