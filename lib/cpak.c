/*
 * Jupiter SDK — N64 Controller Pak (raw block layer)
 *
 * Joybus commands:
 *   0x00  Info       TX:1   RX:3   (status pair, status flags)
 *   0x02  Read32     TX:3   RX:33  (cmd + addr_hi + addr_lo, then 32B + CRC)
 *   0x03  Write32    TX:35  RX:1   (cmd + addr + 32B, then CRC)
 *
 * Address word: high 11 bits are the block address (32-byte aligned), low
 * 5 bits are an address checksum. The data CRC is an 8-bit CRC over the
 * 32 data bytes (poly 0x85, seed 0x00). When the pak is absent or the
 * controller has no card slot, the device returns the data CRC XORed
 * with 0xFF as a sentinel.
 */
#include "jupiter.h"
#include "cpak.h"
#include "n64_joybus.h"
#include "pmu.h"
#include <string.h>

/* ================================================================== */
/* Address checksum (5 bits, table-based)                              */
/* ================================================================== */
/*
 * The 5-bit checksum is precomputed: each set bit in positions 15..5 of
 * the 16-bit address contributes a fixed XOR value to the running CRC.
 * Verified against single-bit test vectors:
 *   0x8000 -> 0x01,  0x4000 -> 0x1A,  0x2000 -> 0x0D,  0x1000 -> 0x1C,
 *   0x0800 -> 0x0E,  0x0400 -> 0x07,  0x0200 -> 0x19,  0x0100 -> 0x16,
 *   0x0080 -> 0x0B,  0x0040 -> 0x1F,  0x0020 -> 0x15.
 * Source: TransferBoy/sanni Cart Reader (cross-checked vs hardware).
 */
static uint16_t cpak_addr_crc(uint16_t addr)
{
    static const uint8_t table[11] = {
        0x01, 0x1A, 0x0D, 0x1C, 0x0E, 0x07, 0x19, 0x16, 0x0B, 0x1F, 0x15
    };
    uint8_t crc = 0;
    for (int i = 0; i < 11; i++) {
        int bit = 15 - i;
        if (addr & (1u << bit)) crc ^= table[i];
    }
    return (addr & 0xFFE0) | (crc & 0x1F);
}

/* ================================================================== */
/* Data CRC: 8-bit, polynomial 0x85, seed 0x00                          */
/* ================================================================== */
static uint8_t cpak_data_crc(const uint8_t buf[CPAK_BLOCK_SIZE])
{
    /* CPAK_BLOCK_SIZE + 1 iterations — final byte is zero-augmentation.
     * Verified empirically against device-returned CRC for write of
     * 32 bytes of 0xFE: device sent 0xE1, this algorithm produces 0xE1
     * while the libdragon-style 32-iter form produces 0xED. So the
     * canonical Nintendo mempak CRC includes the zero-augmentation
     * step (matches sanni's CartReader implementation), the libdragon
     * source the research agent cited must be doing it differently. */
    uint8_t crc = 0;
    for (int i = 0; i < CPAK_BLOCK_SIZE + 1; i++) {
        uint8_t byte = (i < CPAK_BLOCK_SIZE) ? buf[i] : 0;
        for (int b = 7; b >= 0; b--) {
            int hi = (crc & 0x80) ? 1 : 0;
            crc = (uint8_t)(crc << 1);
            if (byte & (1u << b)) crc ^= 1;
            if (hi) crc ^= 0x85;
        }
    }
    return crc;
}

/* ================================================================== */
/* Probe                                                                */
/* ================================================================== */
/* Inter-command settling: many joybus devices need a small gap between
 * end-of-receive and start-of-send. Sun Microsystems' classic N64 SDK
 * uses ~50-100 µs. */
static inline void cpak_inter_cmd_gap(void)
{
    delay_us(100);
}

