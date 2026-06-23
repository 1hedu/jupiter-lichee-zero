/*
 * Jupiter SDK — YM3438 hardware driver
 *
 * Drives a real YM3438 (OPN2C) chip via GPIO bit-banging.
 *
 * Pin mapping (V3s Lichee Pi Zero → 74HCT245 → YM3438):
 *   PB0-PB7  → D0-D7  (8-bit data bus)
 *   PG0      → A0     (address bit 0)
 *   PG1      → A1     (address bit 1)
 *   PG2      → /WR    (write strobe, active low)
 *   PG3      → /CS    (chip select, active low)
 *   PG4      → /IC    (reset, active low)
 *
 * /RD tied high (never read from chip).
 * Clock from Si5351 CLK0 → 74HCT245 spare channel → φM pin.
 *
 * Write timing (from YM3438 datasheet):
 *   /CS and A0/A1 must be stable before /WR goes low.
 *   Data must be stable while /WR is low.
 *   /WR low pulse width: min 200ns (at 7.67 MHz clock).
 *   Between writes: address write needs ~17 clock cycles (~2.2µs),
 *   data write needs ~83 clock cycles (~10.8µs) before next write.
 */
#include "jupiter.h"
#include "pmu.h"

/* ---- GPIO register addresses ---- */
#define PB_CFG0     REG32(PIO_BASE + 0x24)
#define PB_CFG1     REG32(PIO_BASE + 0x28)
#define PB_DAT      REG32(PIO_BASE + 0x34)

#define PG_CFG0     REG32(PIO_BASE + 0xD8)
#define PG_DAT      REG32(PIO_BASE + 0xE8)

/* Control pin bit positions in PG_DAT */
#define PIN_A0      (1 << 0)   /* PG0 */
#define PIN_A1      (1 << 1)   /* PG1 */
#define PIN_nWR     (1 << 2)   /* PG2 — active low */
#define PIN_nCS     (1 << 3)   /* PG3 — active low */
#define PIN_nIC     (1 << 4)   /* PG4 — active low */

/* All control pins high (inactive) */
#define CTRL_IDLE   (PIN_A0 | PIN_A1 | PIN_nWR | PIN_nCS | PIN_nIC)

/* Nanosecond-ish delay via loop. At 1.2 GHz, ~1 iteration ≈ 3-4 ns.
 * 100 iterations ≈ 300-400 ns. Conservative for write pulse timing. */
static inline void ndelay(uint32_t iters)
{
    for (volatile uint32_t i = 0; i < iters; i++);
}

/* ---- Low-level bus write ---- */

/* Write one byte to the YM3438.
 * port: 0 = address reg port 0 (A1=0, A0=0)
 *       1 = data reg port 0    (A1=0, A0=1)
 *       2 = address reg port 1 (A1=1, A0=0)
 *       3 = data reg port 1    (A1=1, A0=1)
 */
static void ym3438_bus_write(uint8_t port, uint8_t data)
{
    /* Set data bus (PB0-PB7) */
    uint32_t pb = PB_DAT;
    pb = (pb & ~0xFF) | data;
    PB_DAT = pb;

    /* Set address lines + /CS low, /WR still high */
    uint32_t ctrl = PIN_nWR | PIN_nIC;  /* /WR high, /IC high (not reset) */
    if (port & 1) ctrl |= PIN_A0;
    if (port & 2) ctrl |= PIN_A1;
    /* /CS = low (not in ctrl) */
    PG_DAT = ctrl;

    /* Setup time: data and address stable before /WR falls */
    ndelay(50);

    /* Assert /WR low */
    ctrl &= ~PIN_nWR;
    PG_DAT = ctrl;

    /* /WR low pulse: min 200ns */
    ndelay(100);

    /* Deassert /WR high */
    ctrl |= PIN_nWR;
    PG_DAT = ctrl;

    /* Hold time */
    ndelay(50);

    /* Deassert /CS (all control idle) */
    PG_DAT = CTRL_IDLE;
}

/* ---- Clock source selection ---- */
#define YM_CLK_SI5351   0   /* External Si5351 at 7.670453 MHz (exact NTSC) */
#define YM_CLK_CCU_MCLK 1   /* CCU CSI_MCLK on PE1: 24MHz/3 = 8.0 MHz */
#define YM_CLK_EXT_XTAL 2   /* External 8 MHz crystal-osc module on phi-M
                             * pin. No V3s clock or pin involvement at
                             * all — chip free-runs from power-on. */

static uint8_t ym_clock_source = YM_CLK_CCU_MCLK;

/* Scale factor for F-Number compensation: 7.670453/8.0 = 0.9588
 * Fixed-point 0.16: 0.9588 * 65536 = 62837 */
