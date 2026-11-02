/*
 * PINE A64+ SDMMC0, native SD bus with CPU FIFO transfers.
 *
 * This implements the existing Jupiter sdmmc.h API. It keeps boot-card
 * power at 3.3 V, uses 4 data lines, and never changes a PLL or PMIC rail.
 * Init is 400 kHz; transfers are 12 MHz from the 24 MHz crystal through
 * A64's fixed /2 MMC post-divider. A64's new timing mode and sample-delay
 * register are explicitly configured after reset.
 *
 * Derived from lib/sdmmc.c's protocol and PIO implementation. Register
 * differences were checked against the A64 User Manual v1.0 section 4.3,
 * Linux v6.12 drivers/mmc/host/sunxi-mmc.c and
 * drivers/clk/sunxi-ng/ccu-sun50i-a64.c, and U-Boot v2025.10
 * drivers/mmc/sunxi_mmc.c. The A64 clock has no old/new mode
 * switch in the CCU: unlike A83T, CCU bit 30 must not be invented here.
 *
 * Hardware validation is required. Errors are propagated; failed data
 * transfers invalidate the card state until sdmmc_init() succeeds again.
 * No writes are performed by initialization or reads.
 */
#include "jupiter.h"
#include "sdmmc.h"
#include "peripherals.h"
#include <stddef.h>

#define SD_BASE           0x01C0F000u
#define SD(reg)           REG32(SD_BASE + (reg))
#define SD_GCTL           0x000u
#define SD_CLKCR          0x004u
#define SD_TIMEOUT        0x008u
#define SD_WIDTH          0x00Cu
#define SD_BLOCK_SIZE     0x010u
#define SD_BYTE_COUNT     0x014u
#define SD_CMD            0x018u
#define SD_ARG            0x01Cu
#define SD_RESP0          0x020u
#define SD_RESP1          0x024u
#define SD_RESP2          0x028u
#define SD_RESP3          0x02Cu
#define SD_IRQ_MASK       0x030u
#define SD_IRQ_RAW        0x038u
#define SD_STATUS         0x03Cu
#define SD_NTSR           0x05Cu
#define SD_DMA_CTRL       0x080u
#define SD_DMA_IRQ_EN     0x08Cu
#define SD_SAMPLE_DELAY   0x144u
#define SD_FIFO           0x200u

#define GCTL_RESET        (BIT(0) | BIT(1) | BIT(2))
#define GCTL_FIFO_RESET   BIT(1)
#define GCTL_CPU_FIFO     BIT(31)
#define CLK_ENABLE        BIT(16)
#define CLK_LOW_POWER     BIT(17)
#define CLK_MASK_DATA0    BIT(31)
#define CMD_LOAD          BIT(31)
#define CMD_UPDATE_CLOCK  BIT(21)
#define CMD_INIT_CLOCKS   BIT(15)
#define CMD_STOP_ABORT    BIT(14)
#define CMD_WAIT_PREV     BIT(13)
#define CMD_AUTO_STOP     BIT(12)
#define CMD_WRITE         BIT(10)
#define CMD_DATA          BIT(9)
#define CMD_RESP_CRC      BIT(8)
#define CMD_LONG_RESP     BIT(7)
#define CMD_RESP          BIT(6)

#define IRQ_CMD_DONE      BIT(2)
#define IRQ_DATA_DONE     BIT(3)
#define IRQ_RESP_TIMEOUT  BIT(8)
#define IRQ_AUTO_DONE     BIT(14)
#define IRQ_ERROR_MASK    (BIT(1) | BIT(6) | BIT(7) | BIT(8) | BIT(9) | \
                           BIT(11) | BIT(12) | BIT(13) | BIT(15))
#define STATUS_FIFO_EMPTY BIT(2)
#define STATUS_FIFO_FULL  BIT(3)
#define STATUS_DATA_BUSY  BIT(9)
#define STATUS_FSM_BUSY   BIT(10)

#define CCU_SD_BUS_GATE   REG32(CCU_BASE + 0x060u)
#define CCU_SD_CLOCK      REG32(CCU_BASE + 0x088u)
#define CCU_SD_BUS_RESET  REG32(CCU_BASE + 0x2C0u)
#define CCU_SD_BIT        BIT(8)
#define SD_OSC_RATE       12000000u  /* 24 MHz oscillator, fixed A64 /2 */

