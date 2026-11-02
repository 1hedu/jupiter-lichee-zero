/*
 * Jupiter SDK — N64 joybus low-level primitives (internal)
 *
 * These were originally inline-only inside input.c. They're lifted out so
 * cpak.c can reuse them without duplicating the bit-bang code.
 *
 * Pin: V3s PE20; PINE A64+ PH9 (Pi-2 header pin 13). Open-drain with an
 * external 3.3 V pull-up. PINE wiring is in boards/pine64/EXTERNAL.md.
 *
 * Bit cell = 4us. A "1" is 1us low + 3us high; a "0" is 3us low + 1us high.
 * The console always pulls low to start a cell; both sides idle high.
 *
 * NOT a public SDK header — include from input.c / cpak.c only.
 */
#ifndef N64_JOYBUS_H
#define N64_JOYBUS_H

#include <stdint.h>

/* Direction control */
void n64_pin_output(void);
void n64_pin_input(void);

/* Bit-level (used by byte-level wrappers; exposed for tight loops) */
void n64_send_bit(int bit);
int  n64_recv_bit(void);   /* returns 0/1, or -1 on timeout (no controller) */

/* Stop bit — always sent by the side ending a transmission */
void n64_send_stop(void);

/* Byte-level */
void n64_send_byte(uint8_t b);
int  n64_recv_byte(uint8_t *out);            /* 0 on success, -1 timeout */
int  n64_recv_buf(uint8_t *out, int n);      /* same; aborts on first timeout */

extern volatile int n64_dbg_bits_got;
extern volatile int n64_dbg_first_to_us;
extern volatile int n64_dbg_recv_buf_bytes;

/* Critical section helpers — joybus is software bit-banged with PMU
 * delays on V3s and the 24 MHz timer on A64. Any IRQ during a transaction
 * (audio mix tick, DMA completion, hstimer) drifts bit timing by tens
 * of microseconds and corrupts the line. Wrap every joybus transaction
 * (send + recv as one unit) in n64_joybus_lock / n64_joybus_unlock so
 * timing is jitter-free. The longest cpak op is ~1.5 ms; audio mix_buf
 * has ~180 ms of depth so the deferred ISR work absorbs cleanly. */
static inline uint32_t n64_joybus_lock(void)
{
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    __asm__ volatile("cpsid i" ::: "memory");
    return cpsr;
}

static inline void n64_joybus_unlock(uint32_t saved_cpsr)
{
    /* Restore previous I-bit state — only re-enable if it was enabled
     * before we masked. CPSR bit 7 = I (1 = IRQs masked). */
    if (!(saved_cpsr & (1u << 7))) {
        __asm__ volatile("cpsie i" ::: "memory");
    }
}

#endif
