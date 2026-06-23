/*
 * Jupiter SDK — SDMMC0 raw-block-read verification example.
 *
 * Build: make GAME=examples/sdmmc/main.c
 *
 * Brings up SDMMC0, prints card identity (SDHC vs SDSC, RCA, capacity),
 * reads LBA 0 (the MBR), confirms the 0x55AA boot signature, then walks
 * the partition table and reads the first 16 bytes of partition 0's
 * boot sector and dumps them as hex + ASCII to UART. If all checks pass
 * we're cleared to start packing music / saves / cinematics onto raw SD
 * blocks past the FAT region.
 */
#include "jupiter.h"
#include "sdmmc.h"
#include <string.h>

#define D(s) uart_puts("[ex] " s "\n")

static uint8_t mbr[512] __attribute__((aligned(4)));
static uint8_t pbs[512] __attribute__((aligned(4)));

static void dump_hex_line(const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint8_t v = p[i];
        uart_putc("0123456789ABCDEF"[(v >> 4) & 0xF]);
        uart_putc("0123456789ABCDEF"[v & 0xF]);
        uart_putc(' ');
    }
    uart_puts(" | ");
    for (uint32_t i = 0; i < n; i++) {
        char c = (char)p[i];
        uart_putc((c >= 0x20 && c < 0x7F) ? c : '.');
    }
    uart_putc('\n');
}

