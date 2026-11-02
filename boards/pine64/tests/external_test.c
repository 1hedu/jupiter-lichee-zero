/* Host contract checks for the actual A64 external drivers.
 * A GPIO-level Si5351 slave model ACKs writes, supplies reads and faults;
 * a Joybus waveform checks byte sampling and absent/stuck-line timeouts.
 * This is not electrical or physical peripheral validation.
 *
 * gcc -std=c11 -O2 -Wall -Wextra -Wno-int-to-pointer-cast \
 *   -DJUPITER_BOARD_PINE64 -DPINE64_HOST_TEST -Iinclude -Ilib \
 *   lib/si5351.c lib/ym3438_hw.c lib/n64_joybus.c \
 *   boards/pine64/tests/external_test.c -o /tmp/pine64-external-test
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "jupiter.h"
#include "pine64_external.h"
#include "n64_joybus.h"

#define R(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define PIO 0x01c20800u
#define PHCFG0 R(PIO + 0xfcu)
#define PHCFG1 R(PIO + 0x100u)
#define PHDAT R(PIO + 0x10cu)
#define PCDAT R(PIO + 0x58u)
static uint32_t ticks;
static int frozen_clock;
static int device_present = 1, scl_stuck, n64_stuck_low;
static int lose_lock, corrupt_readback;
static uint8_t chip_regs[256], reg_pointer;
static int old_scl = 1, old_sda = 1, slave_low;
static unsigned i2c_count, i2c_byte, i2c_state, i2c_next;
static unsigned i2c_writes, i2c_starts;
static int read_nack;
enum { RECEIVE_ADDRESS, RECEIVE_REGISTER, RECEIVE_DATA, TRANSMIT_DATA };
static int n64_receive_active;
static uint32_t n64_wave_start;
static uint8_t n64_reply;
static int previous_n64_output;
static uint32_t n64_low_start, n64_last_release;
static unsigned n64_pulses;
static uint8_t n64_transmitted;
static unsigned ym_writes;
static uint8_t ym_ports[64], ym_data[64];
static uint32_t ym_edge_tick;
static int previous_wr = 1;

static unsigned ph_function(unsigned pin)
{
    return ((pin < 8u ? PHCFG0 : PHCFG1) >> ((pin & 7u) * 4u)) & 15u;
}

static int master_low(unsigned pin)
{
    if (ph_function(pin) != 1u)
        return 0;
    assert(!(PHDAT & (1u << pin))); /* An open-drain pin never drives HIGH. */
    return 1;
}

static uint8_t slave_read(void)
{
    if (reg_pointer == 0u)
        return lose_lock ? 0x20u : chip_regs[0];
    if (corrupt_readback && reg_pointer == 42u)
        return chip_regs[42] ^ 1u;
    return chip_regs[reg_pointer];
}

static void i2c_received_byte(void)
{
    if (i2c_state == RECEIVE_ADDRESS) {
        if ((i2c_byte >> 1) != 0x60u)
            device_present = 0;
        i2c_next = (i2c_byte & 1u) ? TRANSMIT_DATA : RECEIVE_REGISTER;
    } else if (i2c_state == RECEIVE_REGISTER) {
        reg_pointer = (uint8_t)i2c_byte;
        i2c_next = RECEIVE_DATA;
    } else {
        if (device_present) {
            chip_regs[reg_pointer] = reg_pointer == 177u ? 0u : (uint8_t)i2c_byte;
            ++i2c_writes;
        }
        ++reg_pointer;
        i2c_next = RECEIVE_DATA;
    }
}

