/*
 * Jupiter SDK — WC1 save game persistence verifier.
 *
 * Proves we can round-trip a WC1-shaped 16 KiB save game through both
 * storage backends:
 *   - SD raw blocks at a fixed LBA past the music pack
 *   - N64 Controller Pak (cpak) blocks 0..511
 *
 * Each boot:
 *   1. Reads the existing save from each backend, decodes the header,
 *      reports the boot counter from the previous run.
 *   2. Generates a new save (counter = previous + 1; if no valid save
 *      was found in either backend, starts at 1).
 *   3. Writes the new save to both backends.
 *   4. Reads it back from both backends, byte-compares + CRC32-verifies.
 *
 * Power-cycle the device repeatedly: counters should monotonically
 * increment per backend. That proves persistence on real silicon.
 *
 * Save layout (16384 bytes):
 *   offset 0    : wc1_save_hdr (32 B)
 *   offset 32   : tile_state[4096]      (64x64 byte map; mimics WC1 map state)
 *   offset 4128 : unit_state[12256]     (192 units * ~64 B; mimics WC1 unit table)
 *
 * Build: make GAME=examples/wc1_save/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "sdmmc.h"
#include "cpak.h"
#include <string.h>
#include <stddef.h>

#define D(s) uart_puts("[ex] " s "\n")

#define SAVE_BYTES        16384u
#define SAVE_HDR_BYTES    32u
#define SAVE_PAYLOAD_BYTES (SAVE_BYTES - SAVE_HDR_BYTES)

/* SD: pick an LBA that's past the music pack (which lives at 524288..797695)
 * and far from any FAT region. Round to a tidy round number. */
#define SAVE_SD_LBA       800000u
#define SAVE_SD_SECTORS   (SAVE_BYTES / 512u)

/* Cpak: blocks 0..511 = first half of the 32 KB pak. */
#define SAVE_CPAK_BLOCK   0u
#define SAVE_CPAK_NBLOCKS (SAVE_BYTES / CPAK_BLOCK_SIZE)

#define WC1_SAVE_MAGIC    "WC1SAVE\0"   /* 8 chars including the trailing NUL */
#define WC1_SAVE_VERSION  1u

typedef struct {
    char     magic[8];        /* "WC1SAVE\0" */
    uint32_t version;
    uint32_t payload_size;    /* bytes following the header */
    uint32_t boot_counter;
    uint32_t payload_crc32;   /* CRC32 of [hdr_end .. hdr_end+payload_size) */
    uint8_t  reserved[8];
} wc1_save_hdr_t;
_Static_assert(sizeof(wc1_save_hdr_t) == SAVE_HDR_BYTES, "save header must be 32 bytes");

/* Static buffers. SAVE_BYTES is small enough to live in BSS. */
static uint8_t save_buf[SAVE_BYTES] __attribute__((aligned(4)));
static uint8_t read_buf[SAVE_BYTES] __attribute__((aligned(4)));

