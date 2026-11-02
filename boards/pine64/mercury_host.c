/* SPDX-License-Identifier: GPL-2.0-or-later
 * PINE A64 host for the EXISTING Mercury Pico firmware, branch 4fb5904e.
 * Allwinner SPI0, 8 bits / mode 3 / MSB first, transmit only. Manual CS
 * spans a u16-LE length and its complete display list, including pauses.
 *
 * A64 pin mux/CCU: U-Boot spi-sunxi.c, sun50i_a64_pinctrl_functions,
 * clk_a64.c; board: PINE A64 Pin Assignment 160119.pdf, PI-2 header.
 * PI-2 19 PC0 MOSI -> SpotPear Pico GP4 RX; 23 PC2 SCK -> GP6;
 * 24 PC3 CS -> GP5; ground -> ground. Other existing Mercury presets
 * retain their firmware's SPI pins (see docs/WIRING_GUIDE.md).
 */
#include "jupiter.h"
#include "peripherals.h"
#include "pine64_mercury.h"
#include <string.h>

#define SPI_BASE 0x01c68000u
#define SPI_GCR  (SPI_BASE + 0x04u)
#define SPI_TCR  (SPI_BASE + 0x08u)
#define SPI_IER  (SPI_BASE + 0x10u)
#define SPI_ISR  (SPI_BASE + 0x14u)
#define SPI_FCR  (SPI_BASE + 0x18u)
#define SPI_FSR  (SPI_BASE + 0x1cu)
#define SPI_CCR  (SPI_BASE + 0x24u)
#define SPI_MBC  (SPI_BASE + 0x30u)
#define SPI_MTC  (SPI_BASE + 0x34u)
#define SPI_BCC  (SPI_BASE + 0x38u)
#define SPI_TXD  (SPI_BASE + 0x200u)
#define SPI_GCR_ENABLE (BIT(0) | BIT(1) | BIT(7))
#define SPI_GCR_RESET BIT(31)
/* CPOL=CPHA=1 (mode 3, bits 1/0), active-low CS, software-owned CS,
 * discard RX during TX. Must match the Pico slave (mercury_spi.c):
 * in mode 0 its PL022 accepts one byte per CS assertion, so every
 * whole-list transfer was dropped. */
#define SPI_TCR_IDLE (BIT(0) | BIT(1) | BIT(2) | BIT(6) | BIT(7) | BIT(8))
#define SPI_XCH BIT(31)
#define SPI_FIFO_RESET (BIT(31) | BIT(15))
#define SPI_FIFO_ERROR (BIT(8) | BIT(9) | BIT(10) | BIT(11))
#define CCU_SPI_GATE 0x01c20060u
#define CCU_SPI_CLK  0x01c200a0u
#define CCU_SPI_RST  0x01c202c0u
#define PC_CFG0 0x01c20848u
#define PC_DATA 0x01c20858u
#define PC_DRIVE 0x01c2085cu
#define PC_PULL 0x01c20864u
#define PC_CS BIT(3)
#define PC_CFG_MASK 0x0000ff0fu
#define PC_PAD_MASK 0x000000f3u

#ifdef PINE64_MERCURY_TEST
extern uint32_t mercury_test_read32(uintptr_t address);
extern void mercury_test_write32(uintptr_t address, uint32_t value);
extern void mercury_test_write8(uintptr_t address, uint8_t value);
#define read32 mercury_test_read32
#define write32 mercury_test_write32
#define write8 mercury_test_write8
static void barrier(void) {}
#else
static inline uint32_t read32(uintptr_t address) { return REG32(address); }
static inline void write32(uintptr_t address, uint32_t value) { REG32(address) = value; }
static inline void write8(uintptr_t address, uint8_t value) { *(volatile uint8_t *)address = value; }
static inline void barrier(void) { __asm__ volatile("dsb sy" ::: "memory"); }
#endif

static pine64_mercury_stats_t stats;
static uint32_t saved_cfg, saved_pull, saved_drive;
static int initialized, pins_claimed;

