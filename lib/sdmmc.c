/*
 * Jupiter SDK — Allwinner V3s SDMMC0 raw-block-read driver.
 *
 * Reference: u-boot/drivers/mmc/sunxi_mmc.c (sun4i/sun8i variant) and the
 * Allwinner V3s User Manual section "SD/MMC Host Controller". The V3s
 * SDMMC0 controller is sun8i-mmc compatible; FIFO is at offset 0x200,
 * NTSR at 0x100. SDC0 lives at 0x01C0F000 with bus gate / soft-reset bit
 * 8 in CCU_BUS_CLK_GATE0 (0x60) / CCU_BUS_RST0 (0x2C0). Module clock is
 * SDMMC0_CLK_REG (0x88).
 *
 * Pin layout for SDC0 on V3s (mainline DT confirms):
 *   PF0=D1, PF1=D0, PF2=CLK, PF3=CMD, PF4=D3, PF5=D2 — all mux value 2,
 *   bias-pull-up on all six (CLK pull is harmless; mainline does it too).
 *
 * Power-on flow (matches SD Physical Layer 2.00 §4.2):
 *   1. Send 74 dummy clocks at <400 kHz with CMD line high
 *   2. CMD0 (GO_IDLE_STATE)                                  — no resp
 *   3. CMD8 (SEND_IF_COND, voltage 0x1AA)                    — R7
 *   4. ACMD41 (APP_CMD then SD_SEND_OP_COND, HCS=1, 3.3V)    — R3, loop until ready
 *   5. CMD2 (ALL_SEND_CID)                                   — R2 (CID)
 *   6. CMD3 (SEND_RELATIVE_ADDR)                             — R6 (RCA)
 *   7. CMD9 (SEND_CSD with RCA)                              — R2 (CSD; we parse capacity)
 *   8. CMD7 (SELECT_CARD with RCA)                           — R1b
 *   9. ACMD6 (SET_BUS_WIDTH = 4-bit)                         — R1
 *  10. CMD16 (SET_BLOCKLEN = 512)                            — R1
 *  11. Bump controller clock to 25 MHz
 *
 * Read: CMD17 (READ_SINGLE_BLOCK) per block, drained via FIFO polling.
 */
#include "jupiter.h"
#include "sdmmc.h"
#include <stddef.h>

#define UART_TRACE 1
#if UART_TRACE
  #define LOG(s)        uart_puts("[sdmmc] " s "\n")
  #define LOG_HEX(s, v) do { uart_puts("[sdmmc] " s "=0x"); uart_puthex(v); uart_puts("\n"); } while (0)
  #define LOG_DEC(s, v) do { uart_puts("[sdmmc] " s "="); uart_putdec(v); uart_puts("\n"); } while (0)
#else
  #define LOG(s)        ((void)0)
  #define LOG_HEX(s, v) ((void)0)
  #define LOG_DEC(s, v) ((void)0)
#endif

/* ------------------------------------------------------------------ */
/* SDMMC0 register block @ 0x01C0F000                                 */
/* ------------------------------------------------------------------ */

#define SD0_BASE        0x01C0F000u
#define SD(off)         REG32(SD0_BASE + (off))

#define SD_GCTL         0x000
#define SD_CKCR         0x004
#define SD_TMOR         0x008
#define SD_BWDR         0x00C
#define SD_BKSR         0x010
#define SD_BYCR         0x014
#define SD_CMDR         0x018
#define SD_CAGR         0x01C
#define SD_RESP0        0x020
#define SD_RESP1        0x024
#define SD_RESP2        0x028
#define SD_RESP3        0x02C
#define SD_IMKR         0x030
#define SD_MISR         0x034
#define SD_RISR         0x038
#define SD_STAR         0x03C
#define SD_FWLR         0x040
#define SD_FUNS         0x044
#define SD_DBGC         0x050
#define SD_NTSR         0x100   /* New Timing Set */
#define SD_FIFO         0x200

/* SD_GCTL */
#define GCTL_SOFT_RST       BIT(0)
#define GCTL_FIFO_RST       BIT(1)
#define GCTL_DMA_RST        BIT(2)
#define GCTL_INT_EN         BIT(4)
#define GCTL_DMA_EN         BIT(5)
#define GCTL_CD_DBC_EN      BIT(8)
#define GCTL_FIFO_AC_MOD    BIT(31)  /* 1 = AHB access (CPU PIO via SD_FIFO) */

/* SD_CKCR */
#define CKCR_CCLK_EN        BIT(16)
#define CKCR_CCLK_LP_OFF    BIT(17)  /* 1 = low-power mode disabled (always-running clk) */