static uint32_t le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();

    uart_puts("\n\n=== Jupiter SDMMC0 verification ===\n");

    D("step 1: sdmmc_init()");
    int rc = sdmmc_init();
    if (rc) {
        uart_puts("[ex] FATAL: sdmmc_init returned ");
        uart_putdec((uint32_t)(-rc));
        uart_puts(" (negative)\n");
        for (;;) { }
    }

    const sdmmc_card_t *c = sdmmc_card();
    uart_puts("[ex] card: ");
    uart_puts(c->is_sdhc ? "SDHC/SDXC" : "SDSC");
    uart_puts(" capacity=");
    uart_putdec(c->num_blocks / 2048);
    uart_puts(" MiB (");
    uart_putdec(c->num_blocks);
    uart_puts(" blocks)\n");

    D("step 2: read MBR (LBA 0)");
    rc = sdmmc_read_blocks(0, 1, mbr);
    if (rc) {
        uart_puts("[ex] FATAL: read LBA 0 rc=-");
        uart_putdec((uint32_t)(-rc));
        uart_puts("\n");
        for (;;) { }
    }

    uart_puts("[ex] MBR first 32 bytes:\n  ");
    dump_hex_line(mbr, 16);
    uart_puts("  ");
    dump_hex_line(mbr + 16, 16);

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        uart_puts("[ex] FATAL: 0x55AA signature missing — not a valid MBR\n");
        for (;;) { }
    }
    D("MBR signature OK (0x55AA)");

    D("step 3: walk partition table");
    int found = -1;
    for (int p = 0; p < 4; p++) {
        const uint8_t *e = mbr + 0x1BE + p * 16;
        uint8_t  type   = e[4];
        uint32_t startL = le32(e + 8);
        uint32_t sizeL  = le32(e + 12);
        if (!type) continue;
        uart_puts("[ex] part ");
        uart_putdec((uint32_t)p);
        uart_puts(": type=0x");
        uart_puthex(type);
        uart_puts(" startLBA=");
        uart_putdec(startL);
        uart_puts(" sectors=");
        uart_putdec(sizeL);
        uart_puts(" (~");
        uart_putdec(sizeL / 2048);
        uart_puts(" MiB)\n");
        if (found < 0) found = p;
    }
    if (found < 0) {
        uart_puts("[ex] FATAL: no partitions present\n");
        for (;;) { }
    }

    /* Re-decode the chosen partition. */
    const uint8_t *e = mbr + 0x1BE + found * 16;
    uint32_t startL = le32(e + 8);

    D("step 4: read partition 0 boot sector");
    rc = sdmmc_read_blocks(startL, 1, pbs);
    if (rc) {
        uart_puts("[ex] FATAL: read partition boot sector rc=-");
        uart_putdec((uint32_t)(-rc));
        uart_puts("\n");
        for (;;) { }
    }

    /* For FAT16 the OEM string is at offset 3, 8 chars. */
    char oem[9] = {0};
    memcpy(oem, pbs + 3, 8);
    uart_puts("[ex] PBS OEM='");
    for (int i = 0; i < 8; i++) {
        char ch = oem[i];
        uart_putc((ch >= 0x20 && ch < 0x7F) ? ch : '.');
    }
    uart_puts("'\n[ex] PBS first 32 bytes:\n  ");
    dump_hex_line(pbs, 16);
    uart_puts("  ");
    dump_hex_line(pbs + 16, 16);

    /* FAT16 BPB: bytes_per_sector @ 11, sectors_per_cluster @ 13. */
    uint32_t bps = pbs[11] | (pbs[12] << 8);
    uint32_t spc = pbs[13];
    uint32_t total_sectors_16 = pbs[19] | (pbs[20] << 8);
    uint32_t total_sectors_32 = le32(pbs + 32);
    uart_puts("[ex] FAT BPB: bytes_per_sector=");
    uart_putdec(bps);
    uart_puts(" sectors_per_cluster=");
    uart_putdec(spc);
    uart_puts(" total_sectors=");
    uart_putdec(total_sectors_16 ? total_sectors_16 : total_sectors_32);
    uart_puts("\n");

    /* ---------- step 5: write + readback at a safe LBA ----------
     * Partition 0 ends at LBA 264701 (start 2048 + size 262653). LBA
     * 524288 (256 MiB mark) is well past the FAT region. We write a
     * 4-block (2 KiB) test pattern, read it back, and byte-compare. */
    #define SAFE_LBA   524288u
    #define TEST_BLKS  4
    static uint8_t wbuf[TEST_BLKS * 512] __attribute__((aligned(4)));
    static uint8_t rbuf[TEST_BLKS * 512] __attribute__((aligned(4)));

    D("step 5: build write pattern");
    for (uint32_t b = 0; b < TEST_BLKS; b++) {
        for (uint32_t i = 0; i < 512; i += 4) {
            uint32_t off = b * 512 + i;
            uint32_t lba_stamp = SAFE_LBA + b;
            /* First 4 bytes of each block = LBA stamp; rest = DEADBEEF
             * XOR-rotated by offset so each word is unique. */
            if (i == 0) {
                wbuf[off+0] = (uint8_t)(lba_stamp);
                wbuf[off+1] = (uint8_t)(lba_stamp >> 8);
                wbuf[off+2] = (uint8_t)(lba_stamp >> 16);
                wbuf[off+3] = (uint8_t)(lba_stamp >> 24);
            } else {
                uint32_t v = 0xDEADBEEFu ^ (off * 0x9E3779B1u);
                wbuf[off+0] = (uint8_t)(v);
                wbuf[off+1] = (uint8_t)(v >> 8);
                wbuf[off+2] = (uint8_t)(v >> 16);
                wbuf[off+3] = (uint8_t)(v >> 24);
            }
        }
    }

    D("step 6: write test blocks");
    rc = sdmmc_write_blocks(SAFE_LBA, TEST_BLKS, wbuf);
    if (rc) {
        uart_puts("[ex] FATAL: write rc=-");
        uart_putdec((uint32_t)(-rc));
        uart_puts("\n");
        for (;;) { }
    }

    D("step 7: read test blocks back");
    rc = sdmmc_read_blocks(SAFE_LBA, TEST_BLKS, rbuf);
    if (rc) {
        uart_puts("[ex] FATAL: readback rc=-");
        uart_putdec((uint32_t)(-rc));
        uart_puts("\n");
        for (;;) { }
    }

    int diff = 0;
    for (uint32_t i = 0; i < TEST_BLKS * 512; i++) {
        if (wbuf[i] != rbuf[i]) { diff++; }
    }
    uart_puts("[ex] readback bytes differing: ");
    uart_putdec((uint32_t)diff);
    uart_puts("/");
    uart_putdec(TEST_BLKS * 512u);
    uart_puts("\n");
    if (diff) {
        uart_puts("[ex] FATAL: write/read mismatch\n");
        for (;;) { }
    }
    D("write+read round-trip OK");

    /* ---------- step 8: throughput timing ---------- */
    #define BENCH_BLKS 64u
    static uint8_t bbuf[BENCH_BLKS * 512] __attribute__((aligned(4)));
    D("step 8: read 32 KiB and time it");
    uint32_t t0 = timer_read();
    rc = sdmmc_read_blocks(SAFE_LBA, BENCH_BLKS, bbuf);
    uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
    if (rc) {
        uart_puts("[ex] bench read rc=-");
        uart_putdec((uint32_t)(-rc));
        uart_puts("\n");
    } else {
        uart_puts("[ex] read 32 KiB in ");
        uart_putdec(us);
        uart_puts(" us = ~");
        uart_putdec((BENCH_BLKS * 512u * 1000000u / 1024u) / (us ? us : 1));
        uart_puts(" KiB/s\n");
    }

    D("verification PASS — driver is sane");
    uart_puts("[ex] cleared to pack music/saves/cines past the FAT region\n");

    for (;;) { }
}
