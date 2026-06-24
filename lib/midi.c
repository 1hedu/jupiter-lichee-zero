/*
 * Jupiter SDK — MIDI byte-stream layer over UART1
 *
 * Wire format: 31250 baud 8N1 (the MIDI standard since 1983).
 * UART clock source on V3s = OSC24M (24 MHz). Divisor = 24M / (16 × 31250)
 * = 48. So DLL = 48, DLH = 0 → exact 31250 baud, no error.
 *
 * Pinmux:
 *   PE21 = mux 4 → UART1_TX
 *   PE22 = mux 4 → UART1_RX
 *
 * RX path is interrupt-driven via IRQ 33 (V3s SPI #1). The ISR reads
 * every byte sitting in the UART1 RX FIFO and stuffs them into a
 * 1024-byte ring buffer. Main-loop code drains via midi_recv_byte()
 * or via midi_pump() which routes through the SysEx assembler.
 *
 * The SysEx assembler accumulates bytes between F0 and F7 into a
 * 1024-byte packet buffer, then calls the registered handler with the
 * completed packet. Channel-voice / system real-time messages bypass
 * the SysEx buffer and call the short-message handler directly.
 */
#include "jupiter.h"
#include "midi.h"

/* ================================================================
 *  RX ring buffer
 * ================================================================ */
#define RX_RING_SIZE 1024
static volatile uint8_t  s_rx_ring[RX_RING_SIZE];
static volatile uint32_t s_rx_head = 0;          /* write index (ISR)   */
static volatile uint32_t s_rx_tail = 0;          /* read  index (main)  */

static void rx_push(uint8_t b)
{
    uint32_t next = (s_rx_head + 1) & (RX_RING_SIZE - 1);
    if (next == s_rx_tail) {
        /* Ring full — drop oldest by advancing tail. */
        s_rx_tail = (s_rx_tail + 1) & (RX_RING_SIZE - 1);
    }
    s_rx_ring[s_rx_head] = b;
    s_rx_head = next;
}

int midi_recv_byte(uint8_t *out)
{
    if (s_rx_head == s_rx_tail) return 0;
    *out = s_rx_ring[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) & (RX_RING_SIZE - 1);
    return 1;
}

uint32_t midi_recv_available(void)
{
    return (s_rx_head - s_rx_tail) & (RX_RING_SIZE - 1);
}

/* ================================================================
 *  ISR — drain RX FIFO into ring
 * ================================================================ */
static void midi_uart1_isr(void)
{
    while (UART1_USR & UART1_USR_RFNE) {
        uint8_t b = (uint8_t)(UART1_RBR & 0xFF);
        rx_push(b);
    }
}

/* ================================================================
 *  TX (blocking)
 * ================================================================ */
void midi_send(const uint8_t *bytes, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        while (!(UART1_USR & UART1_USR_TFNF))
            ;
        UART1_THR = bytes[i];
    }
}

/* ================================================================
 *  Init — clocks, pinmux, baud, FIFO, IRQ
 * ================================================================ */
#define PE_CFG2_REG         REG32(PIO_BASE + 0x98)

static int s_midi_inited = 0;

void midi_init(void)
{
    if (s_midi_inited) return;
    s_midi_inited = 1;

    /* 1. Gate clock + de-assert reset for UART1 */
    CCU_BUS_CLK_GATE3 |= GATE3_UART1;
    CCU_BUS_RST4      |= RST4_UART1;

    /* 2. Pinmux: PE21 = mux 4 (UART1_TX), PE22 = mux 4 (UART1_RX).
     *    PE_CFG2 covers PE16..PE23; PE21 = bits [23:20], PE22 = [27:24]. */
    uint32_t cfg = PE_CFG2_REG;
    cfg &= ~((0xFu << 20) | (0xFu << 24));
    cfg |=  ((0x4u << 20) | (0x4u << 24));
    PE_CFG2_REG = cfg;

    /* 3. Set baud rate 31250: divisor = 24,000,000 / (16 * 31250) = 48 */
    UART1_LCR = UART1_LCR_DLAB;          /* unlock divisor latches */
    UART1_DLL = 48;
    UART1_DLH = 0;
    UART1_LCR = UART1_LCR_8N1;           /* 8N1, lock divisor */

    /* 4. Enable + reset both FIFOs, set RX trigger to 1/4 full */
    UART1_FCR = UART1_FCR_FIFOE | UART1_FCR_RFIFOR |
                UART1_FCR_XFIFOR | UART1_FCR_RT_QTR;

    /* 5. Enable RX data-available IRQ; register + enable in GIC */
    UART1_IER = UART1_IER_ERBFI;
    irq_register(IRQ_UART1, midi_uart1_isr);
    irq_enable(IRQ_UART1);

    uart_puts("[midi] UART1 init: PE21/PE22 @ 31250 baud, IRQ 33 enabled\n");
}