static void update_bits(uintptr_t address, uint32_t mask, uint32_t value)
{
    write32(address, (read32(address) & ~mask) | (value & mask));
    barrier();
}
static void cs_inactive(void) { update_bits(PC_DATA, PC_CS, PC_CS); }
static void cs_active(void) { update_bits(PC_DATA, PC_CS, 0); }
static int expired(uint32_t start, uint32_t us)
{
    return timer_elapsed(start, timer_read()) >= us * 24u;
}
static int delay_bounded(uint32_t us)
{
    uint32_t start = timer_read();
    unsigned bound = 20000000u;
    while (!expired(start, us))
        if (!--bound) return 0;
    return 1;
}
static int record_error(int error)
{
    stats.last_error = error;
    stats.last_spi_control = read32(SPI_TCR);
    stats.last_spi_fifo = read32(SPI_FSR);
    stats.last_spi_status = read32(SPI_ISR);
    if (error == PINE64_MERCURY_ERR_RESET || error == PINE64_MERCURY_ERR_SPI_TIMEOUT)
        ++stats.timeouts;
    if (error == PINE64_MERCURY_ERR_FIFO)
        ++stats.fifo_errors;
    return error;
}
static int wait_mask(uintptr_t address, uint32_t mask, uint32_t value, uint32_t us)
{
    uint32_t start = timer_read();
    unsigned bound = 1000000u;
    do {
        barrier();
        if ((read32(address) & mask) == value) return 1;
    } while (--bound && !expired(start, us));
    return 0;
}
static int configure_spi(void)
{
    cs_inactive();
    write32(SPI_GCR, SPI_GCR_ENABLE | SPI_GCR_RESET);
    if (!wait_mask(SPI_GCR, SPI_GCR_RESET, 0, 10000u))
        return record_error(PINE64_MERCURY_ERR_RESET);
    write32(SPI_GCR, SPI_GCR_ENABLE);
    write32(SPI_TCR, SPI_TCR_IDLE);
    write32(SPI_IER, 0);
    write32(SPI_ISR, UINT32_MAX);
    write32(SPI_CCR, BIT(12) | 11u); /* HOSC / (2*(11+1)) = 1 MHz. */
    write32(SPI_FCR, SPI_FIFO_RESET);
    if (!wait_mask(SPI_FCR, SPI_FIFO_RESET, 0, 10000u))
        return record_error(PINE64_MERCURY_ERR_RESET);
    return 0;
}

int pine64_mercury_init(void)
{
    timer_init();
    if (!pine64_timer_ready()) return PINE64_MERCURY_ERR_TIMER;
    if (!pins_claimed) {
        saved_cfg = read32(PC_CFG0);
        saved_pull = read32(PC_PULL);
        saved_drive = read32(PC_DRIVE);
        memset(&stats, 0, sizeof(stats));
        pins_claimed = 1;
    }
    initialized = 0;
    stats.spi_hz = PINE64_MERCURY_SPI_HZ;
    cs_inactive(); /* Set CS latch before switching its pad to output. */
    update_bits(PC_CFG0, PC_CFG_MASK, 0x00001404u); /* PC0/2 mux4, PC3 GPIO. */
    update_bits(PC_PULL, PC_PAD_MASK, 1u << 6); /* CS up; MOSI/SCK none. */
    update_bits(PC_DRIVE, PC_PAD_MASK, 0x51u);
    update_bits(CCU_SPI_GATE, BIT(20), BIT(20));
    update_bits(CCU_SPI_RST, BIT(20), 0);
    write32(CCU_SPI_CLK, BIT(31)); /* HOSC, no PLL or shared clock changes. */
    update_bits(CCU_SPI_RST, BIT(20), BIT(20));
    int rc = configure_spi();
    if (rc) return rc;
    initialized = 1;
    stats.last_error = 0;
    uart_puts("[mercury] firmware link: SPI0 1 MHz mode3, u16-LE length\n");
    uart_puts("[mercury] PC0 MOSI / PC2 SCK / PC3 CS; no ACK or framebuffer readback\n");
    return 0;
}

