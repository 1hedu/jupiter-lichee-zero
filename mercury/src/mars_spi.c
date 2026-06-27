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

/*
 * Try to receive a display list from V3s.
 * Non-blocking: returns 0 immediately if CS is not asserted.
 * Returns payload length on success, 0 if no data, -1 on error.
 */
int j32x_spi_recv_displaylist(uint8_t *buf, uint32_t max_len)
{
    /* Non-blocking check: is CS asserted (low)? */
    if (gpio_get(PIN_SPI_CSN))
        return 0;

    /* CS is asserted — read 2-byte length header */
    uint8_t hdr[2];
    spi_read_blocking(SPI_PORT, 0x00, hdr, 2);
    uint16_t payload_len = hdr[0] | (hdr[1] << 8);

    if (payload_len == 0 || payload_len > max_len)
        return -1;

    /* Read payload */
    spi_read_blocking(SPI_PORT, 0x00, buf, payload_len);

    return (int)payload_len;
}