#define PF_CFG0           REG32(PIO_BASE + 0xB4u)
#define PF_DATA           REG32(PIO_BASE + 0xC4u)
#define PF_DRIVE0         REG32(PIO_BASE + 0xC8u)
#define PF_PULL0          REG32(PIO_BASE + 0xD0u)
#define PF_CARD_DETECT    BIT(6)     /* Pine64 card detect, active low */

/* SD card status bits: errors and locked/protected states. CSD/SCR status
 * indicators, current state, READY_FOR_DATA and APP_CMD are not errors. */
#define R1_ERROR_MASK     0xFFF98008u
#define R1_READY          BIT(8)
#define R1_APP_CMD        BIT(5)
#define R1_STATE(v)       (((v) >> 9) & 15u)
#define R1_STATE_TRANSFER 4u
#define R6_ERROR_MASK     (BIT(15) | BIT(14) | BIT(13))
#define MAX_BLOCKS_CMD    128u

enum response_kind { RESPONSE_NONE, RESPONSE_R1, RESPONSE_R1B,
                     RESPONSE_R2, RESPONSE_R3, RESPONSE_R6, RESPONSE_R7 };

static sdmmc_card_t card;
static uint32_t last_irq_status;
static int card_write_protected;

const sdmmc_card_t *sdmmc_card(void)
{
    return &card;
}

static void log_value(const char *name, uint32_t value)
{
    uart_puts("[sdmmc/pine64] ");
    uart_puts(name);
    uart_puts("=");
    uart_puthex(value);
    uart_puts("\n");
}

static int elapsed_us(uint32_t start, uint32_t us)
{
    /* All callers use at most 5 s: this product and the delta fit u32. */
    return timer_elapsed(start, timer_read()) >= us * 24u;
}

static void delay_microseconds(uint32_t us)
{
    uint32_t start = timer_read();
    while (!elapsed_us(start, us))
        ;
}

static int wait_clear(uint32_t reg, uint32_t mask, uint32_t timeout_us)
{
    uint32_t start = timer_read();
    while (SD(reg) & mask) {
        if (elapsed_us(start, timeout_us))
            return PINE64_SD_ERR_TIMEOUT;
    }
    return 0;
}

static int wait_irq(uint32_t bits, uint32_t timeout_us)
{
    uint32_t start = timer_read();
    for (;;) {
        last_irq_status = SD(SD_IRQ_RAW);
        if (last_irq_status & IRQ_ERROR_MASK) {
            /* RESPONSE_TIMEOUT can precede COMMAND_DONE. Linux's A64
             * driver waits for both before finalizing. In particular,
             * CMD55 must not race an unfinished legacy-card CMD8. */
            while ((last_irq_status & IRQ_RESP_TIMEOUT) &&
                   !(last_irq_status & IRQ_CMD_DONE)) {
                if (elapsed_us(start, timeout_us)) {
                    log_value("timed-out command did not finish", last_irq_status);
                    return PINE64_SD_ERR_CONTROLLER;
                }
                last_irq_status |= SD(SD_IRQ_RAW);
            }
            log_value("interrupt error", last_irq_status);
            return ((last_irq_status & IRQ_ERROR_MASK) == IRQ_RESP_TIMEOUT)
                       ? PINE64_SD_ERR_TIMEOUT : PINE64_SD_ERR_CONTROLLER;
        }
        if ((last_irq_status & bits) == bits)
            return 0;
        if (elapsed_us(start, timeout_us)) {
            log_value("interrupt timeout", last_irq_status);
            return PINE64_SD_ERR_TIMEOUT;
        }
    }
}

static int controller_reset(void)
{
    SD(SD_GCTL) = GCTL_RESET;
    int rc = wait_clear(SD_GCTL, GCTL_RESET, 50000u);
    if (rc)
        return rc;
    SD(SD_GCTL) = GCTL_CPU_FIFO;
    SD(SD_IRQ_MASK) = 0;
    SD(SD_DMA_IRQ_EN) = 0;
    SD(SD_DMA_CTRL) = 0;
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    SD(SD_TIMEOUT) = 0xFFFFFFFFu;
    SD(SD_WIDTH) = 0;  /* Reset card protocol starts with one data line. */
    return 0;
}

