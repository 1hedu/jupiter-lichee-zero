/*
 * Jupiter SDK — timer.c
 * Sunxi hardware timer 0, 24 MHz down-counter.
 * ARM generic timer traps via U-Boot `go` — use this instead.
 */
#include "jupiter.h"

#define TMR_BASE  0x01C20C00
#define TMR0_CTRL REG32(TMR_BASE + 0x10)
#define TMR0_INTV REG32(TMR_BASE + 0x14)
#define TMR0_CUR  REG32(TMR_BASE + 0x18)
#define TMR0_EN   BIT(0)
#define TMR0_RLD  BIT(1)
#define TMR0_24M  (1 << 2)

/* ---- CPU PLL: ensure 1.2 GHz ----
 * U-Boot on Lichee Pi Zero defaults to 1.008 GHz (N=28 K=3 M=2).
 * We bump to 1.2 GHz (N=25 K=2 M=1) for the ~19% extra headroom
 * that makes cycle-accurate emulators feasible. */
#define CCU_PLL_CPUX    REG32(0x01C20000)
#define CCU_CPU_CLK_SRC REG32(0x01C20050)

static void cpu_pll_1200mhz(void)
{
    uint32_t pll = CCU_PLL_CPUX;
    uint32_t n = ((pll >> 8) & 0x1F) + 1;
    uint32_t k = ((pll >> 4) & 0x3) + 1;
    uint32_t m = (pll & 0x3) + 1;
    if (24 * n * k / m >= 1200) return;  /* already fast enough */

    /* Switch CPU to 24 MHz oscillator while we reconfigure PLL */
    CCU_CPU_CLK_SRC = (CCU_CPU_CLK_SRC & ~(3u << 16)) | (1u << 16);
    for (volatile int i = 0; i < 1000; i++);

    /* PLL_CPUX: 24 MHz * 25 * 2 / 1 = 1200 MHz */
    CCU_PLL_CPUX = (1u << 31) | (24u << 8) | (1u << 4);
    while (!(CCU_PLL_CPUX & (1u << 28)));  /* wait for lock */

    /* Switch CPU back to PLL */
    CCU_CPU_CLK_SRC = (CCU_CPU_CLK_SRC & ~(3u << 16)) | (2u << 16);
    for (volatile int i = 0; i < 1000; i++);
}

void timer_init(void)
{
    cpu_pll_1200mhz();

    TMR0_CTRL = 0;
    TMR0_INTV = 0xFFFFFFFF;
    TMR0_CTRL = TMR0_24M | TMR0_RLD;
    TMR0_CTRL |= TMR0_EN;
}

uint32_t timer_read(void)
{
    return TMR0_CUR;
}

uint32_t timer_elapsed(uint32_t start, uint32_t end)
{
    return start - end;  /* down-counter */
}

uint32_t ticks_to_us(uint32_t ticks)
{
    return ticks / 24;
}

uint32_t ticks_to_ms(uint32_t ticks)
{
    return ticks / 24000;
}
