/*
 * Jupiter SDK — Si5351 programmable clock generator driver
 *
 * Configures Si5351A over I2C (TWI0: PB6=SCL, PB7=SDA) to output
 * exactly 7.670454 MHz on CLK0 for the YM3438 master clock.
 *
 * After programming, PB6/PB7 are released back to GPIO for the
 * YM3438 data bus (D6/D7).
 *
 * Crystal: 25 MHz
 * PLL A:   25 × (30 + 85227/125000) = 767.04540 MHz
 * MS0:     767045400 / 100 = 7670454.0 Hz (exact)
 */
#include "jupiter.h"

/* ---- TWI0 (I2C) registers ---- */
#define TWI0_BASE       0x01C2AC00
#define TWI0_ADDR       REG32(TWI0_BASE + 0x00)
#define TWI0_XADDR      REG32(TWI0_BASE + 0x04)
#define TWI0_DATA       REG32(TWI0_BASE + 0x08)
#define TWI0_CNTR       REG32(TWI0_BASE + 0x0C)
#define TWI0_STAT       REG32(TWI0_BASE + 0x10)
#define TWI0_CCR        REG32(TWI0_BASE + 0x14)
#define TWI0_SRST       REG32(TWI0_BASE + 0x18)
#define TWI0_EFR        REG32(TWI0_BASE + 0x1C)
#define TWI0_LCR        REG32(TWI0_BASE + 0x20)

/* TWI_CNTR bits */
#define TWI_INT_EN      (1 << 7)
#define TWI_BUS_EN      (1 << 6)
#define TWI_M_STA       (1 << 5)
#define TWI_M_STP       (1 << 4)
#define TWI_INT_FLAG    (1 << 3)
#define TWI_A_ACK       (1 << 2)

/* TWI status codes */
#define TWI_STAT_START      0x08
#define TWI_STAT_RSTART     0x10
#define TWI_STAT_ADDR_W_ACK 0x18
#define TWI_STAT_DATA_W_ACK 0x28

/* Si5351 I2C address */
#define SI5351_ADDR     0x60

/* ---- CCU gates for TWI0 ---- */
#define CCU_BUS_CLK_GATE3  REG32(CCU_BASE + 0x006C)
#define CCU_BUS_RST3       REG32(CCU_BASE + 0x02D8)
#define TWI0_GATE_BIT      (1 << 0)

/* ---- GPIO mux ---- */
#define PB_CFG0     REG32(PIO_BASE + 0x24)
#define PB_CFG1     REG32(PIO_BASE + 0x28)

/* Wait for TWI interrupt flag with timeout */
static int twi_wait(void)
{
    for (volatile int i = 0; i < 100000; i++) {
        if (TWI0_CNTR & TWI_INT_FLAG) return 0;
    }
    return -1;  /* timeout */
}

/* Send START condition */
static int twi_start(void)
{
    TWI0_CNTR = TWI_BUS_EN | TWI_M_STA | TWI_INT_FLAG;
    if (twi_wait()) return -1;
    uint32_t stat = TWI0_STAT & 0xFF;
    return (stat == TWI_STAT_START || stat == TWI_STAT_RSTART) ? 0 : -1;
}

/* Send STOP condition */
static void twi_stop(void)
{
    TWI0_CNTR = TWI_BUS_EN | TWI_M_STP | TWI_INT_FLAG;
    for (volatile int i = 0; i < 10000; i++) {
        if (!(TWI0_CNTR & TWI_M_STP)) break;
    }
}

/* Send one byte, return 0 if ACK received */
static int twi_send(uint8_t byte)
{
    TWI0_DATA = byte;
    TWI0_CNTR = TWI_BUS_EN | TWI_INT_FLAG;
    if (twi_wait()) return -1;
    uint32_t stat = TWI0_STAT & 0xFF;
    return (stat == TWI_STAT_ADDR_W_ACK || stat == TWI_STAT_DATA_W_ACK) ? 0 : -1;
}

/* Read one byte with ACK/NACK */
static uint8_t twi_recv(int ack)
{
    TWI0_CNTR = TWI_BUS_EN | TWI_INT_FLAG | (ack ? TWI_A_ACK : 0);
    twi_wait();
    return (uint8_t)TWI0_DATA;
}

/* Write a register on the Si5351 */
static int si5351_write(uint8_t reg, uint8_t val)
{
    if (twi_start()) return -1;
    if (twi_send(SI5351_ADDR << 1)) { twi_stop(); return -1; }
    if (twi_send(reg))              { twi_stop(); return -1; }
    if (twi_send(val))              { twi_stop(); return -1; }
    twi_stop();
    return 0;
}

/* Read a register from the Si5351 */
static uint8_t si5351_read(uint8_t reg)
{
    twi_start();
    twi_send(SI5351_ADDR << 1);       /* write mode */
    twi_send(reg);                     /* register address */
    twi_start();                       /* repeated start */
    twi_send((SI5351_ADDR << 1) | 1); /* read mode */
    uint8_t val = twi_recv(0);        /* NACK = last byte */
    twi_stop();
    return val;
}

/* ---- Si5351 register table for 7.670454 MHz ----
 *
 * PLL A:  a=30, b=85227, c=125000
 *   P1 = 128*30 + floor(128*85227/125000) - 512 = 3415 = 0x000D57
 *   P2 = 128*85227 - 125000*floor(128*85227/125000) = 34056 = 0x008508
 *   P3 = 125000 = 0x1E848
 *
 * MS0:   a=100, b=0, c=1 (integer, even — lowest jitter)
 *   P1 = 128*100 - 512 = 12288 = 0x003000
 *   P2 = 0, P3 = 1
 */