static int set_card_clock_enabled(int enabled)
{
    uint32_t value = SD(SD_CLKCR);
    value &= ~(CLK_ENABLE | CLK_LOW_POWER | CLK_MASK_DATA0 | 0xFFu);
    if (enabled)
        value |= CLK_ENABLE;

    /* A64 requires masking DAT0 during clock update, even when a card
     * is holding DAT0 busy. Undo the mask after this update command. */
    SD(SD_CLKCR) = value | CLK_MASK_DATA0;
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    SD(SD_CMD) = CMD_LOAD | CMD_UPDATE_CLOCK | CMD_WAIT_PREV;
    int rc = wait_clear(SD_CMD, CMD_LOAD, 750000u);
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    SD(SD_CLKCR) &= ~CLK_MASK_DATA0;
    return rc;
}

static uint32_t oscillator_clock_value(uint32_t requested_hz)
{
    uint32_t divisor = (SD_OSC_RATE + requested_hz - 1u) / requested_hz;
    uint32_t n = 0;
    while (divisor > 16u && n < 3u) {
        divisor = (divisor + 1u) / 2u;
        ++n;
    }
    return BIT(31) | (n << 16) | (divisor - 1u);
}

static int set_card_clock(uint32_t requested_hz)
{
    if (requested_hz < 400000u || requested_hz > SD_OSC_RATE)
        return PINE64_SD_ERR_ARGUMENT;
    int rc = set_card_clock_enabled(0);
    if (rc)
        return rc;
    CCU_SD_CLOCK &= ~BIT(31);
    delay_microseconds(2);
    CCU_SD_CLOCK = oscillator_clock_value(requested_hz);
    delay_microseconds(2);

    /* New timing, 90-degree sampling phase. Sample delay is manual zero,
     * which is the A64 setting used by upstream Linux and U-Boot. */
    SD(SD_NTSR) = BIT(31);
    SD(SD_SAMPLE_DELAY) = BIT(7);
    __asm__ volatile("dsb sy" ::: "memory");
    return set_card_clock_enabled(1);
}

static uint32_t response_flags(enum response_kind kind)
{
    if (kind == RESPONSE_NONE)
        return 0;
    if (kind == RESPONSE_R3)
        return CMD_RESP;  /* ACMD41 has no response CRC. */
    if (kind == RESPONSE_R2)
        return CMD_RESP | CMD_RESP_CRC | CMD_LONG_RESP;
    return CMD_RESP | CMD_RESP_CRC;
}

static int send_command(uint32_t opcode, uint32_t argument,
                        enum response_kind kind, uint32_t extra,
                        uint32_t *response)
{
    int rc = wait_clear(SD_CMD, CMD_LOAD, 1000000u);
    if (!rc)
        rc = wait_clear(SD_STATUS, STATUS_FSM_BUSY, 1000000u);
    if (rc)
        return rc;
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    SD(SD_ARG) = argument;
    SD(SD_CMD) = CMD_LOAD | (opcode & 63u) | response_flags(kind) | extra;
    rc = wait_irq(IRQ_CMD_DONE, 1000000u);
    if (rc) {
        log_value("command", opcode);
        return rc;
    }

    uint32_t r0 = SD(SD_RESP0);
    if (((kind == RESPONSE_R1 || kind == RESPONSE_R1B) && (r0 & R1_ERROR_MASK)) ||
        (kind == RESPONSE_R6 && (r0 & R6_ERROR_MASK))) {
        log_value("card status", r0);
        return PINE64_SD_ERR_CARD;
    }
    if (response) {
        response[0] = r0;
        if (kind == RESPONSE_R2) {
            /* Preserve Jupiter's RESP0..3, least significant word first. */
            response[1] = SD(SD_RESP1);
            response[2] = SD(SD_RESP2);
            response[3] = SD(SD_RESP3);
        }
    }
    SD(SD_IRQ_RAW) = IRQ_CMD_DONE;
    if (kind == RESPONSE_R1B)
        return wait_clear(SD_STATUS, STATUS_DATA_BUSY, 5000000u);
    return 0;
}

static int app_command(uint32_t opcode, uint32_t argument,
                       enum response_kind kind, uint32_t *response)
{
    uint32_t status = 0;
    int rc = send_command(55u, (uint32_t)card.rca << 16, RESPONSE_R1, 0, &status);
    if (rc)
        return rc;
    if (!(status & R1_APP_CMD))
        return PINE64_SD_ERR_CARD;
    return send_command(opcode, argument, kind, 0, response);
}

