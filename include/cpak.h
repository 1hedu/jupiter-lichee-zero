/*
 * Jupiter SDK — N64 Controller Pak (raw block layer)
 *
 * 32 KB battery-backed SRAM in the controller's expansion port. Spoken to
 * via joybus commands relayed by the controller. Independent of input.c
 * polling — but shares the PE20 line, so don't run a poll and a pak read
 * concurrently from interrupts.
 *
 * Block size is fixed at 32 bytes. There are 128 blocks (0..127),
 * giving 4096 bytes of total addressable space — but the on-card memory
 * is 32 KB; addresses 0x0000..0x7FFF are valid in 32-byte units. There
 * are 1024 blocks total. Block index in this API is the 0..1023 range.
 *
 * Detection: cpak_probe() distinguishes between
 *   - no controller (returns -1)
 *   - controller, no pak (returns 0)
 *   - controller + Controller Pak (returns 1)
 *   - controller + Rumble Pak (returns 2)
 *   - controller + Transfer Pak / unknown (returns 3)
 *
 * For most users, cpak_probe() == 1 means "you can read/write blocks."
 *
 * Call input_init(INPUT_N64) before any cpak_* function. Both layers
 * share the PE20 pin configuration.
 */
#ifndef JUPITER_CPAK_H
#define JUPITER_CPAK_H

#include <stdint.h>

#define CPAK_BLOCK_SIZE   32
#define CPAK_NUM_BLOCKS   1024              /* 32 KB / 32 B */
#define CPAK_TOTAL_BYTES  (CPAK_BLOCK_SIZE * CPAK_NUM_BLOCKS)

/* Probe results */
#define CPAK_NONE         0   /* controller present, no pak */
#define CPAK_CONTROLLER   1   /* Controller Pak (memory) */
#define CPAK_RUMBLE       2   /* Rumble Pak */
#define CPAK_TRANSFER     3   /* Transfer Pak / other */

/* Probe the expansion port. Returns one of the CPAK_* constants, or -1 if
 * no controller is connected. */
int cpak_probe(void);

/* Read one 32-byte block. Returns 0 on success, -1 on timeout, -2 on CRC. */
int cpak_read_block(uint16_t block, uint8_t out[CPAK_BLOCK_SIZE]);

/* Write one 32-byte block. Returns 0 on success, -1 on timeout, -2 on CRC. */
int cpak_write_block(uint16_t block, const uint8_t in[CPAK_BLOCK_SIZE]);

/* Convenience: read/write arbitrary byte offsets. Crosses block boundaries
 * by issuing multiple block ops and stitching. Slower than block-aligned
 * I/O but useful for the FS layer and ad-hoc dumps. */
int cpak_read(uint32_t offset, void *buf, uint32_t len);
int cpak_write(uint32_t offset, const void *buf, uint32_t len);

#endif
