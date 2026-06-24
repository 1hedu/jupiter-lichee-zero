/*
 * Jupiter SDK — MIDI byte-stream layer over UART1
 *
 * Provides a MIDI-rate (31250 baud) bidirectional MIDI port on
 * PE21 (TX) / PE22 (RX). TX is blocking; RX is interrupt-driven
 * into a ring buffer that the main loop drains.
 *
 * Overlay 6 (Studio Station) wires PE21/22 to a 5-pin DIN-5 MIDI
 * pair via an opto-isolator on RX and a current-limiting resistor
 * on TX. See docs/MIDI_HW_GUIDE.md for the wiring.
 */
#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>

/* Bring up UART1 at 31250 baud, claim PE21/PE22, enable RX IRQ.
 * Call once at boot. Idempotent. */
void midi_init(void);

/* Blocking TX. Used to push SysEx dumps + regular MIDI messages out
 * the wire. Each byte waits for the TX FIFO to have room. */
void midi_send(const uint8_t *bytes, uint32_t len);

/* Non-blocking RX. Returns 1 and writes one byte to *out if a byte
 * is available in the ring buffer; returns 0 otherwise. Editors
 * should call this in a tight loop each frame to drain accumulated
 * SysEx bytes. */
int  midi_recv_byte(uint8_t *out);

/* Approximate byte count currently waiting in the RX ring buffer.
 * Useful for debug / instrumentation. */
uint32_t midi_recv_available(void);

/* ---------- SysEx assembler ----------
 *
 * Most editor traffic is SysEx (F0...F7). The assembler buffers
 * incoming bytes between matching F0/F7 framing bytes and invokes a
 * callback with each complete SysEx packet. Non-SysEx bytes (channel
 * voice messages, real-time clocks, etc.) get a separate path.
 *
 * Call midi_sysex_set_handler() once at boot. Then call midi_pump()
 * each frame — it drains the ring buffer through the assembler. */
typedef void (*midi_sysex_cb_t)(const uint8_t *bytes, uint32_t len);
typedef void (*midi_short_cb_t)(uint8_t status, uint8_t d1, uint8_t d2);

void midi_sysex_set_handler(midi_sysex_cb_t cb);
void midi_short_set_handler(midi_short_cb_t cb);
void midi_pump(void);

#endif /* MIDI_H */