static const uint8_t si5351_regs[][2] = {
    /* Disable outputs */
    {   3, 0xFF },

    /* Power down all clocks */
    {  16, 0x80 },
    {  17, 0x80 },
    {  18, 0x80 },

    /* PLL A (MSNA): regs 26-33 */
    {  26, 0xE8 },   /* P3[15:8]  = 0xE8                     */
    {  27, 0x48 },   /* P3[7:0]   = 0x48   (P3=0x1E848)      */
    {  28, 0x00 },   /* P1[17:16] = 0                         */
    {  29, 0x0D },   /* P1[15:8]  = 0x0D                      */
    {  30, 0x57 },   /* P1[7:0]   = 0x57   (P1=0xD57=3415)   */
    {  31, 0x10 },   /* P3[19:16]<<4 | P2[19:16] = 0x10       */
    {  32, 0x85 },   /* P2[15:8]  = 0x85                      */
    {  33, 0x08 },   /* P2[7:0]   = 0x08   (P2=0x8508=34056) */

    /* Multisynth 0 (MS0): regs 42-49 */
    {  42, 0x00 },   /* P3[15:8]  = 0                         */
    {  43, 0x01 },   /* P3[7:0]   = 1      (P3=1)             */
    {  44, 0x00 },   /* R0_DIV=0(/1), MS0_DIVBY4=0, P1[17:16]=0 */
    {  45, 0x30 },   /* P1[15:8]  = 0x30                      */
    {  46, 0x00 },   /* P1[7:0]   = 0x00   (P1=0x3000=12288) */
    {  47, 0x00 },   /* P3[19:16]<<4 | P2[19:16] = 0          */
    {  48, 0x00 },   /* P2[15:8]  = 0                         */
    {  49, 0x00 },   /* P2[7:0]   = 0      (P2=0)             */

    /* CLK0: powered up, integer mode, PLL A, 8mA drive */
    {  16, 0x4D },   /* 0b01001101                             */

    /* PLL soft reset */
    { 177, 0xA0 },

    /* Enable CLK0 */
    {   3, 0xFE },
};

/* ---- Public API ---- */

void si5351_init(void)
{
    /* Enable TWI0 clock gate and de-assert reset */
    CCU_BUS_CLK_GATE3 |= TWI0_GATE_BIT;
    CCU_BUS_RST3      |= TWI0_GATE_BIT;

    /* Configure PB6=TWI0_SCK, PB7=TWI0_SDA (mux function 2) */
    uint32_t cfg0 = PB_CFG0;
    cfg0 &= ~(0x7 << 24);  /* PB6 */
    cfg0 |=  (0x2 << 24);  /* func 2 = TWI0_SCK */
    PB_CFG0 = cfg0;

    uint32_t cfg1 = PB_CFG1;
    cfg1 &= ~(0x7 << 28);  /* PB7 — wait, PB7 is in CFG0 bits 31:28 */
    /* Actually PB7 is bits [31:28] of PB_CFG0 (pins 0-7 in CFG0) */
    cfg0 = PB_CFG0;
    cfg0 &= ~(0x7U << 28); /* PB7 */
    cfg0 |=  (0x2U << 28); /* func 2 = TWI0_SDA */
    PB_CFG0 = cfg0;

    /* Software reset TWI */
    TWI0_SRST = 1;
    for (volatile int i = 0; i < 1000; i++);
    TWI0_SRST = 0;

    /* Set I2C clock: 100 kHz.
     * APB clock ≈ 24 MHz (before PLL changes) or 100+ MHz after.
     * Foscl = Fin / (2^CLK_N * (CLK_M+1) * 10)
     * For 24 MHz, 100kHz: CLK_N=1, CLK_M=11 → 24M / (2*12*10) = 100kHz
     * For higher APB: CLK_N=2, CLK_M=11 → Fin / (4*12*10) = Fin/480 */
    TWI0_CCR = (2 << 3) | 11;  /* CLK_N=2, CLK_M=11 */

    /* Enable bus */
    TWI0_CNTR = TWI_BUS_EN;

    /* Wait for Si5351 SYS_INIT to clear */
    uart_puts("[si5351] waiting for SYS_INIT...");
    for (int retry = 0; retry < 100; retry++) {
        uint8_t status = si5351_read(0);
        if (!(status & 0x80)) {
            uart_puts(" ready\n");
            goto ready;
        }
        for (volatile int d = 0; d < 10000; d++);
    }
    uart_puts(" TIMEOUT\n");
    return;

ready:
    /* Program all registers */
    uart_puts("[si5351] programming 7.670454 MHz...\n");
    for (uint32_t i = 0; i < sizeof(si5351_regs) / sizeof(si5351_regs[0]); i++) {
        int rc = si5351_write(si5351_regs[i][0], si5351_regs[i][1]);
        if (rc) {
            uart_puts("[si5351] write FAIL at reg ");
            uart_putdec(si5351_regs[i][0]);
            uart_puts("\n");
            return;
        }
    }

    uart_puts("[si5351] CLK0 = 7.670454 MHz active\n");

    /* Release PB6/PB7 back to GPIO output for YM3438 D6/D7 */
    cfg0 = PB_CFG0;
    cfg0 &= ~(0x7 << 24);  /* PB6 → output */
    cfg0 |=  (0x1 << 24);
    cfg0 &= ~(0x7U << 28); /* PB7 → output */
    cfg0 |=  (0x1U << 28);
    PB_CFG0 = cfg0;

    /* Disable TWI0 clock gate (save power, we're done with I2C) */
    CCU_BUS_CLK_GATE3 &= ~TWI0_GATE_BIT;
}
