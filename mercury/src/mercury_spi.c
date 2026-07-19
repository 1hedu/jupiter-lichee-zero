/*
 * Mercury — SPI slave receiver
 *
 * Receives display list command buffers from the V3s over SPI.
 * The V3s is SPI master, Pico is slave.
 *
 * Protocol:
 *   - V3s asserts CSn, clocks out display list bytes
 *   - First 2 bytes = little-endian payload length
 *   - Remaining bytes = display list commands
 *   - V3s deasserts CSn when done
 *
 * Non-blocking: returns -1 immediately if no CS assertion detected.
 * Uses hardware SPI1 in slave mode.
 */

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/structs/spi.h"
#include "jupiter32x.h"

#define SPI_PORT    spi1

void j32x_spi_init(void)
{
    /* Init SPI1 in slave mode */
    spi_init(SPI_PORT, 25 * 1000 * 1000);  /* 25MHz max */
    spi_set_slave(SPI_PORT, true);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PIN_SPI_RX,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_CSN, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_TX,  GPIO_FUNC_SPI);
}

/* Drop everything sitting in the RX FIFO. */
static void spi_drain_rx(void)
{
    while (spi_is_readable(SPI_PORT))
        (void)spi_get_hw(SPI_PORT)->dr;
}

/* Pull one byte, aborting if the master deasserts CS mid-transfer.
 * Returns -1 on abort. */
static int spi_get_byte_cs(void)
{
    for (;;) {
        if (spi_is_readable(SPI_PORT))
            return (int)(spi_get_hw(SPI_PORT)->dr & 0xFF);
        if (gpio_get(PIN_SPI_CSN)) {
            /* CS went high: catch any bytes that were already in
             * flight, else give up */
            if (spi_is_readable(SPI_PORT))
                return (int)(spi_get_hw(SPI_PORT)->dr & 0xFF);
            return -1;
        }
    }
}

/* Swallow the rest of a bad transfer: wait for CS deassert, drain. */
static void spi_abort_transfer(void)
{
    while (!gpio_get(PIN_SPI_CSN))
        spi_drain_rx();
    spi_drain_rx();
}

/*
 * Try to receive a display list from V3s.
 * Returns payload length on success, 0 if CS idle, -1 on a framed but
 * unusable transfer (bad length, truncation) — the transfer is fully
 * consumed either way, so the next call starts clean.
 *
 * Framing contract with the V3s master: assert CS, stream the whole
 * length+payload without long pauses, deassert CS. Core 0 must call
 * this often enough to keep the 8-deep RX FIFO from overflowing at
 * the chosen SCK rate — i.e. don't start a transfer while the Pico is
 * still rasterizing the previous frame (j32x_swap() returning on the
 * V3s side's previous SCENE_END is the natural pacing point).
 */
int j32x_spi_recv_displaylist(uint8_t *buf, uint32_t max_len)
{
    /* Idle: CS high. Drop any stale bytes from a torn transfer so the
     * next header read starts aligned. */
    if (gpio_get(PIN_SPI_CSN)) {
        spi_drain_rx();
        return 0;
    }

    /* CS asserted — read 2-byte length header (CS-abortable) */
    int b0 = spi_get_byte_cs();
    int b1 = b0 < 0 ? -1 : spi_get_byte_cs();
    if (b0 < 0 || b1 < 0) { spi_abort_transfer(); return -1; }
    uint16_t payload_len = (uint16_t)(b0 | (b1 << 8));

    if (payload_len == 0 || payload_len > max_len) {
        spi_abort_transfer();
        return -1;
    }

    for (uint32_t i = 0; i < payload_len; i++) {
        int b = spi_get_byte_cs();
        if (b < 0) { spi_abort_transfer(); return -1; }
        buf[i] = (uint8_t)b;
    }

    return (int)payload_len;
}