/* SD_CMDR */
#define CMDR_LOAD           BIT(31)
#define CMDR_PRG_CLK        BIT(21)  /* update card clock */
#define CMDR_SEND_INIT_SEQ  BIT(15)  /* 80 dummy clocks */
#define CMDR_STOP_ABRT      BIT(14)
#define CMDR_WAIT_PRG_END   BIT(13)
#define CMDR_AUTO_STOP      BIT(12)  /* CMD12 auto on multi-block */
#define CMDR_SEQ_MODE       BIT(11)
#define CMDR_WRITE          BIT(10)
#define CMDR_DATA_TRANS     BIT(9)
#define CMDR_CHK_RESP_CRC   BIT(8)
#define CMDR_LONG_RESP      BIT(7)
#define CMDR_HAS_RESP       BIT(6)

/* SD_RISR — write-1-to-clear (sun8i-mmc, per V3s manual + u-boot driver) */
#define RISR_RESP_ERR       BIT(1)
#define RISR_CMD_OVER       BIT(2)   /* CC  Command Complete */
#define RISR_DATA_OVER      BIT(3)   /* DTO Data Over */
#define RISR_TXDR           BIT(4)   /* TX  FIFO needs more data (NOT err) */
#define RISR_RXDR           BIT(5)   /* RX  FIFO has data ready  (NOT err) */
#define RISR_RESP_CRC_ERR   BIT(6)   /* RCRC */
#define RISR_DATA_CRC_ERR   BIT(7)   /* DCRC */
#define RISR_RESP_TIMEOUT   BIT(8)   /* RTO */
#define RISR_DATA_TIMEOUT   BIT(9)   /* DRTO */
#define RISR_VOL_CHG_DONE   BIT(10)  /* HTO/VC */
#define RISR_FIFO_RUN_ERR   BIT(11)  /* FRUN */
#define RISR_HW_LOCKED      BIT(12)
#define RISR_START_BIT_ERR  BIT(13)  /* SBE */
#define RISR_AUTO_CMD_DONE  BIT(14)  /* ACD */
#define RISR_END_BIT_ERR    BIT(15)  /* EBE */
#define RISR_CARD_INSERT    BIT(30)
#define RISR_CARD_REMOVE    BIT(31)

#define RISR_ERR_MASK       (RISR_RESP_ERR      | RISR_RESP_CRC_ERR |  \
                             RISR_RESP_TIMEOUT  | RISR_DATA_CRC_ERR |  \
                             RISR_DATA_TIMEOUT  | RISR_FIFO_RUN_ERR |  \
                             RISR_HW_LOCKED     | RISR_START_BIT_ERR| \
                             RISR_END_BIT_ERR)

/* SD_STAR — read-only */
#define STAR_CARD_PRESENT       BIT(8)
#define STAR_CARD_DATA_BUSY     BIT(9)
#define STAR_DATA_FSM_BUSY      BIT(10)
#define STAR_FIFO_LEVEL_MASK    (0x1FFu << 17)
#define STAR_FIFO_EMPTY         BIT(2)
#define STAR_FIFO_FULL          BIT(3)

/* ------------------------------------------------------------------ */
/* CCU offsets used here                                              */
/* ------------------------------------------------------------------ */

#define CCU_BUS_GATE0       (CCU_BASE + 0x0060)
#define CCU_BUS_SOFT_RST0   (CCU_BASE + 0x02C0)
#define CCU_SDMMC0_CLK      (CCU_BASE + 0x0088)

#define BUS_SDMMC0_BIT      BIT(8)

/* SDMMC0_CLK_REG bits (0x88) */
#define SDMMC_CLK_GATE      BIT(31)
#define SDMMC_CLK_SRC_OSC24M    (0u << 24)
#define SDMMC_CLK_SRC_PLL_PERIPH (1u << 24)
#define SDMMC_CLK_N(n)      (((n) & 0x3) << 16)
#define SDMMC_CLK_M(m)      (((m) & 0xF) << 0)

/* ------------------------------------------------------------------ */
/* Pinmux: PF0–PF5 → SDC0 (mux value 2)                               */
/* ------------------------------------------------------------------ */

#define PF_CFG0     REG32(PIO_BASE + 0xB4)  /* PF0..PF7, 4 bits/pin */
#define PF_PULL0    REG32(PIO_BASE + 0xD0)  /* PF0..PF15, 2 bits/pin */

/* ------------------------------------------------------------------ */
/* SD command opcodes                                                 */
/* ------------------------------------------------------------------ */

#define MMC_GO_IDLE         0
#define MMC_ALL_SEND_CID    2
#define MMC_SEND_RELATIVE   3
#define MMC_SEND_OP_COND    1   /* MMC; we use SD ACMD41 */
#define SD_SEND_IF_COND     8
#define MMC_SEND_CSD        9
#define MMC_SELECT_CARD     7
#define MMC_SEND_STATUS     13
#define MMC_SET_BLOCKLEN    16
#define MMC_READ_SINGLE     17
#define MMC_READ_MULTIPLE   18
#define MMC_WRITE_SINGLE    24
#define MMC_WRITE_MULTIPLE  25
#define MMC_APP_CMD         55
#define SD_APP_OP_COND      41
#define SD_APP_SET_BUS_WIDTH 6