void pine64_mercury_deinit(void)
{
    if (!pins_claimed) return;
    cs_inactive();
    write32(SPI_IER, 0);
    write32(SPI_GCR, 0);
    update_bits(CCU_SPI_RST, BIT(20), 0);
    update_bits(CCU_SPI_GATE, BIT(20), 0);
    write32(CCU_SPI_CLK, 0);
    update_bits(PC_CFG0, PC_CFG_MASK, saved_cfg);
    update_bits(PC_PULL, PC_PAD_MASK, saved_pull);
    update_bits(PC_DRIVE, PC_PAD_MASK, saved_drive);
    initialized = 0;
    pins_claimed = 0;
}

static uint16_t u16le(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
int pine64_mercury_validate_displaylist(const uint8_t *list, size_t length)
{
    if (!list || !length || length > PINE64_MERCURY_MAX_LIST)
        return PINE64_MERCURY_ERR_ARGUMENT;
    for (size_t pos = 0; pos < length;) {
        size_t bytes;
        switch (list[pos]) {
        case 0x00: bytes = 1; break;
        case 0x01: bytes = 2; break;
        case 0x02: bytes = 14; break;
        case 0x04: bytes = 18; break;
        case 0x05: bytes = 10; break;
        case 0x11:
            if (length - pos < 4 || list[pos + 1] >= 8 || u16le(list + pos + 2) > 4096)
                return PINE64_MERCURY_ERR_DISPLAY_LIST;
            bytes = 4u + u16le(list + pos + 2);
            break;
        case 0x20:
            if (length - pos < 5) return PINE64_MERCURY_ERR_DISPLAY_LIST;
            {
                uint16_t w = u16le(list + pos + 1), h = u16le(list + pos + 3);
                if (w < 32 || w > PINE64_MERCURY_WIDTH || (w & 3u) ||
                    !h || h > PINE64_MERCURY_HEIGHT)
                    return PINE64_MERCURY_ERR_DISPLAY_LIST;
            }
            bytes = 5;
            break;
        case 0xff:
            return pos + 1 == length ? 0 : PINE64_MERCURY_ERR_DISPLAY_LIST;
        default: return PINE64_MERCURY_ERR_DISPLAY_LIST;
        }
        if (bytes > length - pos) return PINE64_MERCURY_ERR_DISPLAY_LIST;
        pos += bytes;
    }
    return 0;
}

int pine64_mercury_send_displaylist(const uint8_t *list, size_t length)
{
    if (!initialized) return PINE64_MERCURY_ERR_NOT_READY;
    int result = pine64_mercury_validate_displaylist(list, length);
    if (result) return record_error(result);
    pine64_service();
    write32(SPI_ISR, UINT32_MAX);
    write32(SPI_FCR, SPI_FIFO_RESET);
    if (!wait_mask(SPI_FCR, SPI_FIFO_RESET, 0, 10000u))
        return record_error(PINE64_MERCURY_ERR_RESET);
    cs_active();
    /* The existing receiver polls CS on core 0; its autonomous cube may
     * currently be rendering or waiting for scanout. Keep CS asserted
     * before the first clock for one 60 Hz frame plus margin. This is
     * conservative demo pacing, not a substitute for a remote ACK. */
    if (!delay_bounded(PINE64_MERCURY_SELECT_US)) {
        result = PINE64_MERCURY_ERR_TIMER;
        goto failed;
    }
    size_t total = length + 2;
    for (size_t pos = 0; pos < total;) {
        uint32_t count = total - pos > 63u ? 63u : (uint32_t)(total - pos);
        write32(SPI_MBC, count);
        write32(SPI_MTC, count);
        write32(SPI_BCC, count);
        for (uint32_t i = 0; i < count; ++i) {
            size_t offset = pos + i;
            uint8_t byte = offset == 0 ? (uint8_t)length :
                           offset == 1 ? (uint8_t)(length >> 8) : list[offset - 2];
            write8(SPI_TXD, byte);
        }
        barrier();
        write32(SPI_TCR, SPI_TCR_IDLE | SPI_XCH);
        if (!wait_mask(SPI_TCR, SPI_XCH, 0, 10000u)) {
            result = PINE64_MERCURY_ERR_SPI_TIMEOUT;
            goto failed;
        }
        if ((read32(SPI_ISR) & SPI_FIFO_ERROR) || (read32(SPI_FSR) & 0x00ff00ffu)) {
            result = PINE64_MERCURY_ERR_FIFO;
            goto failed;
        }
        pos += count;
    }
    barrier();
    cs_inactive();
    ++stats.lists_sent;
    stats.bytes_sent += (uint32_t)total;
    stats.last_error = 0;
    pine64_service();
    return 0;
failed:
    record_error(result);
    cs_inactive(); /* Truncated transfers release the Pico's polling loop. */
    (void)configure_spi();
    stats.last_error = result;
    return result;
}

const pine64_mercury_stats_t *pine64_mercury_stats(void) { return &stats; }
const char *pine64_mercury_error_string(int error)
{
    switch (error) {
    case 0: return "LOCAL SPI TRANSFER COMPLETE";
    case PINE64_MERCURY_ERR_ARGUMENT: return "INVALID ARGUMENT";
    case PINE64_MERCURY_ERR_TIMER: return "24 MHZ TIMER UNAVAILABLE";
    case PINE64_MERCURY_ERR_RESET: return "SPI RESET TIMEOUT";
    case PINE64_MERCURY_ERR_SPI_TIMEOUT: return "SPI EXCHANGE TIMEOUT";
    case PINE64_MERCURY_ERR_FIFO: return "SPI FIFO ERROR";
    case PINE64_MERCURY_ERR_DISPLAY_LIST: return "INVALID DISPLAY LIST";
    case PINE64_MERCURY_ERR_NOT_READY: return "SPI HOST NOT INITIALIZED";
    case PINE64_MERCURY_ERR_I2C: return "BRIDGE I2C DID NOT COMPLETE";
    case PINE64_MERCURY_ERR_BRIDGE_ID: return "UNEXPECTED BRIDGE CHIP ID";
    default: return "UNKNOWN ERROR";
    }
}
void pine64_mercury_dump_diagnostics(void)
{
    uart_puts("[mercury] "); uart_puts(pine64_mercury_error_string(stats.last_error));
    uart_puts("; local lists sent="); uart_putdec(stats.lists_sent);
    uart_puts(" bytes="); uart_putdec(stats.bytes_sent);
    uart_puts(" timeouts="); uart_putdec(stats.timeouts);
    uart_puts(" fifo errors="); uart_putdec(stats.fifo_errors);
    uart_puts("\n[mercury] TCR="); uart_puthex(stats.last_spi_control);
    uart_puts(" FSR="); uart_puthex(stats.last_spi_fifo);
    uart_puts(" ISR="); uart_puthex(stats.last_spi_status);
    uart_puts("\n");
}

/* TC358743 read-only register access. A64's I2C1 pads are PH2/SCL and
 * PH3/SDA. GPIO open-drain here avoids assumptions about the inherited
 * APB2 divider and leaves the hardware TWI block / its clock untouched.
 * Register addresses are big-endian; register values little-endian,
 * matching mercury/v3s_side/tc358743_init.h and Linux tc358743.c. */
#define PH_CFG0 0x01c208fcu
#define PH_DATA 0x01c2090cu
#define PH_PULL 0x01c20918u
#define BR_SCL BIT(2)
#define BR_SDA BIT(3)
static void br_low(uint32_t pin)
{
    unsigned shift = pin == BR_SCL ? 8u : 12u;
    update_bits(PH_DATA, pin, 0);
    update_bits(PH_CFG0, 15u << shift, 1u << shift);
}
static void br_release(uint32_t pin)
{
    unsigned shift = pin == BR_SCL ? 8u : 12u;
    update_bits(PH_CFG0, 15u << shift, 0);
}
static int br_clock_high(void)
{
    br_release(BR_SCL);
    return wait_mask(PH_DATA, BR_SCL, BR_SCL, 1000u) && delay_bounded(5u);
}
static int br_start(void)
{
    br_release(BR_SDA);
    if (!delay_bounded(5u) || !br_clock_high() || !(read32(PH_DATA) & BR_SDA)) return 0;
    br_low(BR_SDA);
    if (!delay_bounded(5u)) return 0;
    br_low(BR_SCL);
    return 1;
}
static void br_stop(void)
{
    br_low(BR_SCL);
    br_low(BR_SDA);
    (void)delay_bounded(5u);
    (void)br_clock_high();
    br_release(BR_SDA);
    (void)delay_bounded(5u);
}
static int br_write(uint8_t byte)
{
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (byte & 0x80u) br_release(BR_SDA); else br_low(BR_SDA);
        if (!delay_bounded(5u) || !br_clock_high()) return 0;
        br_low(BR_SCL);
        byte = (uint8_t)(byte << 1);
    }
    br_release(BR_SDA);
    if (!delay_bounded(5u) || !br_clock_high()) return 0;
    int ack = !(read32(PH_DATA) & BR_SDA);
    br_low(BR_SCL);
    return ack;
}
static int br_read(uint8_t *value, int ack)
{
    uint8_t byte = 0;
    br_release(BR_SDA);
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (!delay_bounded(5u) || !br_clock_high()) return 0;
        byte = (uint8_t)((byte << 1) | !!(read32(PH_DATA) & BR_SDA));
        br_low(BR_SCL);
    }
    if (ack) br_low(BR_SDA);
    if (!delay_bounded(5u) || !br_clock_high()) return 0;
    br_low(BR_SCL);
    br_release(BR_SDA);
    *value = byte;
    return 1;
}
static int br_register_read(uint8_t address, uint16_t reg, uint8_t *bytes, unsigned count)
{
    int ok = br_start() && br_write((uint8_t)(address << 1)) &&
             br_write((uint8_t)(reg >> 8)) && br_write((uint8_t)reg) &&
             br_start() && br_write((uint8_t)((address << 1) | 1u));
    for (unsigned i = 0; ok && i < count; ++i)
        ok = br_read(bytes + i, i + 1 < count);
    br_stop();
    return ok;
}
int pine64_mercury_probe_bridge(pine64_mercury_bridge_t *bridge)
{
    if (!bridge) return PINE64_MERCURY_ERR_ARGUMENT;
    memset(bridge, 0, sizeof(*bridge));
    timer_init();
    if (!pine64_timer_ready()) {
        bridge->error = PINE64_MERCURY_ERR_TIMER;
        return bridge->error;
    }
    pine64_service();
    uint32_t cfg = read32(PH_CFG0), pull = read32(PH_PULL), data = read32(PH_DATA);
    br_release(BR_SDA);
    br_release(BR_SCL);
    update_bits(PH_PULL, 0xf0u, 0x50u); /* internal pulls supplement board pulls */
    (void)delay_bounded(10u);
    bridge->idle_lines = (uint8_t)(((read32(PH_DATA) & BR_SDA) ? 1u : 0u) |
                                 ((read32(PH_DATA) & BR_SCL) ? 2u : 0u));
    bridge->error = PINE64_MERCURY_ERR_I2C;
    static const uint8_t addresses[2] = {0x0f, 0x1f};
    if (bridge->idle_lines != 3u) goto out;
    for (unsigned i = 0; i < 2; ++i) {
        uint8_t id[2];
        if (!br_register_read(addresses[i], 0x0000, id, 2)) continue;
        bridge->address = addresses[i];
        bridge->chip_id = u16le(id);
        if (id[1] != 0) {
            bridge->error = PINE64_MERCURY_ERR_BRIDGE_ID;
            continue;
        }
        uint8_t status, width[2], height[2];
        if (br_register_read(addresses[i], 0x8520, &status, 1) &&
            br_register_read(addresses[i], 0x8582, width, 2) &&
            br_register_read(addresses[i], 0x8588, height, 2)) {
            bridge->status = status;
            bridge->width = u16le(width) & 0x1fffu;
            bridge->height = u16le(height) & 0x1fffu;
            bridge->error = 0;
            break;
        }
        bridge->error = PINE64_MERCURY_ERR_I2C;
    }
out:
    br_release(BR_SDA);
    br_release(BR_SCL);
    update_bits(PH_DATA, BR_SDA | BR_SCL, data);
    update_bits(PH_PULL, 0xf0u, pull);
    update_bits(PH_CFG0, 0xff00u, cfg);
    return bridge->error;
}