/* CRC32, poly 0xEDB88320, init/final ~0. */
static uint32_t crc32(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/* Generate a deterministic WC1-shaped save with the given boot counter.
 * Tile state and unit state get pseudo-random fills seeded by the counter
 * so different boots produce different content (catches "you read back
 * stale data and didn't notice" bugs). */
static void build_save(uint32_t boot_counter)
{
    memset(save_buf, 0, SAVE_BYTES);
    wc1_save_hdr_t *h = (wc1_save_hdr_t *)save_buf;
    memcpy(h->magic, WC1_SAVE_MAGIC, 8);
    h->version       = WC1_SAVE_VERSION;
    h->payload_size  = SAVE_PAYLOAD_BYTES;
    h->boot_counter  = boot_counter;

    /* Pseudo-random payload, LCG seeded by boot_counter. */
    uint32_t r = 0x9E3779B1u ^ (boot_counter * 0x85EBCA6Bu);
    uint8_t *payload = save_buf + SAVE_HDR_BYTES;
    for (uint32_t i = 0; i < SAVE_PAYLOAD_BYTES; i++) {
        r = r * 1664525u + 1013904223u;
        payload[i] = (uint8_t)(r >> 24);
    }
    /* First 64 bytes of payload: a "tile state header" with row IDs so
     * a hex dump reads sensibly (catches misaligned reads). */
    for (uint32_t y = 0; y < 8; y++) {
        for (uint32_t x = 0; x < 8; x++) {
            payload[y * 8 + x] = (uint8_t)((y << 4) | x);
        }
    }
    h->payload_crc32 = crc32(payload, SAVE_PAYLOAD_BYTES);
}

static int save_hdr_valid(const uint8_t *buf, wc1_save_hdr_t *out)
{
    const wc1_save_hdr_t *h = (const wc1_save_hdr_t *)buf;
    if (memcmp(h->magic, WC1_SAVE_MAGIC, 8) != 0) return 0;
    if (h->version != WC1_SAVE_VERSION) return 0;
    if (h->payload_size != SAVE_PAYLOAD_BYTES) return 0;
    uint32_t actual = crc32(buf + SAVE_HDR_BYTES, h->payload_size);
    if (actual != h->payload_crc32) return 0;
    if (out) *out = *h;
    return 1;
}

/* ---- SD backend round-trip ---- */
static int sd_save(void)
{
    int rc = sdmmc_write_blocks(SAVE_SD_LBA, SAVE_SD_SECTORS, save_buf);
    if (rc) {
        uart_puts("[sd] write rc=-"); uart_putdec((uint32_t)(-rc)); uart_puts("\n");
        return -1;
    }
    return 0;
}

static int sd_load_and_verify(uint32_t expected_counter)
{
    memset(read_buf, 0xCC, SAVE_BYTES);  /* poison so missing reads are visible */
    int rc = sdmmc_read_blocks(SAVE_SD_LBA, SAVE_SD_SECTORS, read_buf);
    if (rc) {
        uart_puts("[sd] read rc=-"); uart_putdec((uint32_t)(-rc)); uart_puts("\n");
        return -1;
    }
    if (memcmp(save_buf, read_buf, SAVE_BYTES) != 0) {
        uart_puts("[sd] readback mismatch\n");
        /* Find first differing byte. */
        for (uint32_t i = 0; i < SAVE_BYTES; i++) {
            if (save_buf[i] != read_buf[i]) {
                uart_puts("[sd]   first diff at byte "); uart_putdec(i);
                uart_puts(": wrote 0x"); uart_puthex(save_buf[i]);
                uart_puts(" read 0x"); uart_puthex(read_buf[i]); uart_puts("\n");
                break;
            }
        }
        return -1;
    }
    wc1_save_hdr_t h;
    if (!save_hdr_valid(read_buf, &h)) {
        uart_puts("[sd] header invalid after readback\n");
        return -1;
    }
    if (h.boot_counter != expected_counter) {
        uart_puts("[sd] counter mismatch: wrote "); uart_putdec(expected_counter);
        uart_puts(" read "); uart_putdec(h.boot_counter); uart_puts("\n");
        return -1;
    }
    return 0;
}

/* ---- Cpak backend round-trip ---- */
static int cpak_save_payload(void)
{
    /* cpak_write does multi-block stitching. */
    int rc = cpak_write(SAVE_CPAK_BLOCK * CPAK_BLOCK_SIZE, save_buf, SAVE_BYTES);
    if (rc) {
        uart_puts("[cpak] write rc=-"); uart_putdec((uint32_t)(-rc)); uart_puts("\n");
        return -1;
    }
    return 0;
}

static int cpak_load_and_verify(uint32_t expected_counter)
{
    memset(read_buf, 0xCC, SAVE_BYTES);
    int rc = cpak_read(SAVE_CPAK_BLOCK * CPAK_BLOCK_SIZE, read_buf, SAVE_BYTES);
    if (rc) {
        uart_puts("[cpak] read rc=-"); uart_putdec((uint32_t)(-rc)); uart_puts("\n");
        return -1;
    }
    if (memcmp(save_buf, read_buf, SAVE_BYTES) != 0) {
        uart_puts("[cpak] readback mismatch\n");
        for (uint32_t i = 0; i < SAVE_BYTES; i++) {
            if (save_buf[i] != read_buf[i]) {
                uart_puts("[cpak]   first diff at byte "); uart_putdec(i);
                uart_puts(": wrote 0x"); uart_puthex(save_buf[i]);
                uart_puts(" read 0x"); uart_puthex(read_buf[i]); uart_puts("\n");
                break;
            }
        }
        return -1;
    }
    wc1_save_hdr_t h;
    if (!save_hdr_valid(read_buf, &h)) {
        uart_puts("[cpak] header invalid after readback\n");
        return -1;
    }
    if (h.boot_counter != expected_counter) {
        uart_puts("[cpak] counter mismatch: wrote "); uart_putdec(expected_counter);
        uart_puts(" read "); uart_putdec(h.boot_counter); uart_puts("\n");
        return -1;
    }
    return 0;
}

/* ---- Existing-save sniffers (read whatever's there from the previous boot) ---- */
static uint32_t sd_read_existing_counter(void)
{
    memset(read_buf, 0xCC, SAVE_BYTES);
    if (sdmmc_read_blocks(SAVE_SD_LBA, SAVE_SD_SECTORS, read_buf) != 0) return 0;
    wc1_save_hdr_t h;
    if (!save_hdr_valid(read_buf, &h)) {
        uart_puts("[sd] no valid prior save\n");
        return 0;
    }
    uart_puts("[sd] prior save: counter="); uart_putdec(h.boot_counter);
    uart_puts(" payload_size="); uart_putdec(h.payload_size);
    uart_puts(" crc=0x"); uart_puthex(h.payload_crc32); uart_puts("\n");
    return h.boot_counter;
}

static uint32_t cpak_read_existing_counter(void)
{
    memset(read_buf, 0xCC, SAVE_BYTES);
    extern volatile int n64_dbg_recv_buf_bytes;
    extern volatile int n64_dbg_bits_got;
    extern volatile int n64_dbg_first_to_us;
    /* Try just block 0 first so we can dump diagnostics on the very
     * first post-probe read. If THAT fails we know the post-probe path
     * is broken without spamming 512 block-read attempts. */
    uint8_t blk0[CPAK_BLOCK_SIZE];
    int b0 = cpak_read_block(0, blk0);
    uart_puts("[cpak] block0 read rc="); uart_putdec((uint32_t)(-b0));
    uart_puts(" bytes_in_buf="); uart_putdec((uint32_t)n64_dbg_recv_buf_bytes);
    uart_puts(" bits="); uart_putdec((uint32_t)n64_dbg_bits_got);
    uart_puts(" to_us="); uart_putdec((uint32_t)n64_dbg_first_to_us);
    uart_puts("\n");

    int rc = cpak_read(SAVE_CPAK_BLOCK * CPAK_BLOCK_SIZE, read_buf, SAVE_BYTES);
    if (rc) {
        uart_puts("[cpak] prior read rc=-"); uart_putdec((uint32_t)(-rc));
        uart_puts(" bytes_in_buf="); uart_putdec((uint32_t)n64_dbg_recv_buf_bytes);
        uart_puts(" bits="); uart_putdec((uint32_t)n64_dbg_bits_got);
        uart_puts(" to_us="); uart_putdec((uint32_t)n64_dbg_first_to_us);
        uart_puts("\n");
        return 0;
    }
    wc1_save_hdr_t h;
    if (!save_hdr_valid(read_buf, &h)) {
        uart_puts("[cpak] no valid prior save\n");
        return 0;
    }
    uart_puts("[cpak] prior save: counter="); uart_putdec(h.boot_counter);
    uart_puts(" payload_size="); uart_putdec(h.payload_size);
    uart_puts(" crc=0x"); uart_puthex(h.payload_crc32); uart_puts("\n");
    return h.boot_counter;
}

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();

    uart_puts("\n\n=== WC1 save persistence verifier ===\n");

    /* ---- Init SDMMC ---- */
    D("step 1: sdmmc_init");
    int rc = sdmmc_init();
    if (rc) {
        uart_puts("[ex] sdmmc_init rc=-"); uart_putdec((uint32_t)(-rc));
        uart_puts(" — SD backend disabled\n");
    }

    /* ---- Init Cpak ---- */
    D("step 2: input_init(N64) + cpak_probe");
    input_init(INPUT_N64);
    int probe = cpak_probe();
    int cpak_ok = 0;
    switch (probe) {
        case  1: uart_puts("[cpak] Controller Pak detected\n"); cpak_ok = 1; break;
        case  0: uart_puts("[cpak] controller present, no pak\n"); break;
        case  2: uart_puts("[cpak] Rumble Pak detected (not memory)\n"); break;
        case  3: uart_puts("[cpak] Transfer/unknown pak\n"); break;
        case -1: uart_puts("[cpak] no controller\n"); break;
        default: uart_puts("[cpak] unexpected probe="); uart_putdec((uint32_t)probe); uart_puts("\n"); break;
    }

    /* ---- Sniff existing saves ---- */
    D("step 3: read prior saves");
    uint32_t sd_prior   = (rc == 0)  ? sd_read_existing_counter()   : 0;
    uint32_t cpak_prior = (cpak_ok)  ? cpak_read_existing_counter() : 0;
    uint32_t prior = sd_prior > cpak_prior ? sd_prior : cpak_prior;
    uint32_t new_counter = prior + 1;
    uart_puts("[ex] new save counter = ");
    uart_putdec(new_counter);
    uart_puts("\n");

    /* ---- Build the new save ---- */
    D("step 4: build_save");
    build_save(new_counter);
    wc1_save_hdr_t *h = (wc1_save_hdr_t *)save_buf;
    uart_puts("[ex] save header: ver=");      uart_putdec(h->version);
    uart_puts(" counter=");                   uart_putdec(h->boot_counter);
    uart_puts(" payload_size=");              uart_putdec(h->payload_size);
    uart_puts(" crc=0x");                     uart_puthex(h->payload_crc32); uart_puts("\n");

    /* ---- SD round-trip ---- */
    int sd_pass = -1;
    if (rc == 0) {
        D("step 5a: SD write");
        if (sd_save() == 0) {
            D("step 5b: SD read + verify");
            sd_pass = sd_load_and_verify(new_counter);
        }
    } else {
        uart_puts("[sd] skipping (init failed)\n");
    }

    /* ---- Cpak round-trip ---- */
    int cpak_pass = -1;
    if (cpak_ok) {
        D("step 6a: cpak write");
        if (cpak_save_payload() == 0) {
            D("step 6b: cpak read + verify");
            cpak_pass = cpak_load_and_verify(new_counter);
        }
    } else {
        uart_puts("[cpak] skipping (no Controller Pak)\n");
    }

    /* ---- Summary ---- */
    uart_puts("\n=== summary ===\n");
    uart_puts("  SD backend:   ");
    uart_puts(sd_pass == 0 ? "PASS" : (rc == 0 ? "FAIL" : "SKIP"));
    uart_puts("  (counter ");
    uart_putdec(sd_prior); uart_puts(" -> "); uart_putdec(new_counter); uart_puts(")\n");
    uart_puts("  Cpak backend: ");
    uart_puts(cpak_pass == 0 ? "PASS" : (cpak_ok ? "FAIL" : "SKIP"));
    uart_puts("  (counter ");
    uart_putdec(cpak_prior); uart_puts(" -> "); uart_putdec(new_counter); uart_puts(")\n");
    uart_puts("\n[ex] power-cycle and re-run — counters should monotonically increase.\n");

    for (;;) { }
}