#define FNUM_SCALE_8MHZ  62837

/* Cached F-Number high byte per channel for recombining after scale */
static uint8_t fnum_hi_cache[6];

/* PE registers for MCLK output */
#define PE_CFG0_REG  REG32(PIO_BASE + 0x90)

/* ---- CCU MCLK clock output ---- */
static void ym3438_ccu_mclk_init(void)
{
    /* Enable CSI bus clock gate (BUS_CLK_GATING1 bit 8) */
    REG32(CCU_BASE + 0x064) |= (1u << 8);

    /* De-assert CSI reset (BUS_SOFT_RST1 bit 8) */
    REG32(CCU_BASE + 0x2C4) |= (1u << 8);

    /* DRAM gate for CSI (bit 1) */
    REG32(CCU_BASE + 0x100) |= (1u << 1);

    /* CSI1_CLK_REG (CCU + 0x134) — PE1 is CSI1 MCLK, NOT CSI0!
     * Bit 15: MCLK gating = 1 (enable master clock output)
     * Bits [10:8]: source = 000 (OSC24M)
     * Bits [4:0]: divider M = 2 (divide by 3) → 24/3 = 8.0 MHz */
    REG32(CCU_BASE + 0x134) = (1u << 15) | (0u << 8) | (2u << 0);

    /* PE1 = function 2 (CSI_MCLK): bits [7:4] of PE_CFG0 */
    PE_CFG0_REG = (PE_CFG0_REG & ~(0xFu << 4)) | (0x2u << 4);

    /* Verify: dump registers and compare with known-working Linux values */
    uart_puts("[ym3438] MCLK verify (compare with Linux):\n");
    uart_puts("  0x064=0x"); uart_puthex(REG32(CCU_BASE + 0x064)); uart_puts(" (linux: 0x1110)\n");
    uart_puts("  0x2C4=0x"); uart_puthex(REG32(CCU_BASE + 0x2C4)); uart_puts(" (linux: 0x1111)\n");
    uart_puts("  0x100=0x"); uart_puthex(REG32(CCU_BASE + 0x100)); uart_puts(" (linux: 0x0002)\n");
    uart_puts("  0x134=0x"); uart_puthex(REG32(CCU_BASE + 0x134)); uart_puts(" (linux: 0x8002)\n");
    uart_puts("  PE_CFG0=0x"); uart_puthex(REG32(PIO_BASE + 0x90)); uart_puts(" (linux: 0x33333323)\n");
}

/* ---- Public API ---- */

void ym3438_hw_init(void)
{
    uart_puts("[ym3438] GPIO init...\n");

    /* Configure PB0-PB7 as GPIO output */
    PB_CFG0 = 0x11111111;  /* PB0-PB7 all function 1 (output) */

    /* Configure PG0-PG4 as GPIO output */
    uint32_t pg_cfg = PG_CFG0;
    pg_cfg &= ~0x000FFFFF;           /* clear PG0-PG4 */
    pg_cfg |=  0x00011111;            /* all output */
    PG_CFG0 = pg_cfg;

    /* All control lines idle (high) */
    PG_DAT = CTRL_IDLE;
    PB_DAT = PB_DAT & ~0xFF;  /* data bus = 0 */

    /* Start clock */
    if (ym_clock_source == YM_CLK_CCU_MCLK) {
        ym3438_ccu_mclk_init();
    } else if (ym_clock_source == YM_CLK_EXT_XTAL) {
        /* Crystal-osc module is wired directly to YM3438 phi-M; nothing
         * for the V3s to do. PE1 stays free, no CCU writes, no I2C. */
        uart_puts("[ym3438] using external 8 MHz crystal (no V3s setup)\n");
    } else {
        uart_puts("[ym3438] using Si5351 clock (configure I2C separately)\n");
    }

    /* Reset the YM3438: /IC low for >24 clock cycles (~3µs at 8 MHz) */
    uart_puts("[ym3438] resetting chip...\n");
    uint32_t ctrl = CTRL_IDLE & ~PIN_nIC;  /* /IC low */
    PG_DAT = ctrl;
    ndelay(2000);  /* ~6-8µs — well over the 24-cycle minimum */
    PG_DAT = CTRL_IDLE;  /* /IC high */
    ndelay(5000);  /* Wait for internal reset to complete */

    /* Clear F-Number cache */
    for (int i = 0; i < 6; i++) fnum_hi_cache[i] = 0;

    uart_puts("[ym3438] ready\n");
}

/* Select clock source. Call before ym3438_hw_init(). */
void ym3438_hw_set_clock(int source)
{
    ym_clock_source = source;
}

