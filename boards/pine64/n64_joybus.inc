/* PINE A64+ N64 Joybus: PH9, Pi-2 header pin 13.
 * External pull-up goes to 3.3 V, never 5 V. The line is open-drain:
 * only LOW is driven; HIGH is the input state. See EXTERNAL.md.
 * Timing and every timeout use the fixed 24 MHz crystal timer.
 */
#include "jupiter.h"
#include "pmu.h"
#include "n64_joybus.h"
#include "external_test.h"

#define N64_PH_CFG1 REG32(0x01c20900u)
#define N64_PH_DAT  REG32(0x01c2090cu)
#define N64_PH_PULL REG32(0x01c20918u)
#define N64_PH_BIT  (1u << 9)

static int n64_pin_initialized;
volatile int n64_dbg_bits_got;
volatile int n64_dbg_first_to_us;
volatile int n64_dbg_recv_buf_bytes;

void n64_pin_input(void)
{
    N64_PH_CFG1 &= ~(15u << 4); /* PH9 input, preserve PH8/PH10..PH15. */
    if (!n64_pin_initialized) {
        timer_init();
        N64_PH_DAT &= ~N64_PH_BIT;
        N64_PH_PULL = (N64_PH_PULL & ~(3u << 18)) | (1u << 18);
        n64_pin_initialized = 1;
    }
    PINE_EXTERNAL_GPIO_CHANGED();
}

void n64_pin_output(void)
{
    /* Do not depend on input-bank readback representing an output latch. */
    N64_PH_DAT &= ~N64_PH_BIT;
    N64_PH_CFG1 = (N64_PH_CFG1 & ~(15u << 4)) | (1u << 4);
    PINE_EXTERNAL_GPIO_CHANGED();
}

static int n64_line_high(void) { return !!(N64_PH_DAT & N64_PH_BIT); }

void n64_send_bit(int bit)
{
    n64_pin_output();
    delay_ns(bit ? 1000u : 3000u);
    n64_pin_input();
    delay_ns(bit ? 3000u : 1000u);
}

void n64_send_stop(void)
{
    n64_pin_output();
    delay_ns(1000u);
    n64_pin_input();
    delay_us(2u);
}

void n64_send_byte(uint8_t byte)
{
    for (int bit = 7; bit >= 0; --bit)
        n64_send_bit((byte >> bit) & 1u);
}

static int n64_wait_level(int high, uint32_t ticks)
{
    uint32_t start = timer_read();
    unsigned polls = 100000u;
    while (n64_line_high() != high) {
        if (timer_elapsed(start, timer_read()) >= ticks || !--polls)
            return -1;
    }
    return 0;
}

int n64_recv_bit(void)
{
    /* Ignore the tail of our stop bit (or the previous low phase) before
     * looking for a new falling edge. This is the upstream phantom-A fix,
     * timed by A64's crystal rather than an assumed 1.2 GHz PMU. */
    if (n64_wait_level(1, 10u * 24u)) {
        n64_pin_input();
        return -1;
    }
    if (n64_wait_level(0, 1500u * 24u)) {
        n64_pin_input();
        return -1;
    }
    uint32_t edge = timer_read();
    unsigned polls = 10000u;
    while (timer_elapsed(edge, timer_read()) < 2u * 24u) {
        if (!--polls)
            return -1;
    }
    int value = n64_line_high();
    if (n64_wait_level(1, 10u * 24u)) {
        n64_pin_input();
        return -1; /* A stuck-low line is a failed packet, not a zero bit. */
    }
    return value;
}

int n64_recv_byte(uint8_t *out)
{
    uint8_t byte = 0;
    n64_dbg_bits_got = 0;
    n64_dbg_first_to_us = 0;
    uint32_t start = timer_read();
    if (!out)
        return -1;
    for (int bit = 7; bit >= 0; --bit) {
        int value = n64_recv_bit();
        if (value < 0) {
            n64_dbg_first_to_us = (int)(timer_elapsed(start, timer_read()) / 24u);
            return -1;
        }
        byte |= (uint8_t)((unsigned)value << bit);
        ++n64_dbg_bits_got;
    }
    *out = byte;
    return 0;
}

int n64_recv_buf(uint8_t *out, int bytes)
{
    n64_dbg_recv_buf_bytes = 0;
    if (!out || bytes < 0 || bytes > 256)
        return -1;
    for (int i = 0; i < bytes; ++i) {
        if (n64_recv_byte(out + i))
            return -1;
        n64_dbg_recv_buf_bytes = i + 1;
    }
    return 0;
}