static uint32_t csd_capacity(const uint32_t csd[4])
{
    uint32_t structure = csd[3] >> 30;
    uint64_t blocks;
    if (structure == 0) {
        uint32_t length_log2 = (csd[2] >> 16) & 15u;
        uint32_t size = ((csd[2] & 0x3FFu) << 2) | (csd[1] >> 30);
        uint32_t multiplier = (csd[1] >> 15) & 7u;
        if (length_log2 < 9u || length_log2 > 11u)
            return 0;
        blocks = (uint64_t)(size + 1u) << (multiplier + 2u + length_log2 - 9u);
    } else if (structure == 1) {
        uint32_t size = ((csd[2] & 63u) << 16) | (csd[1] >> 16);
        blocks = (uint64_t)(size + 1u) * 1024u;
    } else {
        return 0;  /* SDUC needs an extended address and capacity API. */
    }
    return blocks > UINT32_MAX ? 0 : (uint32_t)blocks;
}

static int wait_card_ready(void)
{
    uint32_t start = timer_read();
    do {
        uint32_t status;
        int rc = send_command(13u, (uint32_t)card.rca << 16, RESPONSE_R1, 0, &status);
        if (rc)
            return rc;
        if ((status & R1_READY) && R1_STATE(status) == R1_STATE_TRANSFER)
            return 0;
        delay_microseconds(1000u);
    } while (!elapsed_us(start, 5000000u));
    return PINE64_SD_ERR_TIMEOUT;
}

int sdmmc_init(void)
{
    card.initialised = 0;
    card.is_sdhc = 0;
    card.rca = 0;
    card.num_blocks = 0;
    card_write_protected = 0;
    last_irq_status = 0;
    for (unsigned i = 0; i < 4; ++i)
        card.cid[i] = card.csd[i] = 0;
    if (!pine64_timer_ready())
        return PINE64_SD_ERR_TIMER;

    /* PF0..5 are dedicated to the boot SD card, with 30 mA drive and
     * pull-ups; PF6 is card detect. Leave PF7 and other ports untouched. */
    PF_CFG0 = (PF_CFG0 & 0xF0000000u) | 0x00222222u;
    PF_DRIVE0 = (PF_DRIVE0 & ~0xFFFu) | 0xAAAu;
    PF_PULL0 = (PF_PULL0 & ~0x3FFFu) | 0x1555u;
    delay_microseconds(1000u);
    if (PF_DATA & PF_CARD_DETECT) {
        uart_puts("[sdmmc/pine64] no card detected\n");
        return PINE64_SD_ERR_NO_CARD;
    }

    CCU_SD_BUS_GATE |= CCU_SD_BIT;
    CCU_SD_BUS_RESET &= ~CCU_SD_BIT;
    delay_microseconds(2);
    CCU_SD_BUS_RESET |= CCU_SD_BIT;
    CCU_SD_CLOCK = oscillator_clock_value(400000u);
    delay_microseconds(50u);
    int rc = controller_reset();
    if (rc)
        return rc;
    rc = set_card_clock(400000u);
    if (rc)
        return rc;

    uart_puts("[sdmmc/pine64] identify at 400 kHz\n");
    rc = send_command(0u, 0, RESPONSE_NONE, CMD_INIT_CLOCKS, NULL);
    if (rc)
        return rc;
    delay_microseconds(2000u);

    uint32_t response = 0;
    rc = send_command(8u, 0x1AAu, RESPONSE_R7, 0, &response);
    int v2 = (rc == 0);
    if (v2 && (response & 0xFFFu) != 0x1AAu)
        return PINE64_SD_ERR_VOLTAGE;
    if (rc && !(rc == PINE64_SD_ERR_TIMEOUT && (last_irq_status & IRQ_RESP_TIMEOUT)))
        return rc;  /* Only absent CMD8 is the expected old-SD response. */

    uint32_t ocr = 0;
    uint32_t start = timer_read();
    do {
        rc = app_command(41u, (v2 ? BIT(30) : 0) | 0x00300000u, RESPONSE_R3, &ocr);
        if (rc)
            return rc;
        if (ocr & BIT(31))
            break;
        delay_microseconds(1000u);
    } while (!elapsed_us(start, 2000000u));
    if (!(ocr & BIT(31)))
        return PINE64_SD_ERR_TIMEOUT;
    if (!(ocr & 0x00300000u))
        return PINE64_SD_ERR_VOLTAGE;
    card.is_sdhc = (v2 && (ocr & BIT(30))) ? 1 : 0;

    rc = send_command(2u, 0, RESPONSE_R2, 0, card.cid);
    if (rc)
        return rc;
    rc = send_command(3u, 0, RESPONSE_R6, 0, &response);
    if (rc)
        return rc;
    card.rca = (uint16_t)(response >> 16);
    if (!card.rca)
        return PINE64_SD_ERR_CARD;
    rc = send_command(9u, (uint32_t)card.rca << 16, RESPONSE_R2, 0, card.csd);
    if (rc)
        return rc;
    card.num_blocks = csd_capacity(card.csd);
    if (!card.num_blocks || ((card.csd[3] >> 30) == 1u) != !!card.is_sdhc)
        return PINE64_SD_ERR_CAPACITY;
    card_write_protected = !!(card.csd[0] & (BIT(13) | BIT(12)));

    rc = send_command(7u, (uint32_t)card.rca << 16, RESPONSE_R1B, 0, NULL);
    if (rc)
        return rc;
    rc = app_command(6u, 2u, RESPONSE_R1, NULL);
    if (rc)
        return rc;
    SD(SD_WIDTH) = 1u;
    /* SDHC/SDXC have a fixed 512-byte block size. */
    if (!card.is_sdhc) {
        rc = send_command(16u, SDMMC_BLOCK_SIZE, RESPONSE_R1, 0, NULL);
        if (rc)
            return rc;
    }
    rc = set_card_clock(SD_OSC_RATE);
    if (rc)
        return rc;
    rc = wait_card_ready();
    if (rc)
        return rc;
    card.initialised = 1;
    log_value("blocks", card.num_blocks);
    uart_puts("[sdmmc/pine64] ready, 4-bit, 12 MHz, CPU FIFO\n");
    return 0;
}

