/*
 * Mercury — V3s SPI0 master (display-list link to the Pico)
 *
 * The Pico is an SPI slave (hardware SPI1, mode 0, MSB first) that
 * expects CS-framed transfers: assert CS, stream a 2-byte little-endian
 * payload length then the payload, deassert CS. This driver sends the
 * whole message as ONE hardware burst, so the controller's automatic
 * chip-select covers exactly that framing.
 *
 * Controller: sun6i-style SPI at 0x01C68000 (V3s datasheet / Linux
 * spi-sun6i.c register layout).
 *
 * Pins (Linux pinctrl-sun8i-v3s.c, function 3 = "spi0"):
 *   PC0 = SPI0_MISO   (unused — the Pico's TX is optional readback)
 *   PC1 = SPI0_CLK
 *   PC2 = SPI0_CS
 *   PC3 = SPI0_MOSI
 * These are the SPI-flash pads on the Lichee Pi Zero. Boards with the
 * optional NOR flash populated share the bus — keep the flash CS
 * (same PC2) in mind if yours has one soldered.
 *
 * Bus clock: 24 MHz HOSC / (2*(CDR2+1)) = 6 MHz. The RP2040 slave
 * tops out around sysclk/12 ≈ 11 MHz, so 6 MHz has comfortable margin;
 * a 2 KB display list takes ~2.7 ms — fine for per-frame streaming.
 *
 * Status: UNTESTED on silicon (no Pico link on the bench yet).
 */

#ifndef JUPITER_MERCURY_SPI_MASTER_H
#define JUPITER_MERCURY_SPI_MASTER_H

#include <stdint.h>

#define MSPI_BASE       0x01C68000
#define MSPI(reg)       (*(volatile uint32_t *)(MSPI_BASE + (reg)))
#define MSPI_GCR        0x04    /* global control */
#define MSPI_TCR        0x08    /* transfer control */
#define MSPI_ISR        0x14    /* interrupt status (w1c) */
#define MSPI_FCR        0x18    /* FIFO control */
#define MSPI_FSR        0x1C    /* FIFO status */
#define MSPI_CCR        0x24    /* clock control */
#define MSPI_MBC        0x30    /* total burst counter */
#define MSPI_MTC        0x34    /* transmit counter */
#define MSPI_BCC        0x38    /* burst control counter */
#define MSPI_TXD        0x200   /* TX FIFO (byte-writable) */

#define MSPI_GCR_EN         (1u << 0)
#define MSPI_GCR_MASTER     (1u << 1)
#define MSPI_GCR_TP_EN      (1u << 7)   /* stop txn when FIFO would over/underrun */
#define MSPI_GCR_SRST       (1u << 31)  /* soft reset, self-clearing */

#define MSPI_TCR_SPOL       (1u << 2)   /* CS active-low */
#define MSPI_TCR_DHB        (1u << 8)   /* discard RX during TX (we never read) */
#define MSPI_TCR_XCH        (1u << 31)  /* start exchange */

#define MSPI_ISR_TC         (1u << 12)  /* transfer complete */

#define MSPI_FCR_RF_RST     (1u << 15)
#define MSPI_FCR_TF_RST     (1u << 31)

#define MSPI_TXFIFO_DEPTH   64
#define MSPI_TX_CNT(fsr)    (((fsr) >> 16) & 0xFF)

static inline void mercury_spi_init(void)
{
    volatile uint32_t *ccu = (volatile uint32_t *)0x01C20000;
    volatile uint32_t *pio = (volatile uint32_t *)0x01C20800;

    /* CCU: gate + de-assert reset for SPI0 (bit 20 in both regs),
     * module clock = HOSC 24 MHz straight through. */
    ccu[0x060 / 4] |= (1u << 20);          /* BUS_CLK_GATING0: SPI0 */
    ccu[0x2C0 / 4] |= (1u << 20);          /* BUS_SOFT_RST0:  SPI0 */
    ccu[0x0A0 / 4]  = (1u << 31);          /* SPI0_CLK: gate on, src HOSC, /1 */

    /* Pinmux: PC0-PC3 -> function 3 (spi0). PC_CFG0 covers PC0-7. */
    uint32_t cfg = pio[0x48 / 4];
    cfg &= ~0x0000FFFFu;
    cfg |=  0x00003333u;
    pio[0x48 / 4] = cfg;

    /* Controller: soft reset, then master mode + transmit-pause. */
    MSPI(MSPI_GCR) = MSPI_GCR_SRST;
    for (volatile int i = 0; i < 10000; i++)
        if (!(MSPI(MSPI_GCR) & MSPI_GCR_SRST)) break;
    MSPI(MSPI_GCR) = MSPI_GCR_EN | MSPI_GCR_MASTER | MSPI_GCR_TP_EN;

    /* Mode 0 (CPOL=0, CPHA=0 — matches the Pico slave), CS active-low,
     * automatic chip-select (SS_OWNER=0), discard RX. */
    MSPI(MSPI_TCR) = MSPI_TCR_SPOL | MSPI_TCR_DHB;

    /* 24 MHz / (2*(1+1)) = 6 MHz, CDR2 mode (bit 12). */
    MSPI(MSPI_CCR) = (1u << 12) | 1u;
}

/* Send one raw burst (CS asserted for the whole length). Returns 0 on
 * success, -1 on timeout. Bounded waits throughout — a wedged
 * controller costs ~20 ms, never a hang. */
static inline int mercury_spi_send_raw(const uint8_t *a, uint32_t alen,
                                       const uint8_t *b, uint32_t blen)
{
    uint32_t total = alen + blen;

    MSPI(MSPI_FCR) = MSPI_FCR_TF_RST | MSPI_FCR_RF_RST;
    for (volatile int i = 0; i < 10000; i++)
        if (!(MSPI(MSPI_FCR) & (MSPI_FCR_TF_RST | MSPI_FCR_RF_RST))) break;
    MSPI(MSPI_ISR) = 0xFFFFFFFFu;          /* clear stale status (w1c) */

    MSPI(MSPI_MBC) = total;
    MSPI(MSPI_MTC) = total;
    MSPI(MSPI_BCC) = total;
    MSPI(MSPI_TCR) |= MSPI_TCR_XCH;

    /* Feed the byte-wide TX FIFO from both spans. At 6 MHz the bus
     * drains ~750 KB/s; the timeout covers a 4 KB payload many times
     * over. */
    volatile uint8_t *txd = (volatile uint8_t *)(MSPI_BASE + MSPI_TXD);
    uint32_t sent = 0, spins = 0;
    while (sent < total) {
        if (MSPI_TX_CNT(MSPI(MSPI_FSR)) < MSPI_TXFIFO_DEPTH) {
            uint8_t v = (sent < alen) ? a[sent] : b[sent - alen];
            *txd = v;
            sent++;
            spins = 0;
        } else if (++spins > 20000000u) {
            return -1;
        }
    }

    for (uint32_t w = 0; w < 20000000u; w++) {
        if (MSPI(MSPI_ISR) & MSPI_ISR_TC) {
            MSPI(MSPI_ISR) = 0xFFFFFFFFu;
            return 0;
        }
    }
    return -1;
}

/* Send a display list with the Pico's framing: u16 LE length + payload,
 * one CS-framed burst. */
static inline int mercury_spi_send_displaylist(const uint8_t *dl, uint32_t len)
{
    uint8_t hdr[2] = { (uint8_t)(len & 0xFF), (uint8_t)(len >> 8) };
    if (len == 0 || len > 0xFFFF) return -1;
    return mercury_spi_send_raw(hdr, 2, dl, len);
}

#endif