/* ------------------------------------------------------------------ */
/* Module state                                                       */
/* ------------------------------------------------------------------ */

static sdmmc_card_t g_card;

const sdmmc_card_t *sdmmc_card(void) { return &g_card; }

/* ------------------------------------------------------------------ */
/* Busy-wait helpers                                                  */
/* ------------------------------------------------------------------ */

static void udelay(uint32_t us)
{
    uint32_t t0 = timer_read();
    while (ticks_to_us(timer_elapsed(t0, timer_read())) < us) { }
}

/* Spin while `mask` is set in `reg`. Returns 0 ok, -1 timeout. */
static int wait_clear(uint32_t reg_off, uint32_t mask, uint32_t timeout_us)
{
    uint32_t t0 = timer_read();
    while (SD(reg_off) & mask) {
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > timeout_us) return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Pin / clock / controller bring-up                                  */
/* ------------------------------------------------------------------ */

static void sdmmc_pinmux(void)
{
    /* PF0..PF5 → mux 2 (SDC0), preserve PF6/PF7. */
    uint32_t cfg = PF_CFG0;
    cfg &= ~0x00FFFFFFu;
    cfg |=  0x00222222u;
    PF_CFG0 = cfg;

    /* Pull-up (binary 01) on PF0..PF5 — 2 bits per pin. */
    uint32_t pull = PF_PULL0;
    pull &= ~0x00000FFFu;
    pull |=  0x00000555u;
    PF_PULL0 = pull;
}

/* Program SDMMC0_CLK_REG so the card-side clock lands near `target_hz`.
 * Uses OSC24M for ≤24 MHz targets, PLL_PERIPH (assumed 600 MHz; that's
 * the V3s reset default) for higher rates. Effective divider is
 * 2^N * (M+1) per the V3s user manual. */
static void sdmmc_set_module_clock(uint32_t target_hz)
{
    /* Disable gate before reprogramming. */
    REG32(CCU_SDMMC0_CLK) &= ~SDMMC_CLK_GATE;
    udelay(2);

    uint32_t src_hz;
    uint32_t src_sel;
    if (target_hz <= 24000000u) {
        src_hz = 24000000u;
        src_sel = SDMMC_CLK_SRC_OSC24M;
    } else {
        src_hz = 600000000u;             /* PLL_PERIPH(1x), reset default */
        src_sel = SDMMC_CLK_SRC_PLL_PERIPH;
    }

    uint32_t div = (src_hz + target_hz - 1) / target_hz;
    if (div < 1) div = 1;

    /* Pick smallest N such that (M+1) ≤ 16 with M = div/(2^N) - 1. */
    uint32_t n = 0, m = 0;
    for (n = 0; n < 4; n++) {
        uint32_t s = (div + (1u << n) - 1) >> n;
        if (s <= 16) { m = s ? s - 1 : 0; break; }
    }
    if (n == 4) { n = 3; m = 15; }

    uint32_t v = SDMMC_CLK_GATE | src_sel | SDMMC_CLK_N(n) | SDMMC_CLK_M(m);
    REG32(CCU_SDMMC0_CLK) = v;

    LOG_HEX("clk reg", v);
    LOG_DEC("approx Hz", src_hz / ((1u << n) * (m + 1)));
}

/* Push the new card-side clock to the controller (takes one CMD with
 * UPCLK_ONLY + WAIT_COMPLETE). */
static int sdmmc_update_card_clock(void)
{
    /* Disable card clock first. */
    SD(SD_CKCR) &= ~CKCR_CCLK_EN;

    /* Issue a "program clock" command (no actual cmd index). */
    SD(SD_CMDR) = CMDR_LOAD | CMDR_PRG_CLK | CMDR_WAIT_PRG_END;
    if (wait_clear(SD_CMDR, CMDR_LOAD, 100000)) {
        LOG("update_card_clock: CMD LOAD never cleared");
        return -1;
    }
    SD(SD_RISR) = SD(SD_RISR);  /* clear any spurious bits */

    /* Re-enable the card-side clock. */
    SD(SD_CKCR) |= CKCR_CCLK_EN;

    /* Push again to actually start the new clock. */
    SD(SD_CMDR) = CMDR_LOAD | CMDR_PRG_CLK | CMDR_WAIT_PRG_END;
    if (wait_clear(SD_CMDR, CMDR_LOAD, 100000)) {
        LOG("update_card_clock: CMD LOAD #2 never cleared");
        return -1;
    }
    SD(SD_RISR) = SD(SD_RISR);
    return 0;
}

static int sdmmc_controller_reset(void)
{
    /* Soft-reset controller (auto-clears when done). */
    SD(SD_GCTL) = GCTL_SOFT_RST | GCTL_FIFO_RST | GCTL_DMA_RST;
    if (wait_clear(SD_GCTL, GCTL_SOFT_RST | GCTL_FIFO_RST | GCTL_DMA_RST, 50000)) {
        LOG("controller reset never cleared");
        return -1;
    }
    /* AHB FIFO mode (CPU PIO via SD_FIFO mmio). */
    SD(SD_GCTL) |= GCTL_FIFO_AC_MOD;
    /* Clear all pending IRQ status. */
    SD(SD_RISR) = 0xFFFFFFFFu;
    /* Conservative timeout: 0xFFFFFFFF for both data + response. */
    SD(SD_TMOR) = 0xFFFFFFFFu;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Command transmit                                                    */
/* ------------------------------------------------------------------ */

#define RESP_NONE   0
#define RESP_R1     (CMDR_HAS_RESP | CMDR_CHK_RESP_CRC)
#define RESP_R1B    (CMDR_HAS_RESP | CMDR_CHK_RESP_CRC | CMDR_WAIT_PRG_END)
#define RESP_R2     (CMDR_HAS_RESP | CMDR_LONG_RESP   | CMDR_CHK_RESP_CRC)
#define RESP_R3     (CMDR_HAS_RESP)                       /* no CRC (OCR) */
#define RESP_R6     (CMDR_HAS_RESP | CMDR_CHK_RESP_CRC)
#define RESP_R7     (CMDR_HAS_RESP | CMDR_CHK_RESP_CRC)

/* `extra` carries DATA_TRANS / WRITE / SEND_INIT_SEQ etc. */
static int sdmmc_send_cmd(uint8_t opcode, uint32_t arg, uint32_t resp_flags, uint32_t extra,
                          uint32_t *resp_out)
{
    /* Wait for any in-flight command/data to drain. */
    if (wait_clear(SD_STAR, STAR_DATA_FSM_BUSY, 1000000)) {
        LOG("send_cmd: data fsm busy");
        return -1;
    }
    /* Clear all RISR bits so we can tell what THIS command sets. */
    SD(SD_RISR) = 0xFFFFFFFFu;

    SD(SD_CAGR) = arg;
    SD(SD_CMDR) = CMDR_LOAD | (opcode & 0x3F) | resp_flags | extra;

    /* Wait for command-done (cmd_over). */
    uint32_t risr = 0;
    uint32_t t0 = timer_read();
    while (1) {
        risr = SD(SD_RISR);
        if (risr & RISR_ERR_MASK) {
            LOG_HEX("cmd err RISR", risr);
            LOG_DEC("opcode", opcode);
            return -2;
        }
        if (risr & RISR_CMD_OVER) break;
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 1000000u) {
            LOG_HEX("cmd timeout RISR", risr);
            LOG_DEC("opcode", opcode);
            return -3;
        }
    }

    if (resp_out) {
        if (resp_flags & CMDR_LONG_RESP) {
            /* R2: 128-bit CID/CSD in RESP3..RESP0 (high to low). */
            resp_out[0] = SD(SD_RESP0);
            resp_out[1] = SD(SD_RESP1);
            resp_out[2] = SD(SD_RESP2);
            resp_out[3] = SD(SD_RESP3);
        } else {
            resp_out[0] = SD(SD_RESP0);
        }
    }

    /* Clear cmd_over (and incidentally any benign bits). Errors caught above. */
    SD(SD_RISR) = SD(SD_RISR);
    return 0;
}

/* APP_CMD wrapper: send CMD55 with RCA, then the actual ACMD opcode. */
static int sdmmc_app_cmd(uint8_t acmd, uint32_t arg, uint32_t resp_flags, uint32_t *resp_out)
{
    uint32_t r1 = 0;
    int rc = sdmmc_send_cmd(MMC_APP_CMD, ((uint32_t)g_card.rca) << 16, RESP_R1, 0, &r1);
    if (rc) return rc;
    return sdmmc_send_cmd(acmd, arg, resp_flags, 0, resp_out);
}

/* ------------------------------------------------------------------ */
/* Card identification + init                                         */
/* ------------------------------------------------------------------ */

/* Parse SDHC v2 CSD (CSD_STRUCTURE == 1): C_SIZE in bits [69:48], capacity
 * = (C_SIZE + 1) * 1024 blocks. We have CSD packed RESP0..RESP3 LSW first,
 * with the 8 missing CRC bits left off the bottom (the controller drops
 * CRC). So the full 128-bit CSD is RESP3:RESP2:RESP1:RESP0 with each word
 * shifted left 8 bits relative to the spec. */
static uint32_t sdmmc_csd_blocks(const uint32_t csd[4])
{
    /* CSD_STRUCTURE is bit [127:126] of the 128-bit CSD. With our shift,
     * that lands in CSD[3] bits [31:30]. */
    uint32_t csd_struct = (csd[3] >> 30) & 0x3;
    if (csd_struct == 0) {
        /* CSDv1 (SDSC): READ_BL_LEN at [83:80], C_SIZE at [73:62],
         * C_SIZE_MULT at [49:47]. After our 8-bit shift: */
        uint32_t read_bl_len = (csd[2] >> 16) & 0xF;
        uint32_t c_size      = ((csd[2] & 0x3FFu) << 2) | ((csd[1] >> 30) & 0x3);
        uint32_t c_size_mult = (csd[1] >> 15) & 0x7;
        uint32_t mult        = 1u << (c_size_mult + 2);
        uint32_t blocknr     = (c_size + 1) * mult;
        uint32_t block_len   = 1u << read_bl_len;
        return (uint64_t)blocknr * block_len / SDMMC_BLOCK_SIZE;
    } else {
        /* CSDv2 (SDHC/SDXC): C_SIZE at [69:48], 22 bits.
         * After 8-bit shift: bits [77:56] of (CSD <<8). Lives in csd[2]
         * bits [29:8] (high 16 bits) and ... actually with CSD[3:0] being
         * RESP3..RESP0 the spec bits land as: */
        uint32_t c_size = ((csd[2] & 0x3Fu) << 16) | ((csd[1] >> 16) & 0xFFFFu);
        return (c_size + 1) * 1024u;
    }
}

int sdmmc_init(void)
{
    g_card.initialised = 0;
    g_card.is_sdhc = 0;
    g_card.rca = 0;
    g_card.num_blocks = 0;

    LOG("init: pinmux PF0..PF5 -> SDC0");
    sdmmc_pinmux();

    LOG("init: ungate + reset SDMMC0 in CCU");
    REG32(CCU_BUS_GATE0)     |= BUS_SDMMC0_BIT;
    REG32(CCU_BUS_SOFT_RST0) |= BUS_SDMMC0_BIT;
    udelay(50);

    LOG("init: program module clock to ~400 kHz");
    sdmmc_set_module_clock(400000);

    LOG("init: controller soft reset");
    if (sdmmc_controller_reset()) return -10;

    LOG("init: enable card clock");
    SD(SD_CKCR) = CKCR_CCLK_EN | CKCR_CCLK_LP_OFF;
    if (sdmmc_update_card_clock()) return -11;

    /* 74 init clocks. CMD0 with SEND_INIT_SEQ pulses the line. */
    LOG("init: 80 dummy clocks");
    int rc = sdmmc_send_cmd(0, 0, RESP_NONE, CMDR_SEND_INIT_SEQ, NULL);
    if (rc) { LOG_DEC("init seq rc", (uint32_t)rc); /* not fatal: some cards reply, some don't */ }

    /* CMD0 — go-idle. */
    LOG("CMD0 GO_IDLE");
    rc = sdmmc_send_cmd(MMC_GO_IDLE, 0, RESP_NONE, 0, NULL);
    if (rc) return -20;

    /* CMD8 — voltage probe (SD 2.0+ cards echo the check pattern). */
    uint32_t r7 = 0;
    LOG("CMD8 SEND_IF_COND");
    rc = sdmmc_send_cmd(SD_SEND_IF_COND, 0x000001AAu, RESP_R7, 0, &r7);
    int v2_card = (rc == 0) && ((r7 & 0xFFu) == 0xAAu);
    LOG_DEC("v2 card", (uint32_t)v2_card);

    /* ACMD41 loop. HCS=1 if v2; voltage window = 3.2-3.4V (0x300000). */
    uint32_t ocr = 0;
    uint32_t arg41 = (v2_card ? (1u << 30) : 0u) | 0x00FF8000u;  /* HCS + voltage */
    for (int tries = 0; tries < 1000; tries++) {
        rc = sdmmc_app_cmd(SD_APP_OP_COND, arg41, RESP_R3, &ocr);
        if (rc) {
            LOG_DEC("ACMD41 rc", (uint32_t)rc);
            return -30;
        }
        if (ocr & 0x80000000u) break;  /* card finished power-up */
        udelay(1000);
    }
    if (!(ocr & 0x80000000u)) {
        LOG("ACMD41 timeout — card never left busy");
        return -31;
    }
    g_card.is_sdhc = (ocr & (1u << 30)) ? 1 : 0;
    LOG_HEX("OCR", ocr);
    LOG_DEC("SDHC", g_card.is_sdhc);

    /* CMD2 — get CID. */
    LOG("CMD2 ALL_SEND_CID");
    rc = sdmmc_send_cmd(MMC_ALL_SEND_CID, 0, RESP_R2, 0, g_card.cid);
    if (rc) return -40;

    /* CMD3 — get RCA (R6: bits [31:16] = RCA). */
    uint32_t r6 = 0;
    LOG("CMD3 SEND_RELATIVE_ADDR");
    rc = sdmmc_send_cmd(MMC_SEND_RELATIVE, 0, RESP_R6, 0, &r6);
    if (rc) return -50;
    g_card.rca = (uint16_t)(r6 >> 16);
    LOG_HEX("RCA", g_card.rca);

    /* CMD9 — get CSD using RCA. */
    LOG("CMD9 SEND_CSD");
    rc = sdmmc_send_cmd(MMC_SEND_CSD, ((uint32_t)g_card.rca) << 16, RESP_R2, 0, g_card.csd);
    if (rc) return -60;
    g_card.num_blocks = sdmmc_csd_blocks(g_card.csd);
    LOG_DEC("blocks", g_card.num_blocks);
    LOG_DEC("MB", g_card.num_blocks / 2048);

    /* CMD7 — select card. */
    LOG("CMD7 SELECT");
    rc = sdmmc_send_cmd(MMC_SELECT_CARD, ((uint32_t)g_card.rca) << 16, RESP_R1B, 0, NULL);
    if (rc) return -70;

    /* ACMD6 — switch to 4-bit bus. */
    LOG("ACMD6 SET_BUS_WIDTH=4");
    rc = sdmmc_app_cmd(SD_APP_SET_BUS_WIDTH, 2, RESP_R1, NULL);
    if (rc) return -80;
    SD(SD_BWDR) = 1;  /* 0=1bit, 1=4bit, 2=8bit */

    /* CMD16 — block length 512. */
    LOG("CMD16 SET_BLOCKLEN=512");
    rc = sdmmc_send_cmd(MMC_SET_BLOCKLEN, SDMMC_BLOCK_SIZE, RESP_R1, 0, NULL);
    if (rc) return -90;

    /* Bump module clock to 50 MHz from PLL_PERIPH (600 MHz / 12). High-
     * speed SD allows 50 MHz; if a card is stubborn we can dial back. */
    LOG("bump clock to ~50 MHz (PLL_PERIPH)");
    sdmmc_set_module_clock(50000000);
    if (sdmmc_update_card_clock()) return -91;

    g_card.initialised = 1;
    LOG("init OK");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Block read (CMD17, FIFO PIO)                                       */
/* ------------------------------------------------------------------ */

static int sdmmc_read_one_block(uint32_t lba, uint32_t *dst)
{
    /* Setup data transfer. */
    SD(SD_BKSR) = SDMMC_BLOCK_SIZE;
    SD(SD_BYCR) = SDMMC_BLOCK_SIZE;
    SD(SD_RISR) = 0xFFFFFFFFu;
    SD(SD_GCTL) |= GCTL_FIFO_RST;
    if (wait_clear(SD_GCTL, GCTL_FIFO_RST, 50000)) return -1;
    SD(SD_GCTL) |= GCTL_FIFO_AC_MOD;

    /* SDSC uses byte address, SDHC uses block address. */
    uint32_t arg = g_card.is_sdhc ? lba : (lba * SDMMC_BLOCK_SIZE);

    SD(SD_CAGR) = arg;
    SD(SD_CMDR) = CMDR_LOAD | (MMC_READ_SINGLE & 0x3F)
                | RESP_R1
                | CMDR_DATA_TRANS
                | CMDR_WAIT_PRG_END;

    /* Drain FIFO into dst. Each word = 4 bytes. */
    uint32_t words_left = SDMMC_BLOCK_SIZE / 4;
    uint32_t t0 = timer_read();
    while (words_left) {
        uint32_t risr = SD(SD_RISR);
        if (risr & RISR_ERR_MASK) {
            LOG_HEX("read RISR err", risr);
            return -2;
        }
        if (!(SD(SD_STAR) & STAR_FIFO_EMPTY)) {
            *dst++ = SD(SD_FIFO);
            words_left--;
            t0 = timer_read();
            continue;
        }
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 500000u) {
            LOG_HEX("read timeout STAR", SD(SD_STAR));
            LOG_HEX("read timeout RISR", SD(SD_RISR));
            return -3;
        }
    }

    /* Wait for DATA_OVER. */
    t0 = timer_read();
    while (!(SD(SD_RISR) & RISR_DATA_OVER)) {
        if (SD(SD_RISR) & RISR_ERR_MASK) return -4;
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 500000u) return -5;
    }
    SD(SD_RISR) = 0xFFFFFFFFu;
    return 0;
}