void pine64_external_gpio_changed(void)
{
    int scl = !master_low(2u) && !scl_stuck;
    int sda = !master_low(3u) && !slave_low;
    if (scl && old_scl && sda != old_sda) {
        /* START / STOP are data transitions while clock stays high. */
        if (!sda) {
            ++i2c_starts;
            i2c_state = RECEIVE_ADDRESS;
            i2c_count = 0;
            i2c_byte = 0;
        } else {
            slave_low = 0;
            i2c_count = 0;
            i2c_byte = 0;
        }
    }
    if (scl && !old_scl) {
        if (i2c_count == 8u && i2c_state == TRANSMIT_DATA)
            read_nack = sda;
        if (i2c_count < 8u && i2c_state != TRANSMIT_DATA) {
            i2c_byte = (i2c_byte << 1) | (unsigned)sda;
            if (i2c_count == 7u)
                i2c_received_byte();
        }
        ++i2c_count;
    } else if (!scl && old_scl) {
        if (i2c_count == 8u) {
            slave_low = i2c_state == TRANSMIT_DATA ? 0 : device_present;
        } else if (i2c_count == 9u) {
            i2c_count = 0;
            i2c_byte = 0;
            if (i2c_state == TRANSMIT_DATA) {
                if (read_nack)
                    i2c_state = RECEIVE_ADDRESS;
                else
                    ++reg_pointer;
            } else {
                i2c_state = i2c_next;
            }
            slave_low = i2c_state == TRANSMIT_DATA && !(slave_read() & 0x80u);
        } else if (i2c_state == TRANSMIT_DATA) {
            slave_low = !(slave_read() & (1u << (7u - i2c_count)));
        }
    }
    sda = !master_low(3u) && !slave_low;
    old_scl = scl;
    old_sda = sda;
    PHDAT = (PHDAT & ~((1u << 2) | (1u << 3))) |
            ((unsigned)scl << 2) | ((unsigned)sda << 3);

    int n64_output = master_low(9u);
    if (n64_output && !previous_n64_output)
        n64_low_start = ticks;
    if (!n64_output && previous_n64_output) {
        uint32_t duration = ticks - n64_low_start;
        assert(duration == 24u || duration == 72u);
        if (n64_pulses < 8u)
            n64_transmitted = (uint8_t)((n64_transmitted << 1) | (duration == 24u));
        n64_last_release = ticks;
        ++n64_pulses;
    }
    previous_n64_output = n64_output;
    int n64_low = n64_output || n64_stuck_low;
    if (n64_receive_active && ticks >= n64_wave_start) {
        unsigned bit = (ticks - n64_wave_start) / 96u;
        unsigned phase = (ticks - n64_wave_start) % 96u;
        if (bit < 8u) {
            unsigned low_ticks = (n64_reply & (1u << (7u - bit))) ? 24u : 72u;
            n64_low |= phase < low_ticks;
        }
    }
    PHDAT = (PHDAT & ~(1u << 9)) | (!n64_low ? 1u << 9 : 0u);

    int wr = !!(PCDAT & (1u << 10));
    int cs = !!(PCDAT & (1u << 11));
    if ((R(PIO + 0x4cu) & 0xfffffu) == 0x11111u && !cs) {
        if (!wr && previous_wr) {
            assert(ym_writes < sizeof(ym_ports));
            ym_edge_tick = ticks;
            ym_ports[ym_writes] = (PCDAT >> 8) & 3u;
            ym_data[ym_writes] = PCDAT & 0xffu;
        } else if (wr && !previous_wr) {
            assert(ticks - ym_edge_tick >= 12u); /* >= 500 ns strobe. */
            ++ym_writes;
        }
    }
    previous_wr = wr;
}

void timer_init(void) {}
uint32_t timer_read(void)
{
    if (!frozen_clock)
        ++ticks;
    pine64_external_gpio_changed();
    return 0u - ticks;
}
uint32_t timer_elapsed(uint32_t start, uint32_t end) { return start - end; }
void pine64_delay_ns(uint32_t ns)
{
    ticks += (uint32_t)(((uint64_t)ns * 24u + 999u) / 1000u);
    pine64_external_gpio_changed();
}
void pine64_delay_us(uint32_t us) { pine64_delay_ns(us * 1000u); }
void uart_puts(const char *s)
{
#ifdef PINE64_EXTERNAL_TRACE
    fputs(s, stderr);
#else
    (void)s;
#endif
}
void uart_putdec(uint32_t n) { (void)n; }
void uart_puthex(uint32_t n) { (void)n; }

