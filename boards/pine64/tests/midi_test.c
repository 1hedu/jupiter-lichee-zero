/* Host contract checks of the actual MIDI driver/parser, with only MMIO,
 * clock and interrupt delivery substituted. No UART electrical claims.
 * cc -std=c11 -O2 -Wall -Wextra -Werror -DJUPITER_BOARD_PINE64 \
 *   -Iinclude boards/pine64/tests/midi_test.c -o /tmp/pine64-midi-test
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static volatile uint32_t registers[0x9000 / 4];
static volatile uint32_t *test_reg(uintptr_t address)
{
    assert(address >= 0x01c20000u && address < 0x01c29000u && !(address & 3u));
    return &registers[(address - 0x01c20000u) / 4u];
}
#define REG32(address) (*test_reg(address))
#include "../../../lib/midi.c"

static uint32_t ticks, registered_irq, enabled_irq;
static void (*registered_handler)(void);
static int frozen, timeout_reports;
uint32_t timer_read(void) { if (!frozen) ticks += 24; return ticks; }
uint32_t timer_elapsed(uint32_t start, uint32_t end) { return end - start; }
uint32_t ticks_to_us(uint32_t value) { return value / 24; }
void pine64_delay_us(uint32_t us) { ticks += us * 24; }
void irq_register(uint32_t id, void (*handler)(void))
{ registered_irq = id; registered_handler = handler; }
void irq_enable(uint32_t id) { enabled_irq = id; }
void uart_puts(const char *text)
{ if (strstr(text, "transmit timeout")) ++timeout_reports; }

static uint8_t messages[32][3], sysex[32];
static unsigned message_count, sysex_len;
static void short_message(uint8_t status, uint8_t d1, uint8_t d2)
{
    assert(message_count < 32);
    messages[message_count][0] = status;
    messages[message_count][1] = d1;
    messages[message_count++][2] = d2;
}
static void sysex_message(const uint8_t *bytes, uint32_t length)
{
    assert(length <= sizeof(sysex));
    memcpy(sysex, bytes, length);
    sysex_len = length;
}
static void receive(const uint8_t *bytes, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) rx_push(bytes[i]);
    midi_pump();
}

int main(void)
{
    /* Refuse an unsupported inherited clock without touching GPIO. */
    REG32(PIO_BASE + 0x24) = 0xaabbccddu;
    REG32(CCU_BASE + 0x58) = 0x02000000u;
    midi_init();
    assert(!s_midi_inited && REG32(PIO_BASE + 0x24) == 0xaabbccddu);

    /* The known SD2 clock gives divisor 48; preserve unrelated pins. */
    REG32(CCU_BASE + 0x58) = 0x01000000u;
    REG32(PIO_BASE + 0x40) = 0xaabbccddu;
    uint32_t pull = REG32(PIO_BASE + 0x40);
    CCU_BUS_CLK_GATE3 = 0x90000000u;
    CCU_BUS_RST4 = 0x80000000u;
    MIDI_UART_REG(0x7c) = UART1_USR_TFNF;
    midi_init();
    assert(s_midi_inited && registered_irq == 34 && enabled_irq == 34);
    assert(registered_handler == midi_uart_isr);
    assert(REG32(PIO_BASE + 0x24) == 0xaabbcc22u);
    assert(REG32(PIO_BASE + 0x40) == ((pull & ~0xcu) | 4u));
    assert(CCU_BUS_CLK_GATE3 == 0x90040000u && CCU_BUS_RST4 == 0x80040000u);
    assert(MIDI_UART_REG(0x00) == 48 && MIDI_UART_REG(0x0c) == 3);
    assert(REG32(CCU_BASE + 0x58) == 0x01000000u);
    const uint8_t tx[] = {0x90, 60, 100};
    midi_send(tx, sizeof(tx));
    assert(MIDI_UART_REG(0x00) == 100);
    MIDI_UART_REG(0x7c) = 0;
    frozen = 1;
    midi_send(tx, sizeof(tx));
    assert(timeout_reports == 1); /* Poll cap works even with stopped time. */
    frozen = 0;

    /* Ring overflow drops new input and never advances the consumer tail. */
    for (unsigned pass = 0; pass < 3; ++pass) {
        for (unsigned i = 0; i < 1023; ++i) rx_push((uint8_t)i);
        uint32_t tail = s_rx_tail;
        rx_push(0xee);
        assert(s_rx_tail == tail && midi_recv_available() == 1023);
        for (unsigned i = 0; i < 1023; ++i) {
            uint8_t value;
            assert(midi_recv_byte(&value) && value == (uint8_t)i);
        }
        assert(midi_recv_available() == 0);
    }

    midi_short_set_handler(short_message);
    midi_sysex_set_handler(sysex_message);
    const uint8_t input[] = {
        0xc2,5,6, 0xd3,7,8, 0x91,60,0xf8,100,61,101,
        0xf1,3,4, 0x90,62,102, 0xf3,7,8, 0xf2,1,2,3,4, 0xf6,5,6
    };
    const uint8_t expected[][3] = {
        {0xc2,5,0},{0xc2,6,0},{0xd3,7,0},{0xd3,8,0},
        {0x91,60,100},{0x91,61,101},{0xf1,3,0},{0x90,62,102},
        {0xf3,7,0},{0xf2,1,2},{0xf6,0,0}
    };
    receive(input, sizeof(input));
    assert(message_count == sizeof(expected)/sizeof(expected[0]));
    assert(memcmp(messages, expected, sizeof(expected)) == 0);
    const uint8_t sx[] = {0xf0,0x41,0x10,0xf8,0x12,0xf7,9};
    const uint8_t expected_sx[] = {0xf0,0x41,0x10,0x12,0xf7};
    receive(sx, sizeof(sx));
    assert(sysex_len == sizeof(expected_sx));
    assert(memcmp(sysex, expected_sx, sizeof(expected_sx)) == 0);
    assert(message_count == sizeof(expected)/sizeof(expected[0]));

    /* A stuck RFNE status is contained to one FIFO-sized IRQ budget. */
    MIDI_UART_REG(0x7c) = UART1_USR_RFNE;
    MIDI_UART_REG(0x00) = 0x42;
    registered_handler();
    assert(midi_recv_available() == 64);
    puts("PASS MIDI: UART2 pinmux/divisor/preservation, frozen-clock timeout, bounded IRQ, ring overflow/wrap, running status, system common and SysEx");
    return 0;
}
