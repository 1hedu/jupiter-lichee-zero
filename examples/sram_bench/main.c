/*
 * SRAM_C Diagnostic + Benchmark
 *
 * Phase 1: Dump SRAM controller regs, VE version, MMU page table entry
 * Phase 2: Verbose probe — print what happens at each 4KB boundary
 * Phase 3: Benchmark if SRAM is usable
 *
 * Build: make GAME=examples/sram_bench/main.c
 */
#include "jupiter.h"
#include "pmu.h"

#define SAMPLE_COUNT  512   /* small enough to fit even 4KB SRAM */

static int16_t ddr_buf[SAMPLE_COUNT];
static volatile uint32_t evict_buf[8192];

static void evict_l1(void)
{
    volatile uint32_t sum = 0;
    for (uint32_t i = 0; i < 8192; i++)
        sum += evict_buf[i];
    (void)sum;
}

static uint32_t bench_read(volatile int16_t *buf, uint32_t n)
{
    volatile int32_t sum = 0;
    __asm__ volatile("" ::: "memory");
    uint32_t t0 = pmu_cycles();
    for (uint32_t i = 0; i < n; i++)
        sum += buf[i];
    __asm__ volatile("" ::: "memory");
    return pmu_cycles() - t0;
}

static uint32_t bench_write(volatile int16_t *buf, uint32_t n)
{
    __asm__ volatile("" ::: "memory");
    uint32_t t0 = pmu_cycles();
    for (uint32_t i = 0; i < n; i++)
        buf[i] = (int16_t)i;
    __asm__ volatile("" ::: "memory");
    return pmu_cycles() - t0;
}

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();

    uart_puts("\n========================================\n");
    uart_puts("  SRAM_C Full Diagnostic\n");
    uart_puts("========================================\n\n");

    /* ---- Phase 0: Register dump BEFORE sram_init ---- */
    uart_puts("--- BEFORE sram_init ---\n");
    uart_puts("SRAM_CTRL+0x00=0x"); uart_puthex(REG32(0x01C00000)); uart_puts("\n");
    uart_puts("SRAM_CTRL+0x04=0x"); uart_puthex(REG32(0x01C00004)); uart_puts("\n");
    uart_puts("SRAM_CTRL+0x08=0x"); uart_puthex(REG32(0x01C00008)); uart_puts("\n");
    uart_puts("SRAM_CTRL+0x0C=0x"); uart_puthex(REG32(0x01C0000C)); uart_puts("\n");
    uart_puts("CCU BUS_GATE0=0x");  uart_puthex(REG32(0x01C20060)); uart_puts("\n");
    uart_puts("CCU BUS_RST0=0x");   uart_puthex(REG32(0x01C202C0)); uart_puts("\n");
    uart_puts("CCU VE_CLK=0x");     uart_puthex(REG32(0x01C2013C)); uart_puts("\n");
    uart_puts("CCU DRAM_GATE=0x");  uart_puthex(REG32(0x01C20100)); uart_puts("\n");
    uart_puts("CCU PLL_VE=0x");     uart_puthex(REG32(0x01C20058)); uart_puts("\n");
    /* NOTE: DO NOT read VE_VERSION (0x01C0E000) here — bus fault if clocks off */
    uart_puts("\n");

    /* ---- Phase 1: Init SRAM ---- */
    sram_init();

    /* ---- Phase 1b: Register dump AFTER sram_init ---- */
    uart_puts("\n--- AFTER sram_init ---\n");
    uart_puts("SRAM_CTRL+0x00=0x"); uart_puthex(REG32(0x01C00000)); uart_puts("\n");
    uart_puts("SRAM_CTRL+0x04=0x"); uart_puthex(REG32(0x01C00004)); uart_puts("\n");
    uart_puts("SRAM_CTRL+0x08=0x"); uart_puthex(REG32(0x01C00008)); uart_puts("\n");
    uart_puts("SRAM_CTRL+0x0C=0x"); uart_puthex(REG32(0x01C0000C)); uart_puts("\n");
    uart_puts("VE_VERSION=0x");     uart_puthex(REG32(0x01C0E000)); uart_puts("\n");
    uart_puts("PLL_VE=0x");         uart_puthex(REG32(0x01C20058)); uart_puts("\n");

    /* ---- Phase 2: Verbose probe ---- */
    uart_puts("\n--- Verbose SRAM probe ---\n");
    volatile uint32_t *base = (volatile uint32_t *)SRAM_C_ADDR;

    /* Write sentinel */
    base[0] = 0xDEADBEEF;
    __asm__ volatile("dsb");
    uart_puts("base[0] write 0xDEADBEEF, read=0x");
    uart_puthex(base[0]);
    uart_puts("\n");

    /* Test first 64KB in 4KB steps */
    for (uint32_t off = 0x1000; off <= 0x10000; off += 0x1000) {
        volatile uint32_t *p = (volatile uint32_t *)(SRAM_C_ADDR + off);
        uint32_t pattern = 0xCAFE0000 | (off >> 12);

        *p = pattern;
        __asm__ volatile("dsb");

        uint32_t got_base = base[0];
        uint32_t got_probe = *p;

        uart_puts("  +0x");
        uart_puthex(off);
        uart_puts(": wrote=0x");
        uart_puthex(pattern);
        uart_puts(" read=0x");
        uart_puthex(got_probe);
        uart_puts(" base=0x");
        uart_puthex(got_base);

        if (got_base != 0xDEADBEEF)
            uart_puts(" ALIASED!");
        if (got_probe != pattern)
            uart_puts(" READBACK_FAIL!");
        uart_puts("\n");

        /* Restore sentinel */
        base[0] = 0xDEADBEEF;
        __asm__ volatile("dsb");
    }

    /* ---- Phase 3: Single-word latency test ---- */
    uart_puts("\n--- Single word latency ---\n");
    {
        volatile uint32_t *sram_word = (volatile uint32_t *)SRAM_C_ADDR;
        volatile uint32_t *ddr_word = (volatile uint32_t *)ddr_buf;
        uint32_t dummy;

        /* DDR warm */
        *ddr_word = 0x12345678;
        dummy = *ddr_word;

        __asm__ volatile("dsb" ::: "memory");
        uint32_t t0 = pmu_cycles();
        dummy = *ddr_word;
        __asm__ volatile("dsb" ::: "memory");
        uint32_t t1 = pmu_cycles();
        uart_puts("DDR single read (warm): ");
        uart_putdec(t1 - t0);
        uart_puts(" cyc\n");

        /* DDR cold */
        evict_l1();
        __asm__ volatile("dsb" ::: "memory");
        t0 = pmu_cycles();
        dummy = *ddr_word;
        __asm__ volatile("dsb" ::: "memory");
        t1 = pmu_cycles();
        uart_puts("DDR single read (cold): ");
        uart_putdec(t1 - t0);
        uart_puts(" cyc\n");

        /* SRAM */
        *sram_word = 0x12345678;
        __asm__ volatile("dsb" ::: "memory");
        t0 = pmu_cycles();
        dummy = *sram_word;
        __asm__ volatile("dsb" ::: "memory");
        t1 = pmu_cycles();
        uart_puts("SRAM single read:       ");
        uart_putdec(t1 - t0);
        uart_puts(" cyc\n");

        (void)dummy;
    }

    /* ---- Phase 4: Benchmark (using only confirmed SRAM size) ---- */
    uint32_t sz = sram_size();
    uint32_t usable = (sz >= SAMPLE_COUNT * 2) ? SAMPLE_COUNT : sz / 2;

    if (usable > 0) {
        uart_puts("\n--- Benchmark (");
        uart_putdec(usable);
        uart_puts(" samples) ---\n");

        volatile int16_t *sram = (volatile int16_t *)SRAM_C_ADDR;
        volatile int16_t *ddr  = (volatile int16_t *)ddr_buf;
        uint32_t c;

        /* Write */
        bench_write(ddr, usable);
        c = bench_write(ddr, usable);
        uart_puts("Write DDR(warm): ");
        uart_putdec(c); uart_puts(" cyc  (");
        uart_putdec(c / usable); uart_puts("/s)\n");

        c = bench_write(sram, usable);
        uart_puts("Write SRAM:      ");
        uart_putdec(c); uart_puts(" cyc  (");
        uart_putdec(c / usable); uart_puts("/s)\n");

        /* Read */
        bench_read(ddr, usable);
        c = bench_read(ddr, usable);
        uart_puts("Read  DDR(warm): ");
        uart_putdec(c); uart_puts(" cyc  (");
        uart_putdec(c / usable); uart_puts("/s)\n");

        c = bench_read(sram, usable);
        uart_puts("Read  SRAM:      ");
        uart_putdec(c); uart_puts(" cyc  (");
        uart_putdec(c / usable); uart_puts("/s)\n");
    }

    uart_puts("\n========================================\n");
    uart_puts("  Diagnostic complete.\n");
    uart_puts("========================================\n");

    while (1);
    return 0;
}