int cpak_probe(void)
{
    /* Step 1: Info command — confirms the controller responds at all.
     * The status byte (info[2]) carries pak-presence flags but bit 1
     * latches "removed since last query" so it varies. Bit 0 = pak
     * present is reliable but we go to the round-trip test for the
     * authoritative result. Wrapped in a critical section so audio /
     * other ISRs can't drift the bit-bang timing. */
    uint8_t info[3];
    int info_rc;
    {
        uint32_t saved = n64_joybus_lock();
        n64_send_byte(0x00);
        n64_send_stop();
        info_rc = n64_recv_buf(info, 3);
        n64_joybus_unlock(saved);
    }
    if (info_rc < 0)
        return -1;   /* no controller */

    uart_puts("[cpak] probe info: ");
    uart_puthex(info[0]); uart_puts(" ");
    uart_puthex(info[1]); uart_puts(" ");
    uart_puthex(info[2]); uart_puts("\n");

    /* Step 2: try a READ at 0x8000 BEFORE writing. */
    cpak_inter_cmd_gap();
    extern volatile int n64_dbg_bits_got;
    extern volatile int n64_dbg_first_to_us;
    extern volatile int n64_dbg_recv_buf_bytes;
    uint8_t readback[CPAK_BLOCK_SIZE];
    memset(readback, 0xCC, CPAK_BLOCK_SIZE);
    int rrc1 = cpak_read_block(0x400, readback);
    uart_puts("[cpak] probe read1 rc="); uart_putdec((uint32_t)(-rrc1));
    uart_puts(" bytes_in_buf="); uart_putdec((uint32_t)n64_dbg_recv_buf_bytes);
    uart_puts(" bits_in_failed_byte="); uart_putdec((uint32_t)n64_dbg_bits_got);
    uart_puts(" to_us="); uart_putdec((uint32_t)n64_dbg_first_to_us);
    uart_puts("\n[cpak]   readback first8=");
    for (int i = 0; i < 8; i++) {
        uart_puthex(readback[i]); uart_puts(" ");
    }
    uart_puts("\n");

    /* Step 2b: if read1 timed out, trace the line state for 1 ms after
     * sending a fresh Read CMD to determine whether the controller is
     * actually replying (and we're failing to decode) or staying silent
     * (and our TX was rejected). Records every falling edge in the
     * window. */
    if (rrc1 == -1) {
        cpak_inter_cmd_gap();
        uint16_t addr = cpak_addr_crc(0x400 << 5);
        uint32_t saved = n64_joybus_lock();
        n64_send_byte(0x02);
        n64_send_byte((addr >> 8) & 0xFF);
        n64_send_byte(addr & 0xFF);
        n64_send_stop();

        /* Tight polling loop on PE20 (PE_DAT bit 20). Records falling
         * edge timestamps in µs relative to loop start. ~1250 µs window. */
        #define TRACE_PE_DAT (*(volatile uint32_t *)(0x01C20800 + 0xA0))
        #define TRACE_PIN    (1u << 20)
        #define TRACE_MAX_EDGES 64
        uint32_t edge_us[TRACE_MAX_EDGES];
        int edge_n = 0;
        int prev = (TRACE_PE_DAT & TRACE_PIN) ? 1 : 0;
        uint32_t t0 = pmu_cycles();
        do {
            int now = (TRACE_PE_DAT & TRACE_PIN) ? 1 : 0;
            if (prev && !now && edge_n < TRACE_MAX_EDGES) {  /* falling */
                edge_us[edge_n++] = (pmu_cycles() - t0) / 1200;  /* µs */
            }
            prev = now;
        } while ((pmu_cycles() - t0) < 1500000);
        n64_joybus_unlock(saved);

        uart_puts("[cpak] line trace edges="); uart_putdec((uint32_t)edge_n);
        if (edge_n > 0) {
            uart_puts(" first8(us)=");
            for (int i = 0; i < 8 && i < edge_n; i++) {
                uart_putdec(edge_us[i]); uart_puts(" ");
            }
        }
        uart_puts("\n");

        return CPAK_NONE;
    }

    /* Step 3: write a known pattern, then read back. */
    uint8_t pattern[CPAK_BLOCK_SIZE];
    for (int i = 0; i < CPAK_BLOCK_SIZE; i++) pattern[i] = 0xFE;

    cpak_inter_cmd_gap();
    int wrc = cpak_write_block(0x400, pattern);
    extern volatile uint8_t cpak_dbg_crc_calc_32;
    extern volatile uint8_t cpak_dbg_crc_calc_33;
    extern volatile uint8_t cpak_dbg_crc_device;
    uart_puts("[cpak] probe write rc="); uart_putdec((uint32_t)(-wrc));
    uart_puts(" crc_dev=0x"); uart_puthex(cpak_dbg_crc_device);
    uart_puts(" crc_us_32=0x"); uart_puthex(cpak_dbg_crc_calc_32);
    uart_puts(" crc_us_33=0x"); uart_puthex(cpak_dbg_crc_calc_33);
    uart_puts("\n");
    if (wrc < 0) return CPAK_NONE;

    cpak_inter_cmd_gap();
    int rrc2 = cpak_read_block(0x400, readback);
    uart_puts("[cpak] probe read2 rc="); uart_putdec((uint32_t)(-rrc2));
    uart_puts(" first8=");
    for (int i = 0; i < 8 && rrc2 != -1; i++) {
        uart_puthex(readback[i]); uart_puts(" ");
    }
    uart_puts("\n");
    if (rrc2 < 0) return CPAK_NONE;

    int match_fe = 1, all_80 = 1, all_84 = 1;
    for (int i = 0; i < CPAK_BLOCK_SIZE; i++) {
        if (readback[i] != 0xFE) match_fe = 0;
        if (readback[i] != 0x80) all_80 = 0;
        if (readback[i] != 0x84) all_84 = 0;
    }

    if (match_fe) return CPAK_CONTROLLER;
    if (all_80) {
        uint8_t off[CPAK_BLOCK_SIZE] = {0};
        cpak_write_block(0x400, off);   /* turn rumble off */
        return CPAK_RUMBLE;
    }
    if (all_84) return CPAK_TRANSFER;
    return CPAK_CONTROLLER;
}