/* Write to YM3438 register (port 0).
 * addr: register address (0x00-0xFF)
 * data: register value
 */
void ym3438_hw_write(uint8_t addr, uint8_t data)
{
    ym3438_bus_write(0, addr);  /* A1=0, A0=0: address port 0 */
    ndelay(600);                /* ~2.2µs wait (17 clocks at 7.67MHz) */
    ym3438_bus_write(1, data);  /* A1=0, A0=1: data port 0 */
}

/* Write to YM3438 register (port 1, for channels 4-6).
 * addr: register address (0x00-0xFF)
 * data: register value
 */
void ym3438_hw_write_port1(uint8_t addr, uint8_t data)
{
    ym3438_bus_write(2, addr);  /* A1=1, A0=0: address port 1 */
    ndelay(600);
    ym3438_bus_write(3, data);  /* A1=1, A0=1: data port 1 */
}

/* Inter-write delay: call after ym3438_hw_write/write_port1 before
 * the next write. The YM3438 needs ~83 clock cycles (~10.8µs) to
 * process a data write. */
void ym3438_hw_busy_wait(void)
{
    ndelay(3500);  /* ~10-14µs */
}

/* ---- F-Number scaling for 8MHz clock ----
 *
 * VGM files assume 7.670453 MHz. At 8.0 MHz, the same F-Numbers
 * produce notes ~4.3% sharp. To compensate:
 *   F_new = F_orig × 7.670453 / 8.0 = F_orig × 0.9588
 *
 * F-Number is split across two registers per channel:
 *   $A0-$A2 (port 0) / $A0-$A2 (port 1): low 8 bits
 *   $A4-$A6 (port 0) / $A4-$A6 (port 1): high 3 bits [2:0] + block [5:3]
 *
 * The high byte ($A4-$A6) must be written FIRST (latches both).
 * We cache the high byte, and when the low byte is written, we
 * scale the combined 11-bit F-Number and write both corrected values.
 */
static void ym3438_scaled_write(uint8_t port, uint8_t addr, uint8_t data)
{
    /* Channel index: 0-2 for port 0, 3-5 for port 1 */
    int ch = (addr & 3) + (port ? 3 : 0);

    /* F-Number scaling kicks in for any 8 MHz path (CCU MCLK and the
     * direct 8 MHz crystal). Si5351 runs at the canonical 7.670453 MHz
     * so VGM data is already correct there. */
    int scale_8mhz = (ym_clock_source == YM_CLK_CCU_MCLK ||
                      ym_clock_source == YM_CLK_EXT_XTAL);
    if (scale_8mhz && addr >= 0xA4 && addr <= 0xA6) {
        /* High byte write — cache it, write as-is (latches on low byte write) */
        fnum_hi_cache[ch] = data;
        if (port == 0)
            ym3438_hw_write(addr, data);
        else
            ym3438_hw_write_port1(addr, data);
    } else if (scale_8mhz && addr >= 0xA0 && addr <= 0xA2) {
        /* Low byte write — combine with cached high, scale, write both */
        uint8_t hi = fnum_hi_cache[ch];
        uint32_t fnum = ((uint32_t)(hi & 0x07) << 8) | data;
        uint32_t block = (hi >> 3) & 0x07;

        /* Scale: F_new = F_orig * 62837 / 65536 */
        fnum = (fnum * FNUM_SCALE_8MHZ) >> 16;
        if (fnum > 0x7FF) fnum = 0x7FF;

        uint8_t new_hi = (block << 3) | ((fnum >> 8) & 0x07) | (hi & 0xC0);
        uint8_t new_lo = fnum & 0xFF;

        /* Write high first (re-latch), then low */
        if (port == 0) {
            ym3438_hw_write(addr + 4, new_hi);  /* $A4-$A6 */
            ndelay(600);
            ym3438_hw_write(addr, new_lo);       /* $A0-$A2 */
        } else {
            ym3438_hw_write_port1(addr + 4, new_hi);
            ndelay(600);
            ym3438_hw_write_port1(addr, new_lo);
        }
    } else {
        /* Non-frequency register — pass through */
        if (port == 0)
            ym3438_hw_write(addr, data);
        else
            ym3438_hw_write_port1(addr, data);
    }
}

/* VGM-style register write: port (0 or 1) + addr + data.
 * Includes the inter-write busy wait.
 * Automatically scales F-Numbers when running at 8.0 MHz. */
void ym3438_hw_vgm_write(uint8_t port, uint8_t addr, uint8_t data)
{
    ym3438_scaled_write(port, addr, data);
    ym3438_hw_busy_wait();
}
