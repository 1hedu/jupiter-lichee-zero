/*
 * Jupiter SDK — N64 joybus low-level primitives.
 *
 * Bit-bang implementation extracted from input.c so cpak.c can reuse the
 * same protocol layer. Pin: PE20, open-drain with external pull-up. The
 * bit cell is 4 us. A "1" is 1 us low + 3 us high; a "0" is 3 us low + 1
 * us high. The console always pulls low to start a cell; both sides
 * idle high.
 *
 * Timing comes from PMU cycle counter delays (delay_ns / delay_us /
 * pmu_cycles), set up by pmu_init() before the first joybus call.
 *
 * Concurrent use with input poll polling: not safe — they share the
 * PE20 pin. Sequence cpak ops between input_poll() calls (poll first,
 * then do the cpak op, then poll again — the polling re-asserts the
 * pin direction it needs).
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "n64_joybus.h"

#define PE_CFG2     REG32(PIO_BASE + 0x98)  /* PE16-PE23 */
#define PE_DAT      REG32(PIO_BASE + 0xA0)
#define N64_DATA    (1u << 20)              /* PE20 */

static inline void pe_clr(uint32_t mask)   { PE_DAT &= ~mask; }
static inline int  pe_read(uint32_t mask)  { return (PE_DAT & mask) ? 1 : 0; }

void n64_pin_output(void)
{
    /* PE20: bits [19:16] of PE_CFG2 = 0001 (output) */
    PE_CFG2 = (PE_CFG2 & ~(0xFu << 16)) | (0x1u << 16);
}

void n64_pin_input(void)
{
    /* PE20: bits [19:16] of PE_CFG2 = 0000 (input) */
    PE_CFG2 = (PE_CFG2 & ~(0xFu << 16));
}

void n64_send_bit(int bit)
{
    /* Set DAT bit BEFORE flipping pin to output — otherwise the pin
     * briefly drives whatever stale value happened to be in DAT[20]
     * when output mode is asserted. */
    pe_clr(N64_DATA);
    n64_pin_output();
    if (bit) {
        delay_ns(1000);   /* 1 µs LOW */
        n64_pin_input();
        delay_ns(3000);   /* 3 µs HIGH (pull-up) */
    } else {
        delay_ns(3000);   /* 3 µs LOW */
        n64_pin_input();
        delay_ns(1000);   /* 1 µs HIGH */
    }
}

void n64_send_stop(void)
{
    pe_clr(N64_DATA);
    n64_pin_output();
    delay_ns(1000);       /* 1 µs LOW (stop-bit pulse) */
    n64_pin_input();
    /* 2 µs idle — the controller starts its reply at ~2 µs after our
     * stop's rising edge. Anything longer here (e.g. 5 µs) makes us
     * begin polling AFTER the first reply bit has started, missing
     * the start edge and shifting every received byte left by 1 bit. */
    delay_us(2);
}

void n64_send_byte(uint8_t b)
{
    for (int i = 7; i >= 0; i--)
        n64_send_bit((b >> i) & 1);
}

int n64_recv_bit(void)
{
    /* Wrap-safe: delta-since-start, not absolute target. */
    uint32_t t_w = pmu_cycles();
    while (pe_read(N64_DATA)) {
        if ((pmu_cycles() - t_w) > 1800000) return -1;
    }

    delay_ns(2000);
    int val = pe_read(N64_DATA);

    uint32_t t_e = pmu_cycles();
    while (!pe_read(N64_DATA)) {
        if ((pmu_cycles() - t_e) > 12000) break;
    }

    return val;
}

/* Diagnostic: counts how many bits we successfully captured before the
 * most recent timeout. Reset by callers as needed. */
volatile int n64_dbg_bits_got = 0;
volatile int n64_dbg_first_to_us = 0;  /* µs from byte start until timeout */

int n64_recv_byte(uint8_t *out)
{
    uint8_t b = 0;
    n64_dbg_bits_got = 0;
    uint32_t t_byte_start = pmu_cycles();
    for (int i = 7; i >= 0; i--) {
        /* Wrap-safe timeout: subtract first, compare delta. The previous
         * `pmu_cycles() > start + N` form breaks on uint32_t wrap every
         * 3.6 s and could spin forever in that window. */
        uint32_t t_w = pmu_cycles();
        while (PE_DAT & N64_DATA) {
            if ((pmu_cycles() - t_w) > 1800000) {
                n64_dbg_first_to_us = (int)((pmu_cycles() - t_byte_start) / 1200);
                return -1;
            }
        }
        uint32_t t_edge = pmu_cycles();
        while ((pmu_cycles() - t_edge) < 2400) { }
        int v = (PE_DAT & N64_DATA) ? 1 : 0;
        b |= (uint8_t)(v << i);
        n64_dbg_bits_got++;
        uint32_t t_e = pmu_cycles();
        while (!(PE_DAT & N64_DATA)) {
            if ((pmu_cycles() - t_e) > 12000) break;
        }
    }
    *out = b;
    return 0;
}

/* Diagnostic: how many bytes were captured before the most recent
 * timeout in recv_buf. Reset on every recv_buf call. */
volatile int n64_dbg_recv_buf_bytes = 0;

int n64_recv_buf(uint8_t *out, int n)
{
    n64_dbg_recv_buf_bytes = 0;
    for (int i = 0; i < n; i++) {
        if (n64_recv_byte(&out[i]) < 0) return -1;
        n64_dbg_recv_buf_bytes = i + 1;
    }
    return 0;
}