/* Multi-block read via CMD18 + auto-CMD12 stop. One command pulls all
 * `count` blocks; only one round-trip of card response latency, vs N
 * for repeated CMD17. */
static int sdmmc_read_multi(uint32_t lba, uint32_t count, uint32_t *dst)
{
    uint32_t total_bytes = count * SDMMC_BLOCK_SIZE;
    SD(SD_BKSR) = SDMMC_BLOCK_SIZE;
    SD(SD_BYCR) = total_bytes;
    SD(SD_RISR) = 0xFFFFFFFFu;
    SD(SD_GCTL) |= GCTL_FIFO_RST;
    if (wait_clear(SD_GCTL, GCTL_FIFO_RST, 50000)) return -1;
    SD(SD_GCTL) |= GCTL_FIFO_AC_MOD;

    uint32_t arg = g_card.is_sdhc ? lba : (lba * SDMMC_BLOCK_SIZE);

    SD(SD_CAGR) = arg;
    /* u-boot sunxi_mmc.c sets only DATA_EXPIRE | AUTO_STOP | WAIT_PRE_OVER
     * for multi-block reads, no explicit SEQ_MODE. Bit 11 (what we called
     * SEQ_MODE) has variant-specific meaning and tripping it on V3s seems
     * to corrupt the data. Match upstream. */
    SD(SD_CMDR) = CMDR_LOAD | (MMC_READ_MULTIPLE & 0x3F)
                | RESP_R1
                | CMDR_DATA_TRANS
                | CMDR_AUTO_STOP
                | CMDR_WAIT_PRG_END;

    uint32_t words_left = total_bytes / 4;
    uint32_t t0 = timer_read();
    while (words_left) {
        uint32_t risr = SD(SD_RISR);
        if (risr & RISR_ERR_MASK) {
            LOG_HEX("read multi RISR err", risr);
            return -2;
        }
        if (!(SD(SD_STAR) & STAR_FIFO_EMPTY)) {
            *dst++ = SD(SD_FIFO);
            words_left--;
            t0 = timer_read();
            continue;
        }
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 1000000u) {
            LOG_HEX("read multi timeout STAR", SD(SD_STAR));
            LOG_HEX("read multi timeout RISR", SD(SD_RISR));
            return -3;
        }
    }

    /* Wait for both DATA_OVER and the auto-CMD12 to complete. */
    t0 = timer_read();
    while ((SD(SD_RISR) & (RISR_DATA_OVER | RISR_AUTO_CMD_DONE))
           != (RISR_DATA_OVER | RISR_AUTO_CMD_DONE)) {
        if (SD(SD_RISR) & RISR_ERR_MASK) return -4;
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 1000000u) {
            LOG_HEX("read multi tail RISR", SD(SD_RISR));
            return -5;
        }
    }
    SD(SD_RISR) = 0xFFFFFFFFu;
    return 0;
}