/* ================================================================
 *  SysEx assembler + dispatch
 * ================================================================ */
#define SYSEX_BUF_SIZE 1024
static uint8_t  s_sx_buf[SYSEX_BUF_SIZE];
static uint32_t s_sx_len = 0;
static int      s_sx_in_progress = 0;
static uint8_t  s_running_status = 0;

/* For channel-voice messages: count of remaining data bytes for the
 * current status byte. -1 = no current message. */
static int      s_short_remain = 0;
static uint8_t  s_short_d1     = 0;

static midi_sysex_cb_t s_sysex_cb = 0;
static midi_short_cb_t s_short_cb = 0;

void midi_sysex_set_handler(midi_sysex_cb_t cb) { s_sysex_cb = cb; }
void midi_short_set_handler(midi_short_cb_t cb) { s_short_cb = cb; }

/* MIDI status byte type tables. Number of data bytes per status nibble. */
static int data_bytes_for_status(uint8_t s)
{
    uint8_t hi = s & 0xF0;
    switch (hi) {
    case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0: return 2;
    case 0xC0: case 0xD0: return 1;
    case 0xF0:
        /* System common — vary; we handle 0xF1/0xF3 = 1 data, 0xF2 = 2 */
        if (s == 0xF1 || s == 0xF3) return 1;
        if (s == 0xF2) return 2;
        return 0;
    }
    return 0;
}

void midi_pump(void)
{
    uint8_t b;
    while (midi_recv_byte(&b)) {
        /* System real-time bytes (0xF8..0xFF) interleave anywhere and
         * never break running status. We ignore them for editor use. */
        if (b >= 0xF8) continue;

        /* SysEx framing */
        if (b == 0xF0) {
            s_sx_in_progress = 1;
            s_sx_len = 0;
            s_sx_buf[s_sx_len++] = b;
            s_running_status = 0;
            continue;
        }
        if (b == 0xF7) {
            if (s_sx_in_progress) {
                if (s_sx_len < SYSEX_BUF_SIZE)
                    s_sx_buf[s_sx_len++] = b;
                if (s_sysex_cb) s_sysex_cb(s_sx_buf, s_sx_len);
                s_sx_in_progress = 0;
                s_sx_len = 0;
            }
            continue;
        }
        if (s_sx_in_progress) {
            if (s_sx_len < SYSEX_BUF_SIZE)
                s_sx_buf[s_sx_len++] = b;
            else
                s_sx_in_progress = 0;   /* overflow — drop */
            continue;
        }

        /* Status byte (top bit set)? Start a new short message. */
        if (b & 0x80) {
            s_running_status = b;
            s_short_remain = data_bytes_for_status(b);
            s_short_d1 = 0;
            if (s_short_remain == 0 && s_short_cb)
                s_short_cb(b, 0, 0);
            continue;
        }

        /* Data byte for short message (with running status) */
        if (s_running_status == 0) continue;   /* orphan data, drop */
        if (s_short_remain == 2) {
            s_short_d1 = b;
            s_short_remain = 1;
        } else if (s_short_remain == 1) {
            if (s_short_cb) s_short_cb(s_running_status, s_short_d1, b);
            s_short_remain = data_bytes_for_status(s_running_status);
            s_short_d1 = 0;
        }
    }
}
