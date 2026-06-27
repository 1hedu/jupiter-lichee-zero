/*
 * Jupiter 32X — CSI Output Driver
 *
 * Runs on Core 1. Scans out the front framebuffer as parallel CSI (DVP)
 * that the V3s captures as a camera feed.
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "jupiter32x.h"
#include "csi_out.pio.h"

static PIO  csi_pio = pio0;
static uint pixel_sm;
static int  dma_chan;

void j32x_csi_start(void)
{
    uint offset = pio_add_program(csi_pio, &csi_pixel_program);
    pixel_sm = pio_claim_unused_sm(csi_pio, true);
    csi_pixel_program_init(csi_pio, pixel_sm, offset,
                           PIN_D0, PIN_PCLK, J32X_PIO_CLKDIV);

    gpio_init(PIN_HREF);
    gpio_init(PIN_VSYNC);
    gpio_set_dir(PIN_HREF, GPIO_OUT);
    gpio_set_dir(PIN_VSYNC, GPIO_OUT);
    gpio_put(PIN_HREF, 0);
    gpio_put(PIN_VSYNC, 0);

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(csi_pio, pixel_sm, true));

    dma_channel_configure(dma_chan, &cfg,
        &csi_pio->txf[pixel_sm], NULL, J32X_WIDTH, false);

    pio_sm_set_enabled(csi_pio, pixel_sm, true);
}

static inline void output_line(const uint8_t *data)
{
    dma_channel_set_read_addr(dma_chan, data, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
}

static inline void blank_us(uint us) { busy_wait_us(us); }

void j32x_csi_scanout(void)
{
    while (1) {
        const uint8_t *fb = g_state.fb[g_state.front];

        /* VSYNC pulse */
        gpio_put(PIN_VSYNC, 1);
        blank_us(90);
        gpio_put(PIN_VSYNC, 0);

        /* Vertical back porch */
        blank_us(300);

        /* Active lines */
        for (int y = 0; y < J32X_HEIGHT; y++) {
            blank_us(2);                        /* H back porch */
            gpio_put(PIN_HREF, 1);              /* active data */
            output_line(&fb[y * J32X_WIDTH]);
            gpio_put(PIN_HREF, 0);
            blank_us(2);                        /* H front porch */
        }

        /* Vertical front porch */
        blank_us(90);

        g_state.frame_count++;
    }
}