int sdmmc_read_blocks(uint32_t lba, uint32_t count, void *dst)
{
    if (!g_card.initialised) return -1;
    if (((uintptr_t)dst) & 3u) return -2;
    if (count == 0) return 0;
    if (count == 1) {
        return sdmmc_read_one_block(lba, (uint32_t *)dst);
    }
    return sdmmc_read_multi(lba, count, (uint32_t *)dst);
}

/* ------------------------------------------------------------------ */
/* Block write (CMD24, FIFO PIO)                                      */
/* ------------------------------------------------------------------ */

static int sdmmc_write_one_block(uint32_t lba, const uint32_t *src)
{
    /* Match the multi-block path's entry guards: a previous CMD24/CMD25
     * may have left the data FSM still unwinding, or the card still in
     * its post-DAT0 programming-busy window. Without these gates,
     * back-to-back single-block writes can wedge silently. */
    if (wait_clear(SD_STAR, STAR_DATA_FSM_BUSY, 5000000)) return -7;
    {
        uint32_t t0 = timer_read();
        while (SD(SD_STAR) & STAR_CARD_DATA_BUSY) {
            if (ticks_to_us(timer_elapsed(t0, timer_read())) > 5000000u) return -8;
        }
    }

    SD(SD_BKSR) = SDMMC_BLOCK_SIZE;
    SD(SD_BYCR) = SDMMC_BLOCK_SIZE;
    SD(SD_GCTL) |= GCTL_FIFO_RST;
    if (wait_clear(SD_GCTL, GCTL_FIFO_RST, 50000)) return -1;
    SD(SD_GCTL) |= GCTL_FIFO_AC_MOD;
    SD(SD_RISR) = 0xFFFFFFFFu;

    uint32_t arg = g_card.is_sdhc ? lba : (lba * SDMMC_BLOCK_SIZE);

    SD(SD_CAGR) = arg;
    SD(SD_CMDR) = CMDR_LOAD | (MMC_WRITE_SINGLE & 0x3F)
                | RESP_R1
                | CMDR_DATA_TRANS
                | CMDR_WRITE
                | CMDR_WAIT_PRG_END;

    /* Push 128 words. Stall when FIFO is full. */
    uint32_t words_left = SDMMC_BLOCK_SIZE / 4;
    uint32_t t0 = timer_read();
    while (words_left) {
        uint32_t risr = SD(SD_RISR);
        if (risr & RISR_ERR_MASK) {
            LOG_HEX("write RISR err", risr);
            return -2;
        }
        if (!(SD(SD_STAR) & STAR_FIFO_FULL)) {
            SD(SD_FIFO) = *src++;
            words_left--;
            t0 = timer_read();
            continue;
        }
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 500000u) {
            LOG_HEX("write fifo timeout STAR", SD(SD_STAR));
            return -3;
        }
    }

    /* Wait DATA_OVER. */
    t0 = timer_read();
    while (!(SD(SD_RISR) & RISR_DATA_OVER)) {
        if (SD(SD_RISR) & RISR_ERR_MASK) return -4;
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 1000000u) return -5;
    }

    /* Wait for the card to leave programming-busy (DAT0 deasserts). */
    t0 = timer_read();
    while (SD(SD_STAR) & STAR_CARD_DATA_BUSY) {
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 5000000u) return -6;
    }
    SD(SD_RISR) = 0xFFFFFFFFu;
    return 0;
}

