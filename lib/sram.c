/*
 * Jupiter SDK — sram.c
 * SRAM_C (CedarVE working memory) remap to CPU + size probe.
 *
 * The V3s has on-die SRAM at 0x01D00000 normally owned by the Video Engine.
 * When VE isn't in use, remap it to the CPU for zero-wait-state access.
 *
 * SRAM_C sits behind the VE's bus domain. The bus bridge must be clocked
 * (PLL_VE + bus gate) or every access takes ~90 cycles through the
 * unclocked bridge.
 *
 * KEY: PLL_VE is at CCU+0x018, NOT CCU+0x058 (that's AHB2_CFG).
 * Confirmed via Linux /dev/mem register dump.
 */
#include "jupiter.h"

static uint32_t sram_c_size = 0;

void sram_init(void)
{
    /* 1. Enable PLL_VE at CCU+0x018 (clock source for VE bus bridge).
     *    bit31=enable, bit28=lock(ro), [15:8]=N, [3:0]=M.
     *    Reset default: 0x03006207 → N=98, M=7, disabled.
     *    Just enable the PLL with existing N/M → 24*98/8 = 294 MHz. */
    {
        uint32_t pll = REG32(CCU_BASE + 0x0018);
        uart_puts("[sram] PLL_VE reset=0x");
        uart_puthex(pll);

        pll |= (1u << 31);  /* Enable */
        REG32(CCU_BASE + 0x0018) = pll;

        /* Wait for PLL lock (bit 28) with timeout */
        uint32_t timeout = 100000;
        while (!(REG32(CCU_BASE + 0x0018) & (1u << 28)) && --timeout);

        uart_puts(" → 0x");
        uart_puthex(REG32(CCU_BASE + 0x0018));
        if (timeout == 0)
            uart_puts(" LOCK TIMEOUT!\n");
        else
            uart_puts(" locked\n");
    }

    /* 2. VE bus clock gate (CCU+0x060 bit 0) */
    REG32(CCU_BASE + 0x0060) |= (1u << 0);

    /* 3. De-assert VE reset (CCU+0x2C0 bit 0) — needed for bus bridge */
    REG32(CCU_BASE + 0x02C0) |= (1u << 0);

    /* 4. VE module clock: source=PLL_VE(0), gate enable */
    REG32(CCU_BASE + 0x013C) = (1u << 31) | (0u << 24);

    __asm__ volatile("dsb");

    uart_puts("[sram] BUS_GATE=0x");
    uart_puthex(REG32(CCU_BASE + 0x0060));
    uart_puts(" RST=0x");
    uart_puthex(REG32(CCU_BASE + 0x02C0));
    uart_puts(" VE_CLK=0x");
    uart_puthex(REG32(CCU_BASE + 0x013C));
    uart_puts("\n");

    /* 5. Remap SRAM_C to CPU */
    uint32_t ctrl = SRAM_CTRL_REG1;
    ctrl |= (1u << 24);
    SRAM_CTRL_REG1 = ctrl;
    __asm__ volatile("dsb");

    uart_puts("[sram] CTRL=0x");
    uart_puthex(SRAM_CTRL_REG1);
    uart_puts("\n");

    /* 6. Probe actual size */
    sram_c_size = sram_probe();

    uart_puts("[sram] SRAM_C @ 0x01D00000, size=");
    uart_putdec(sram_c_size / 1024);
    uart_puts("KB\n");
}

uint32_t sram_probe(void)
{
    volatile uint32_t *base = (volatile uint32_t *)SRAM_C_ADDR;

    base[0] = 0xDEADBEEF;
    __asm__ volatile("dsb");

    if (base[0] != 0xDEADBEEF) {
        uart_puts("[sram] WARNING: base readback failed!\n");
        return 0;
    }

    uint32_t size = 4096;

    for (uint32_t off = 4096; off < 512 * 1024; off += 4096) {
        volatile uint32_t *p = (volatile uint32_t *)(SRAM_C_ADDR + off);

        uint32_t pattern = 0xCAFE0000 | (off >> 12);
        *p = pattern;
        __asm__ volatile("dsb");

        if (base[0] != 0xDEADBEEF)
            break;

        if (*p != pattern)
            break;

        size = off + 4096;
    }

    return size;
}

uint32_t sram_size(void)
{
    return sram_c_size;
}