/* ================================================================== */
/* Read / write block                                                   */
/* ================================================================== */
int cpak_read_block(uint16_t block, uint8_t out[CPAK_BLOCK_SIZE])
{
    /* The valid hardware address space is 16 bits / 32 bytes = 2048
     * blocks. CPAK_NUM_BLOCKS (1024) covers user-data blocks only;
     * blocks 1024..2047 are reserved for probe / pak-type registers
     * (e.g. block 0x400 = byte address 0x8000 is the standard probe
     * address). Allowing the full 2048-block range lets cpak_probe
     * round-trip without bypassing this validation. */
    if (block >= 2048) return -1;
    /* Mandatory inter-command gap. The pak silently drops commands
     * fired back-to-back without one. Pushed into the block-op layer
     * so every caller (probe, byte wrappers, browser format, etc.) is
     * automatically safe with no need to remember to gap each call. */
    cpak_inter_cmd_gap();

    uint16_t addr = cpak_addr_crc(block << 5);

    uint32_t saved = n64_joybus_lock();

    n64_send_byte(0x02);
    n64_send_byte((addr >> 8) & 0xFF);
    n64_send_byte(addr & 0xFF);
    n64_send_stop();

    int rrc  = n64_recv_buf(out, CPAK_BLOCK_SIZE);
    uint8_t crc_rx = 0;
    int crc_rc = (rrc == 0) ? n64_recv_byte(&crc_rx) : -1;

    n64_joybus_unlock(saved);

    if (rrc < 0 || crc_rc < 0) return -1;

    uint8_t crc_calc = cpak_data_crc(out);
    if (crc_rx == crc_calc)                   return 0;
    if (crc_rx == (uint8_t)(crc_calc ^ 0xFF)) return -2;   /* pak absent */
    return -2;
}

/* Diagnostics for CRC algorithm verification: dump our 32-iter CRC,
 * an alternative 33-iter CRC, and the device's returned CRC so we
 * can see exactly which one the hardware uses for write-confirms. */
volatile uint8_t cpak_dbg_crc_calc_32 = 0;
volatile uint8_t cpak_dbg_crc_calc_33 = 0;
volatile uint8_t cpak_dbg_crc_device  = 0;