static void abort_transfer(void)
{
    /* The previous command might have left the card in DATA/RCV state.
     * STOP_ABORT intentionally bypasses the normal wait-for-idle gate. */
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    SD(SD_ARG) = 0;
    SD(SD_CMD) = CMD_LOAD | CMD_STOP_ABORT | CMD_RESP | CMD_RESP_CRC | 12u;
    (void)wait_clear(SD_CMD, CMD_LOAD, 250000u);
    (void)wait_clear(SD_STATUS, STATUS_DATA_BUSY, 500000u);
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    card.initialised = 0;
}

static int transfer_blocks(uint32_t lba, uint32_t count, uint32_t *data, int write)
{
    int rc = wait_clear(SD_CMD, CMD_LOAD, 1000000u);
    if (!rc)
        rc = wait_clear(SD_STATUS, STATUS_FSM_BUSY | STATUS_DATA_BUSY, 5000000u);
    if (rc)
        goto fail;
    SD(SD_GCTL) |= GCTL_FIFO_RESET;
    rc = wait_clear(SD_GCTL, GCTL_FIFO_RESET, 50000u);
    if (rc)
        goto fail;
    SD(SD_GCTL) = GCTL_CPU_FIFO;
    SD(SD_BLOCK_SIZE) = SDMMC_BLOCK_SIZE;
    SD(SD_BYTE_COUNT) = count * SDMMC_BLOCK_SIZE;
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    SD(SD_ARG) = card.is_sdhc ? lba : lba * SDMMC_BLOCK_SIZE;
    uint32_t opcode = write ? (count > 1u ? 25u : 24u) : (count > 1u ? 18u : 17u);
    SD(SD_CMD) = CMD_LOAD | CMD_RESP | CMD_RESP_CRC | CMD_DATA | CMD_WAIT_PREV |
                 (write ? CMD_WRITE : 0) | (count > 1u ? CMD_AUTO_STOP : 0) | opcode;

    uint32_t remaining = count * (SDMMC_BLOCK_SIZE / 4u);
    uint32_t start = timer_read();
    uint32_t transfer_start = start;
    int response_checked = 0;
    while (remaining) {
        if (elapsed_us(transfer_start, 5000000u)) {
            rc = PINE64_SD_ERR_TIMEOUT;
            goto fail;
        }
        uint32_t status = SD(SD_IRQ_RAW);
        if (status & IRQ_ERROR_MASK) {
            log_value("data interrupt error", status);
            rc = PINE64_SD_ERR_CONTROLLER;
            goto fail;
        }
        if (!response_checked && (status & IRQ_CMD_DONE)) {
            if (SD(SD_RESP0) & R1_ERROR_MASK) {
                log_value("data card status", SD(SD_RESP0));
                rc = PINE64_SD_ERR_CARD;
                goto fail;
            }
            response_checked = 1;
        }
        uint32_t blocked = write ? STATUS_FIFO_FULL : STATUS_FIFO_EMPTY;
        if (!(SD(SD_STATUS) & blocked)) {
            if (write)
                SD(SD_FIFO) = *data++;
            else
                *data++ = SD(SD_FIFO);
            --remaining;
            start = timer_read();
        } else if (elapsed_us(start, 1000000u)) {
            rc = PINE64_SD_ERR_TIMEOUT;
            goto fail;
        }
    }

    rc = wait_irq(IRQ_CMD_DONE | IRQ_DATA_DONE | (count > 1u ? IRQ_AUTO_DONE : 0),
                  5000000u);
    if (rc)
        goto fail;
    if (!response_checked && (SD(SD_RESP0) & R1_ERROR_MASK)) {
        rc = PINE64_SD_ERR_CARD;
        goto fail;
    }
    rc = wait_clear(SD_STATUS, STATUS_FSM_BUSY | STATUS_DATA_BUSY, 5000000u);
    if (rc)
        goto fail;
    SD(SD_IRQ_RAW) = 0xFFFFFFFFu;
    /* A finished FIFO transfer is insufficient for successful media
     * programming. CMD13 checks the card's final status and TRAN state. */
    rc = wait_card_ready();
    if (rc)
        goto fail;
    return 0;

fail:
    abort_transfer();
    uart_puts("[sdmmc/pine64] transfer failed; card requires reinitialization\n");
    return rc;
}

