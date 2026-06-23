/*
 * Jupiter SDK — Allwinner V3s SDMMC0 raw-block-read driver
 *
 * Card is initialised through the standard SD/SDHC spec sequence
 * (CMD0 → CMD8 → ACMD41 → CMD2 → CMD3 → CMD9 → CMD7 → ACMD6) at low
 * speed (~400 kHz), then the controller clock is bumped to 25 MHz for
 * data transfers. Reads use FIFO PIO — no DMA wired yet.
 *
 * Public API is intentionally tiny: init once, then read N 512-byte
 * blocks at a given LBA into a caller-provided buffer.
 */
#ifndef JUPITER_SDMMC_H
#define JUPITER_SDMMC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDMMC_BLOCK_SIZE   512u

/* Card capacity is in 512-byte blocks. Populated by sdmmc_init(). */
typedef struct {
    uint8_t  initialised;
    uint8_t  is_sdhc;       /* 1 = SDHC/SDXC (block addressed), 0 = SDSC (byte) */
    uint16_t rca;
    uint32_t num_blocks;    /* total user-area blocks */
    uint32_t cid[4];
    uint32_t csd[4];
} sdmmc_card_t;

/* Returns 0 on success, negative error code on failure.
 * Logs progress + final card identity to UART. */
int sdmmc_init(void);

/* Read `count` 512-byte blocks starting at logical block address `lba`
 * into `dst`. `dst` must be 4-byte aligned. Returns 0 on success,
 * negative error code on failure. */
int sdmmc_read_blocks(uint32_t lba, uint32_t count, void *dst);

/* Write `count` 512-byte blocks from `src` starting at LBA `lba`. `src`
 * must be 4-byte aligned. Returns 0 on success, negative on failure.
 * NOTE: there is no soft check against partition tables — caller is
 * responsible for choosing an LBA past any FAT region they want to
 * preserve. */
int sdmmc_write_blocks(uint32_t lba, uint32_t count, const void *src);

/* Read-only accessor for diagnostics. */
const sdmmc_card_t *sdmmc_card(void);

#ifdef __cplusplus
}
#endif

#endif