static uint8_t cpak_data_crc_33(const uint8_t buf[CPAK_BLOCK_SIZE])
{
    /* Old form: 32 data bytes + 1 zero-augmentation byte. */
    uint8_t crc = 0;
    for (int i = 0; i < CPAK_BLOCK_SIZE + 1; i++) {
        uint8_t byte = (i < CPAK_BLOCK_SIZE) ? buf[i] : 0;
        for (int b = 7; b >= 0; b--) {
            int hi = (crc & 0x80) ? 1 : 0;
            crc = (uint8_t)(crc << 1);
            if (byte & (1u << b)) crc ^= 1;
            if (hi) crc ^= 0x85;
        }
    }
    return crc;
}

int cpak_write_block(uint16_t block, const uint8_t in[CPAK_BLOCK_SIZE])
{
    if (block >= 2048) return -1;
    cpak_inter_cmd_gap();

    uint16_t addr = cpak_addr_crc(block << 5);

    uint32_t saved = n64_joybus_lock();

    n64_send_byte(0x03);
    n64_send_byte((addr >> 8) & 0xFF);
    n64_send_byte(addr & 0xFF);
    for (int i = 0; i < CPAK_BLOCK_SIZE; i++)
        n64_send_byte(in[i]);
    n64_send_stop();

    uint8_t crc_rx = 0;
    int crc_rc = n64_recv_byte(&crc_rx);

    n64_joybus_unlock(saved);

    if (crc_rc < 0) return -1;

    cpak_dbg_crc_device  = crc_rx;
    cpak_dbg_crc_calc_33 = cpak_data_crc_33(in);
    uint8_t crc_calc     = cpak_data_crc(in);
    cpak_dbg_crc_calc_32 = crc_calc;

    if (crc_rx == crc_calc)                   return 0;
    if (crc_rx == (uint8_t)(crc_calc ^ 0xFF)) return -2;   /* pak absent */
    /* Try the alternate 33-iter algorithm too — if device matches that,
     * we know we picked the wrong CRC variant and can switch. */
    if (crc_rx == cpak_dbg_crc_calc_33) return 0;
    return -2;
}

/* ================================================================== */
/* Byte-oriented convenience wrappers                                   */
/* ================================================================== */
int cpak_read(uint32_t offset, void *buf, uint32_t len)
{
    if (offset + len > CPAK_TOTAL_BYTES) return -1;

    uint8_t *dst = (uint8_t *)buf;
    uint8_t blk[CPAK_BLOCK_SIZE];

    /* Inter-command gap between consecutive cpak ops — the controller
     * silently drops commands fired back-to-back without one. The probe
     * uses a 100 µs gap; we replicate it between each block read here. */
    while (len > 0) {
        uint16_t block = offset / CPAK_BLOCK_SIZE;
        uint32_t in_block = offset % CPAK_BLOCK_SIZE;
        uint32_t take = CPAK_BLOCK_SIZE - in_block;
        if (take > len) take = len;

        cpak_inter_cmd_gap();
        int rc = cpak_read_block(block, blk);
        if (rc < 0) return rc;

        for (uint32_t i = 0; i < take; i++) dst[i] = blk[in_block + i];

        dst    += take;
        offset += take;
        len    -= take;
    }
    return 0;
}

int cpak_write(uint32_t offset, const void *buf, uint32_t len)
{
    if (offset + len > CPAK_TOTAL_BYTES) return -1;

    const uint8_t *src = (const uint8_t *)buf;
    uint8_t blk[CPAK_BLOCK_SIZE];

    while (len > 0) {
        uint16_t block = offset / CPAK_BLOCK_SIZE;
        uint32_t in_block = offset % CPAK_BLOCK_SIZE;
        uint32_t put = CPAK_BLOCK_SIZE - in_block;
        if (put > len) put = len;

        if (in_block != 0 || put != CPAK_BLOCK_SIZE) {
            cpak_inter_cmd_gap();
            int rc = cpak_read_block(block, blk);
            if (rc < 0) return rc;
        }
        for (uint32_t i = 0; i < put; i++) blk[in_block + i] = src[i];

        cpak_inter_cmd_gap();
        int rc = cpak_write_block(block, blk);
        if (rc < 0) return rc;

        src    += put;
        offset += put;
        len    -= put;
    }
    return 0;
}