static int validate_request(uint32_t lba, uint32_t count, const void *data)
{
    if (!card.initialised)
        return PINE64_SD_ERR_NOT_READY;
    if (count == 0)
        return 0;
    if (!data || ((uintptr_t)data & 3u))
        return PINE64_SD_ERR_ARGUMENT;
    if (lba >= card.num_blocks || count > card.num_blocks - lba)
        return PINE64_SD_ERR_ARGUMENT;
    if ((uint64_t)count * SDMMC_BLOCK_SIZE > UINTPTR_MAX - (uintptr_t)data)
        return PINE64_SD_ERR_ARGUMENT;
    if (!card.is_sdhc && (uint64_t)(lba + count - 1u) * SDMMC_BLOCK_SIZE > UINT32_MAX)
        return PINE64_SD_ERR_ARGUMENT;
    return 0;
}

int sdmmc_read_blocks(uint32_t lba, uint32_t count, void *dst)
{
    int rc = validate_request(lba, count, dst);
    if (rc)
        return rc;
    uint32_t *data = dst;
    while (count) {
        uint32_t chunk = count > MAX_BLOCKS_CMD ? MAX_BLOCKS_CMD : count;
        rc = transfer_blocks(lba, chunk, data, 0);
        if (rc)
            return rc;
        data += chunk * (SDMMC_BLOCK_SIZE / 4u);
        lba += chunk;
        count -= chunk;
    }
    return 0;
}

int sdmmc_write_blocks(uint32_t lba, uint32_t count, const void *src)
{
    int rc = validate_request(lba, count, src);
    if (rc || !count)
        return rc;
    if (card_write_protected)
        return PINE64_SD_ERR_CARD;
    const uint32_t *data = src;
    while (count) {
        uint32_t chunk = count > MAX_BLOCKS_CMD ? MAX_BLOCKS_CMD : count;
        /* transfer_blocks does not modify the pointed data on writes. */
        rc = transfer_blocks(lba, chunk, (uint32_t *)(uintptr_t)data, 1);
        if (rc)
            return rc;
        data += chunk * (SDMMC_BLOCK_SIZE / 4u);
        lba += chunk;
        count -= chunk;
    }
    return 0;
}