static void test_clock(void)
{
    assert(pine64_si5351_start() == 0);
    assert(chip_regs[3] == 0xfeu && chip_regs[16] == 0x4fu);
    assert(i2c_writes > 30u && i2c_starts > 80u);
    assert(chip_regs[26] == 0xe8u && chip_regs[30] == 0x57u);
    assert(ph_function(2u) == 0u && ph_function(3u) == 0u);
    assert(ticks < 24000000u);
}
static void test_clock_missing(void)
{
    device_present = 0;
    uint32_t pc = PCDAT, config = R(PIO + 0x48u);
    ym3438_hw_set_clock(YM_CLK_SI5351);
    ym3438_hw_init();
    assert(pine64_ym3438_status() == PINE64_YM_CLOCK_FAILED);
    assert(pine64_ym3438_clock_hz() == 0u);
    ym3438_hw_vgm_write(0, 0x28u, 0xf0u);
    assert(ym_writes == 0u && PCDAT == pc && R(PIO + 0x48u) == config);
    assert(ticks < 24000u);
}
static void test_clock_stuck(void)
{
    scl_stuck = 1;
    assert(pine64_si5351_start() < 0);
    assert(ticks < 48000u);
    assert(ph_function(2u) == 0u && ph_function(3u) == 0u);
}
static void test_clock_frozen(void)
{
    frozen_clock = scl_stuck = 1;
    assert(pine64_si5351_start() < 0); /* Poll limit supplements timer. */
}
static void test_clock_bad_readback(void)
{
    corrupt_readback = 1;
    assert(pine64_si5351_start() < 0);
    assert(chip_regs[3] == 0xffu);
}
static void test_clock_unlocked(void)
{
    lose_lock = 1;
    assert(pine64_si5351_start() < 0);
    assert(chip_regs[3] == 0xffu && ticks < 12000000u);
}
static void test_ym_gpio(void)
{
    uint32_t preserve = PCDAT & ~0x1fffu;
    uint32_t cfg1 = R(PIO + 0x4cu) & ~0xfffffu;
    uint32_t pf = R(PIO + 0xb4u);
    uint8_t ccu_before[0x400];
    memcpy(ccu_before, (const void *)0x01c20000u, sizeof(ccu_before));
    ym3438_hw_set_clock(YM_CLK_EXT_XTAL);
    ym3438_hw_init();
    assert(pine64_ym3438_status() == PINE64_YM_WRITE_ONLY_READY);
    assert(pine64_ym3438_clock_hz() == 8000000u);
    ym3438_hw_write(0x22u, 0x08u);
    ym3438_hw_write_port1(0x30u, 0x71u);
    assert(ym_writes == 4u);
    assert(ym_ports[0] == 0u && ym_data[0] == 0x22u);
    assert(ym_ports[1] == 1u && ym_data[1] == 0x08u);
    assert(ym_ports[2] == 2u && ym_data[2] == 0x30u);
    assert(ym_ports[3] == 3u && ym_data[3] == 0x71u);
    ym3438_hw_vgm_write(0, 0xa4u, 0x22u);
    ym3438_hw_vgm_write(0, 0xa0u, 0x69u);
    uint32_t fnum = (0x269u * 62837u) >> 16;
    assert(ym_data[7] == (0x20u | (fnum >> 8)));
    assert(ym_data[9] == (uint8_t)fnum);
    /* CH3 special frequencies need their own port-0-only cache. Port 1
     * writes to the same numeric addresses must pass through unchanged
     * and must never replace the pending special-mode high byte. */
    for (unsigned slot = 0; slot < 3; ++slot) {
        uint8_t hi = 0x21u + slot, lo = 0x56u + slot;
        ym3438_hw_vgm_write(0, 0xacu + slot, hi);
        unsigned n = ym_writes;
        ym3438_hw_vgm_write(1, 0xacu + slot, 0x37u);
        assert(ym_writes == n + 2 && ym_ports[n] == 2 && ym_data[n] == 0xacu + slot);
        assert(ym_ports[n + 1] == 3 && ym_data[n + 1] == 0x37u);
        n = ym_writes;
        ym3438_hw_vgm_write(0, 0xa8u + slot, lo);
        fnum = (((uint32_t)(hi & 7u) * 256u + lo) * 62837u) >> 16;
        assert(ym_writes == n + 4 && ym_data[n] == 0xacu + slot);
        assert(ym_data[n + 1] == ((hi & 0xf8u) | (fnum >> 8)));
        assert(ym_data[n + 2] == 0xa8u + slot && ym_data[n + 3] == (uint8_t)fnum);
    }
    unsigned n = ym_writes;
    ym3438_hw_vgm_write(0, 0xa0u, 0x11u); /* normal cache still holds 0x22 */
    fnum = (0x211u * 62837u) >> 16;
    assert(ym_data[n + 1] == (0x20u | (fnum >> 8)));
    assert(ym_data[n + 3] == (uint8_t)fnum);
    n = ym_writes;
    ym3438_hw_vgm_write(1, 0xa8u, 0xffu);
    assert(ym_writes == n + 2 && ym_data[n] == 0xa8u && ym_data[n + 1] == 0xffu);
    ym3438_hw_init(); /* reinitialization clears both frequency caches */
    n = ym_writes;
    ym3438_hw_vgm_write(0, 0xa8u, 0xffu);
    fnum = (0xffu * 62837u) >> 16;
    assert(ym_data[n + 1] == 0 && ym_data[n + 3] == fnum);
    assert((PCDAT & ~0x1fffu) == preserve);
    assert((R(PIO + 0x4cu) & ~0xfffffu) == cfg1);
    assert(R(PIO + 0xb4u) == pf);
    assert(memcmp(ccu_before, (const void *)0x01c20000u, sizeof(ccu_before)) == 0);
}
static void test_n64_send_receive(void)
{
    uint32_t cfg0 = PHCFG0, cfg1 = PHCFG1 & ~(15u << 4);
    n64_pin_input();
    n64_send_byte(0xa5u);
    n64_send_stop();
    assert(n64_pulses == 9u && n64_transmitted == 0xa5u);
    assert(ph_function(9u) == 0u && ticks - n64_last_release == 48u);
    assert(PHCFG0 == cfg0 && (PHCFG1 & ~(15u << 4)) == cfg1);
    n64_receive_active = 1;
    n64_wave_start = ticks + 100u;
    n64_reply = 0xb6u;
    uint8_t reply = 0;
    assert(n64_recv_byte(&reply) == 0 && reply == 0xb6u);
    assert(n64_dbg_bits_got == 8);
}
static void test_n64_absent(void)
{
    n64_pin_input();
    uint8_t value = 0x55u;
    assert(n64_recv_byte(&value) < 0);
    assert(value == 0x55u && n64_dbg_bits_got == 0);
    assert(ticks < 40000u && ph_function(9u) == 0u);
}
static void test_n64_low(void)
{
    n64_stuck_low = 1;
    n64_pin_input();
    assert(n64_recv_bit() < 0);
    assert(ticks < 500u && ph_function(9u) == 0u);
}

int main(void)
{
    assert(mmap((void *)0x01c20000u, 0x10000u, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED);
    R(PIO + 0x4cu) = 0x76500000u;
    PCDAT = 0x015fa500u;
    R(PIO + 0xb4u) = 0x77767777u; /* Boot SD PF mux sentinel. */
    PHDAT = (1u << 2) | (1u << 3) | (1u << 9);
    void (*tests[])(void) = {test_clock, test_clock_missing, test_clock_stuck,
        test_clock_frozen, test_clock_bad_readback, test_clock_unlocked,
        test_ym_gpio, test_n64_send_receive, test_n64_absent, test_n64_low};
    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        pid_t child = fork();
        assert(child >= 0);
        if (!child) {
            tests[i]();
            _exit(0);
        }
        int status;
        assert(waitpid(child, &status, 0) == child);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            fprintf(stderr, "external contract test %u failed\n", i);
            return 1;
        }
    }
    puts("PINE64 external GPIO: Si5351 ACK/readback/lock/faults, YM masked writes, N64 byte timing/timeouts PASS");
    return 0;
}