/* Multi-block write via CMD25 + auto-CMD12 stop. One command pushes
 * `count` blocks back-to-back; only one card-busy wait at the very
 * end (vs one between each CMD24). Sustained sequential write
 * throughput goes up roughly 10x.
 *
 * Same FIFO-PIO push pattern as the single-block path: stall when
 * FIFO is full, only fail on actual error bits in RISR. */
static int sdmmc_write_multi(uint32_t lba, uint32_t count, const uint32_t *src)
{
    uint32_t total_bytes = count * SDMMC_BLOCK_SIZE;

    /* Gate against leftover state from a previous CMD25. The single-shot
     * `sdmmc_send_cmd` path waits on STAR_DATA_FSM_BUSY; `write_multi`
     * punches CMDR directly so we have to do it ourselves. Without this,
     * back-to-back CMD25s can fire while the data FSM is still unwinding
     * the auto-CMD12 from the previous transaction, and the next FIFO
     * push or AUTO_CMD_DONE wait hangs. */
    if (wait_clear(SD_STAR, STAR_DATA_FSM_BUSY, 5000000)) {
        LOG("write multi: data FSM still busy from prior xfer");
        return -7;
    }
    /* Wait for any lingering card-side programming busy too. The previous
     * call's tail wait can return at the moment DAT0 deasserts, but the
     * card may immediately re-assert if there's an internal flush in
     * progress. */
    {
        uint32_t t0 = timer_read();
        while (SD(SD_STAR) & STAR_CARD_DATA_BUSY) {
            if (ticks_to_us(timer_elapsed(t0, timer_read())) > 5000000u) {
                LOG("write multi: card still busy from prior xfer");
                return -8;
            }
        }
    }

    SD(SD_BKSR) = SDMMC_BLOCK_SIZE;
    SD(SD_BYCR) = total_bytes;
    /* Reset the FIFO first, then clear RISR. Clearing RISR before the
     * reset is racy: the reset itself can set status bits as it
     * settles, leaving a stale AUTO_CMD_DONE / DATA_OVER from the prior
     * transaction visible to this one. */
    SD(SD_GCTL) |= GCTL_FIFO_RST;
    if (wait_clear(SD_GCTL, GCTL_FIFO_RST, 50000)) return -1;
    SD(SD_GCTL) |= GCTL_FIFO_AC_MOD;
    SD(SD_RISR) = 0xFFFFFFFFu;

    uint32_t arg = g_card.is_sdhc ? lba : (lba * SDMMC_BLOCK_SIZE);

    SD(SD_CAGR) = arg;
    SD(SD_CMDR) = CMDR_LOAD | (MMC_WRITE_MULTIPLE & 0x3F)
                | RESP_R1
                | CMDR_DATA_TRANS
                | CMDR_WRITE
                | CMDR_AUTO_STOP
                | CMDR_WAIT_PRG_END;

    /* Push every word — controller streams them through to the card
     * without per-block CMD overhead. */
    uint32_t words_left = total_bytes / 4;
    uint32_t t0 = timer_read();
    while (words_left) {
        uint32_t risr = SD(SD_RISR);
        if (risr & RISR_ERR_MASK) {
            LOG_HEX("write multi RISR err", risr);
            return -2;
        }
        if (!(SD(SD_STAR) & STAR_FIFO_FULL)) {
            SD(SD_FIFO) = *src++;
            words_left--;
            t0 = timer_read();
            continue;
        }
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 2000000u) {
            LOG_HEX("write multi fifo timeout STAR", SD(SD_STAR));
            return -3;
        }
    }

    /* Wait for both DATA_OVER and the auto-CMD12. */
    t0 = timer_read();
    while ((SD(SD_RISR) & (RISR_DATA_OVER | RISR_AUTO_CMD_DONE))
           != (RISR_DATA_OVER | RISR_AUTO_CMD_DONE)) {
        if (SD(SD_RISR) & RISR_ERR_MASK) return -4;
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 5000000u) {
            LOG_HEX("write multi tail RISR", SD(SD_RISR));
            return -5;
        }
    }

    /* Wait for the card to leave programming-busy. */
    t0 = timer_read();
    while (SD(SD_STAR) & STAR_CARD_DATA_BUSY) {
        if (ticks_to_us(timer_elapsed(t0, timer_read())) > 5000000u) return -6;
    }
    SD(SD_RISR) = 0xFFFFFFFFu;
    return 0;
}

int sdmmc_write_blocks(uint32_t lba, uint32_t count, const void *src)
{
    if (!g_card.initialised) return -1;
    if (((uintptr_t)src) & 3u) return -2;
    if (count == 0) return 0;
    if (count == 1) {
        return sdmmc_write_one_block(lba, (const uint32_t *)src);
    }
    return sdmmc_write_multi(lba, count, (const uint32_t *)src);
}
