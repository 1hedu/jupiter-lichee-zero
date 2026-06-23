/*
 * Jupiter SDK — Hardware Diagnostic + Benchmark Suite v2
 *
 * Exhaustive hardware characterization. Probes every CP15 register,
 * tests every cache configuration, measures every memory pattern.
 * No more guessing.
 *
 * Build: make GAME=examples/benchmark/main.c
 */
#include "jupiter.h"

#define VOLATILE_USE(x) __asm__ volatile("" :: "r"(x))
#define BARRIER()       __asm__ volatile("" ::: "memory")

/* Scratch area: 0x41800000–0x41FFFFFF (8MB, in cached DRAM region) */
#define SCRATCH_ADDR    0x41800000
#define SCRATCH         ((uint32_t *)SCRATCH_ADDR)

/* ================================================================
 *  HELPERS
 * ================================================================ */

static void hex32(uint32_t v) { uart_puts("0x"); uart_puthex(v); }


static void section(const char *s)
{
    uart_puts("\n========== ");
    uart_puts(s);
    uart_puts(" ==========\n");
}

/* Quick latency test: pointer chase 10K ops through 4KB (256 words).
 * If D-cache works: ~3-4 cyc/op. If DRAM: ~270 cyc/op. */
static uint32_t quick_latency_test(uint32_t *buf, uint32_t n)
{
    /* Build sequential chase */
    for (uint32_t i = 0; i < n; i++)
        buf[i] = (i + 1) % n;

    /* Warm: chase once through */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < n; i++) idx = buf[idx];
    VOLATILE_USE(idx);

    /* Measure */
    idx = 0;
    uint32_t iters = 10000;
    uint32_t t0 = timer_read();
    for (uint32_t i = 0; i < iters; i++)
        idx = buf[idx];
    uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
    VOLATILE_USE(idx);
    return (us * 1200) / iters;  /* cycles per op */
}

/* Quick bandwidth test: sequential read 32KB. */
static uint32_t quick_bw_read(volatile uint32_t *buf, uint32_t words)
{
    uint32_t sum = 0;
    uint32_t t0 = timer_read();
    for (uint32_t i = 0; i < words; i++) sum += buf[i];
    uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
    VOLATILE_USE(sum);
    return us;
}

/* Quick bandwidth test: sequential write 32KB. */
static uint32_t quick_bw_write(uint32_t *buf, uint32_t words)
{
    uint32_t t0 = timer_read();
    for (uint32_t i = 0; i < words; i++) buf[i] = i;
    BARRIER();
    uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
    return us;
}


/* ================================================================
 *  PART 1: CP15 Register Dump
 * ================================================================ */

static void dump_cp15(void)
{
    section("PART 1: CP15 Register Dump");
    uint32_t v;

    /* SCTLR — System Control Register */
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(v));
    uart_puts("SCTLR  = "); hex32(v); uart_puts("\n");
    uart_puts("  M(MMU)=");    uart_putdec((v >> 0) & 1);
    uart_puts(" A(align)=");   uart_putdec((v >> 1) & 1);
    uart_puts(" C(Dcache)=");  uart_putdec((v >> 2) & 1);
    uart_puts(" I(Icache)=");  uart_putdec((v >> 12) & 1);
    uart_puts(" Z(branch)=");  uart_putdec((v >> 11) & 1);
    uart_puts(" TRE=");        uart_putdec((v >> 28) & 1);
    uart_puts(" AFE=");        uart_putdec((v >> 29) & 1);
    uart_puts("\n");

    /* ACTLR — Auxiliary Control Register (implementation-specific) */
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(v));
    uart_puts("ACTLR  = "); hex32(v); uart_puts("\n");
    uart_puts("  SMP(bit6)="); uart_putdec((v >> 6) & 1);
    uart_puts(" L1pctl(bit13:12)="); uart_putdec((v >> 13) & 1);
    uart_puts("\n");
    if (!((v >> 6) & 1)) {
        uart_puts("  *** SMP bit is CLEAR! On Cortex-A7, SMP must be set\n");
        uart_puts("  *** for D-cache to function, even on single-core.\n");
    }

    /* CTR — Cache Type Register */
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(v));
    uart_puts("CTR    = "); hex32(v); uart_puts("\n");
    uart_puts("  IminLine="); uart_putdec(4 << (v & 0xF));
    uart_puts("B DminLine="); uart_putdec(4 << ((v >> 16) & 0xF));
    uart_puts("B\n");

    /* CLIDR — Cache Level ID Register */
    __asm__ volatile("mrc p15, 1, %0, c0, c0, 1" : "=r"(v));
    uart_puts("CLIDR  = "); hex32(v); uart_puts("\n");
    uint32_t l1type = v & 7;
    uart_puts("  L1: ");
    if (l1type == 0) uart_puts("none");
    else if (l1type == 1) uart_puts("I-cache only");
    else if (l1type == 2) uart_puts("D-cache only");
    else if (l1type == 3) uart_puts("separate I+D");
    else if (l1type == 4) uart_puts("unified");
    uart_puts("\n");
    uint32_t l2type = (v >> 3) & 7;
    uart_puts("  L2: ");
    if (l2type == 0) uart_puts("none");
    else if (l2type == 4) uart_puts("unified");
    else { uart_puts("type="); uart_putdec(l2type); }
    uart_puts("\n");

    /* CCSIDR for L1 D-cache */
    __asm__ volatile("mcr p15, 2, %0, c0, c0, 0" :: "r"(0)); /* select L1 data */
    __asm__ volatile("isb");
    __asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(v));
    uart_puts("CCSIDR(L1D) = "); hex32(v); uart_puts("\n");
    {
        uint32_t line = 1 << ((v & 7) + 4);     /* log2 line size in words, +4 */
        uint32_t assoc = ((v >> 3) & 0x3FF) + 1;
        uint32_t sets = ((v >> 13) & 0x7FFF) + 1;
        uint32_t size = line * assoc * sets;
        uart_puts("  line="); uart_putdec(line);
        uart_puts("B ways="); uart_putdec(assoc);
        uart_puts(" sets="); uart_putdec(sets);
        uart_puts(" total="); uart_putdec(size / 1024);
        uart_puts("KB\n");
    }

    /* CCSIDR for L1 I-cache */
    __asm__ volatile("mcr p15, 2, %0, c0, c0, 0" :: "r"(1)); /* select L1 instruction */
    __asm__ volatile("isb");
    __asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(v));
    uart_puts("CCSIDR(L1I) = "); hex32(v); uart_puts("\n");

    /* CCSIDR for L2 (if exists) */
    __asm__ volatile("mcr p15, 2, %0, c0, c0, 0" :: "r"(2)); /* select L2 */
    __asm__ volatile("isb");
    __asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(v));
    uart_puts("CCSIDR(L2)  = "); hex32(v); uart_puts("\n");

    /* TTBR0 */
    __asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(v));
    uart_puts("TTBR0  = "); hex32(v); uart_puts("\n");

    /* DACR */
    __asm__ volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(v));
    uart_puts("DACR   = "); hex32(v); uart_puts("\n");

    /* TTBCR */
    __asm__ volatile("mrc p15, 0, %0, c2, c0, 2" : "=r"(v));
    uart_puts("TTBCR  = "); hex32(v); uart_puts("\n");

    /* Read back a few page table entries */
    uart_puts("\nPage table entries:\n");
    uint32_t ttbr;
    __asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr));
    volatile uint32_t *pt = (volatile uint32_t *)(ttbr & ~0x3FFF);

    uart_puts("  [0x000] 0x00000000 periph: "); hex32(pt[0x000]); uart_puts("\n");
    uart_puts("  [0x010] 0x01000000 DE2:    "); hex32(pt[0x010]); uart_puts("\n");
    uart_puts("  [0x400] 0x40000000 code:   "); hex32(pt[0x400]); uart_puts("\n");
    uart_puts("  [0x410] 0x41000000 code+:  "); hex32(pt[0x410]); uart_puts("\n");
    uart_puts("  [0x418] 0x41800000 scratch:"); hex32(pt[0x418]); uart_puts("\n");
    uart_puts("  [0x420] 0x42000000 FB0:    "); hex32(pt[0x420]); uart_puts("\n");
    uart_puts("  [0x422] 0x42200000 FB1:    "); hex32(pt[0x422]); uart_puts("\n");
}


/* ================================================================
 *  PART 2: D-Cache Enable Experiments
 * ================================================================ */

static void try_enable_dcache(const char *label, int set_smp)
{
    uart_puts("\n>> Trying: ");
    uart_puts(label);
    uart_puts("\n");

    /* Step 1: Disable D-cache and MMU cleanly */
    uint32_t sctlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    sctlr &= ~(1 << 2); /* C=0 */
    sctlr &= ~(1 << 0); /* M=0 */
    __asm__ volatile("dsb");
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(sctlr));
    __asm__ volatile("dsb");
    __asm__ volatile("isb");

    /* Step 2: Set/clear SMP bit in ACTLR */
    if (set_smp) {
        uint32_t actlr;
        __asm__ volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(actlr));
        actlr |= (1 << 6); /* SMP bit */
        __asm__ volatile("mcr p15, 0, %0, c1, c0, 1" :: "r"(actlr));
        __asm__ volatile("isb");
    }

    /* Step 3: Invalidate entire D-cache (DCISW, all sets/ways) */
    {
        __asm__ volatile("mcr p15, 2, %0, c0, c0, 0" :: "r"(0)); /* CSSELR = L1D */
        __asm__ volatile("isb");

        uint32_t ccsidr;
        __asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(ccsidr));

        uint32_t log2ls = (ccsidr & 7) + 4;
        uint32_t sets = ((ccsidr >> 13) & 0x7FFF) + 1;
        uint32_t ways = ((ccsidr >> 3) & 0x3FF) + 1;
        uint32_t wshift = (ways <= 1) ? 0 : __builtin_clz(ways - 1);

        for (uint32_t w = 0; w < ways; w++)
            for (uint32_t s = 0; s < sets; s++) {
                uint32_t val = (w << wshift) | (s << log2ls);
                __asm__ volatile("mcr p15, 0, %0, c7, c6, 2" :: "r"(val));
            }
    }
    __asm__ volatile("dsb");

    /* Step 4: Invalidate TLB */
    __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" :: "r"(0));
    __asm__ volatile("dsb");
    __asm__ volatile("isb");

    /* Step 5: Enable MMU, then D-cache */
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    sctlr |= (1 << 0);  /* M = MMU on */
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(sctlr));
    __asm__ volatile("dsb");
    __asm__ volatile("isb");

    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    sctlr |= (1 << 2);  /* C = D-cache on */
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(sctlr));
    __asm__ volatile("dsb");
    __asm__ volatile("isb");

    /* Verify */
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    uart_puts("   SCTLR after: "); hex32(sctlr);
    uart_puts(" M="); uart_putdec(sctlr & 1);
    uart_puts(" C="); uart_putdec((sctlr >> 2) & 1);
    uart_puts("\n");

    uint32_t actlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(actlr));
    uart_puts("   ACTLR after: "); hex32(actlr);
    uart_puts(" SMP="); uart_putdec((actlr >> 6) & 1);
    uart_puts("\n");

    /* Quick latency test */
    uint32_t cyc = quick_latency_test(SCRATCH, 256);
    uart_puts("   Latency (1KB chase): ~");
    uart_putdec(cyc);
    uart_puts(" cyc/op ");
    if (cyc < 20) uart_puts("[CACHED!]");
    else if (cyc < 50) uart_puts("[partial cache?]");
    else uart_puts("[uncached/DRAM]");
    uart_puts("\n");

    /* Quick read bandwidth */
    uint32_t us_r = quick_bw_read((volatile uint32_t *)SCRATCH, 8192);
    uint32_t mbps = (32768) / (us_r > 0 ? us_r : 1);
    uart_puts("   Read BW (32KB): ");
    uart_putdec(mbps);
    uart_puts(" MB/s ");
    if (mbps > 500) uart_puts("[CACHED!]");
    else uart_puts("[DRAM speed]");
    uart_puts("\n");

    /* Quick write bandwidth */
    uint32_t us_w = quick_bw_write(SCRATCH, 8192);
    uint32_t mbps_w = (32768) / (us_w > 0 ? us_w : 1);
    uart_puts("   Write BW (32KB): ");
    uart_putdec(mbps_w);
    uart_puts(" MB/s\n");
}

static void part2_dcache_experiments(void)
{
    section("PART 2: D-Cache Enable Experiments");

    uart_puts("Testing whether SMP bit in ACTLR fixes D-cache...\n");

    /* Attempt 1: Current method (what mmu_init does — no SMP) */
    try_enable_dcache("mmu_init style (no SMP)", 0);

    /* Attempt 2: With SMP bit set */
    try_enable_dcache("SMP=1 then enable cache", 1);

    /* Show which one worked */
    uart_puts("\nIf SMP=1 shows [CACHED!] above, that was the fix.\n");
}


/* ================================================================
 *  PART 3: Memory Latency Sweep (after best config found)
 * ================================================================ */

static void part3_latency_sweep(void)
{
    section("PART 3: Memory Latency Sweep");

    uart_puts("Pointer-chase latency at various buffer sizes:\n");

    uint32_t sizes[] = { 256, 1024, 2048, 4096, 8192, 16384, 32768,
                          65536, 131072, 262144, 524288, 1048576 };
    const char *labels[] = { "  1KB", "  4KB", "  8KB", " 16KB", " 32KB", " 64KB",
                              "128KB", "256KB", "512KB", "  1MB", "  2MB", "  4MB" };

    for (int i = 0; i < 12; i++) {
        uint32_t n = sizes[i];
        uint32_t *buf = SCRATCH;

        /* Build chase */
        for (uint32_t j = 0; j < n; j++)
            buf[j] = (j + 1) % n;

        /* Warm */
        uint32_t idx = 0;
        for (uint32_t j = 0; j < n * 2; j++) idx = buf[idx];
        VOLATILE_USE(idx);

        /* Measure */
        uint32_t iters = (n < 4096) ? 100000 : (n < 65536) ? 50000 : 10000;
        idx = 0;
        uint32_t t0 = timer_read();
        for (uint32_t j = 0; j < iters; j++) idx = buf[idx];
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        VOLATILE_USE(idx);

        uint32_t cyc = (us * 1200) / iters;
        uart_puts("  ");
        uart_puts(labels[i]);
        uart_puts(": ~");
        uart_putdec(cyc);
        uart_puts(" cyc/op\n");
    }
}


/* ================================================================
 *  PART 4: Bandwidth Sweep
 * ================================================================ */

static void part4_bandwidth(void)
{
    section("PART 4: Read/Write Bandwidth");

    uint32_t sizes[] = { 4096, 16384, 32768, 65536, 262144, 524288 };
    const char *labels[] = { "  4KB", " 16KB", " 32KB", " 64KB", "256KB", "512KB" };

    uart_puts("Sequential READ:\n");
    for (int s = 0; s < 6; s++) {
        uint32_t bytes = sizes[s];
        uint32_t words = bytes / 4;
        volatile uint32_t *p = (volatile uint32_t *)SCRATCH;

        uint32_t sum = 0;
        for (uint32_t i = 0; i < words; i++) sum += p[i];
        VOLATILE_USE(sum);

        uint32_t reps = (bytes <= 32768) ? 100 : 10;
        uint32_t total = bytes * reps;
        uint32_t t0 = timer_read();
        for (uint32_t r = 0; r < reps; r++) {
            sum = 0;
            for (uint32_t i = 0; i < words; i++) sum += p[i];
            VOLATILE_USE(sum);
        }
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));

        uart_puts("  ");
        uart_puts(labels[s]);
        uart_puts(": ");
        if (us > 0) uart_putdec(total / us); else uart_puts("?");
        uart_puts(" MB/s (");
        uart_putdec(us);
        uart_puts(" us)\n");
    }

    uart_puts("Sequential WRITE:\n");
    for (int s = 0; s < 6; s++) {
        uint32_t bytes = sizes[s];
        uint32_t words = bytes / 4;
        uint32_t *p = SCRATCH;

        uint32_t reps = (bytes <= 32768) ? 100 : 10;
        uint32_t total = bytes * reps;
        uint32_t t0 = timer_read();
        for (uint32_t r = 0; r < reps; r++) {
            for (uint32_t i = 0; i < words; i++) p[i] = i;
            BARRIER();
        }
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));

        uart_puts("  ");
        uart_puts(labels[s]);
        uart_puts(": ");
        if (us > 0) uart_putdec(total / us); else uart_puts("?");
        uart_puts(" MB/s (");
        uart_putdec(us);
        uart_puts(" us)\n");
    }

    uart_puts("NEON memset:\n");
    for (int s = 0; s < 6; s++) {
        uint32_t bytes = sizes[s];
        uint32_t reps = (bytes <= 32768) ? 100 : 10;
        uint32_t total = bytes * reps;
        uint32_t t0 = timer_read();
        for (uint32_t r = 0; r < reps; r++)
            memset32_neon(SCRATCH_ADDR, 0xDEADBEEF, bytes);
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));

        uart_puts("  ");
        uart_puts(labels[s]);
        uart_puts(": ");
        if (us > 0) uart_putdec(total / us); else uart_puts("?");
        uart_puts(" MB/s\n");
    }
}


/* ================================================================
 *  PART 5: Cache Pollution Test
 * ================================================================ */

static void part5_pollution(void)
{
    section("PART 5: Cache Pollution (lookup through write sweep)");

    static uint32_t lut[256];
    static uint8_t fmap[1024];
    for (uint32_t i = 0; i < 256; i++) lut[i] = i * 0x01010101;
    for (uint32_t i = 0; i < 1024; i++) fmap[i] = (uint8_t)(i & 255);

    uint32_t wsizes[] = { 4096, 32768, 65536, 131072, 262144, 524288 };
    const char *wl[] = { "  4KB", " 32KB", " 64KB", "128KB", "256KB", "512KB" };

    uart_puts("2 lookups (map+LUT) per pixel, varying FB size:\n");
    for (int s = 0; s < 6; s++) {
        uint32_t *wbuf = (uint32_t *)FB0_ADDR;
        uint32_t npix = wsizes[s] / 4;

        /* Warm LUT+map */
        uint32_t w = 0;
        for (uint32_t i = 0; i < 256; i++) w += lut[i];
        for (uint32_t i = 0; i < 1024; i++) w += fmap[i];
        VOLATILE_USE(w);

        uint32_t t0 = timer_read();
        for (uint32_t i = 0; i < npix; i++) {
            uint8_t tile = fmap[i & 1023];
            wbuf[i] = lut[tile];
        }
        BARRIER();
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));

        uart_puts("  write=");
        uart_puts(wl[s]);
        uart_puts(": ");
        uart_putdec(us);
        uart_puts(" us (");
        if (us > 0 && npix > 0) uart_putdec(us * 1200 / npix);
        uart_puts(" cyc/px)\n");
    }
}


/* ================================================================
 *  PART 6: ARM/NEON Transfer + Function Call
 * ================================================================ */

static void part6_misc(void)
{
    section("PART 6: ARM<->NEON Transfer");

    uint32_t iters = 100000;

    /* VDUP */
    {
        uint32_t val = 0xDEADBEEF;
        uint32_t t0 = timer_read();
        for (uint32_t i = 0; i < iters; i++) {
            __asm__ volatile("vdup.32 q0, %0\nvdup.32 q1, %0\n"
                             "vdup.32 q2, %0\nvdup.32 q3, %0\n"
                             :: "r"(val) : "q0","q1","q2","q3");
        }
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  VDUP x4:  ~"); uart_putdec(us * 1200 / (iters * 4));
        uart_puts(" cyc/op\n");
    }

    /* VMOV lane insert */
    {
        uint32_t val = 0xDEADBEEF;
        uint32_t t0 = timer_read();
        for (uint32_t i = 0; i < iters; i++) {
            __asm__ volatile("vmov.32 d0[0], %0\nvmov.32 d0[1], %0\n"
                             "vmov.32 d1[0], %0\nvmov.32 d1[1], %0\n"
                             :: "r"(val) : "q0");
        }
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  VMOV.32:  ~"); uart_putdec(us * 1200 / (iters * 4));
        uart_puts(" cyc/op\n");
    }

    section("PART 6b: Function Call Overhead");

    /* 8-arg call overhead (noinline) */
    extern void iso_scanline(uint32_t *, const uint8_t *, const uint32_t *,
                              int32_t, int32_t, int32_t, int32_t, uint32_t);
    static uint8_t dummy_map[1024];
    static uint32_t dummy_lut[10];
    {
        uint32_t reps = 100;
        uint32_t t0 = timer_read();
        for (uint32_t r = 0; r < reps; r++) {
            for (uint32_t sy = 0; sy < 136; sy++) {
                uint32_t *row = (uint32_t *)FB0_ADDR + sy * 960;
                iso_scanline(row, dummy_map, dummy_lut, 0, 0, 100, 100, 240);
            }
        }
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  iso_scanline x136: ");
        uart_putdec(us / reps);
        uart_puts(" us/frame\n");
    }
}


/* ================================================================
 *  PART 7: Practical Rendering Patterns
 * ================================================================ */

static void part7_rendering(void)
{
    section("PART 7: Rendering Patterns");

    uint32_t fb_bytes = LCD_W * LCD_H * 4;

    /* Flat NEON fill */
    {
        uint32_t t0 = timer_read();
        memset32_neon(FB0_ADDR, 0xFF102030, fb_bytes);
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  NEON flat fill 480x272:   ");
        uart_putdec(us); uart_puts(" us (");
        if (us > 0) uart_putdec(fb_bytes / us);
        uart_puts(" MB/s)\n");
    }

    /* dcache flush */
    {
        memset32_neon(FB0_ADDR, 0xFF102030, fb_bytes);
        uint32_t t0 = timer_read();
        dcache_clean_range(FB0_ADDR, fb_bytes);
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  dcache_clean 480x272:     "); uart_putdec(us); uart_puts(" us\n");
    }

    /* Full-res scatter r+w (isometric model) */
    {
        static uint8_t tmap[1024];
        static uint32_t tlut[10];
        for (int i = 0; i < 1024; i++) tmap[i] = (uint8_t)(i & 4);
        for (int i = 0; i < 10; i++) tlut[i] = 0xFF000000 | (i * 0x112233);

        uint32_t *fb = (uint32_t *)FB0_ADDR;
        uint32_t npix = LCD_W * LCD_H;
        int32_t u = 0, v = 0, du = 180, dv = 95;

        uint32_t t0 = timer_read();
        for (uint32_t i = 0; i < npix; i++) {
            uint32_t tx = (uint32_t)(u >> 12) & 31;
            uint32_t ty = (uint32_t)(v >> 12) & 31;
            uint8_t tile = tmap[(ty << 5) | tx];
            fb[i] = tlut[tile * 2 + ((tx ^ ty) & 1)];
            u += du; v += dv;
        }
        BARRIER();
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  Scatter r+w 480x272:      ");
        uart_putdec(us); uart_puts(" us (");
        if (us > 0) uart_putdec(us * 1200 / npix);
        uart_puts(" cyc/px)\n");
    }

    /* Half-res via iso_scanline + memcpy doubling */
    {
        static uint8_t tmap[1024];
        static uint32_t tlut[10];
        for (int i = 0; i < 1024; i++) tmap[i] = (uint8_t)(i & 4);
        for (int i = 0; i < 10; i++) tlut[i] = 0xFF000000 | (i * 0x112233);

        uint32_t *fb = (uint32_t *)FB0_ADDR;
        int32_t u_row = 0, v_row = 0;

        uint32_t t0 = timer_read();
        for (uint32_t sy = 0; sy < LCD_H / 2; sy++) {
            uint32_t *row = fb + (sy * 2) * LCD_W;
            iso_scanline(row, tmap, tlut, u_row, v_row, 360, 190, LCD_W / 2);
            memcpy_neon(row + LCD_W, row, LCD_W * 4);
            u_row += 380; v_row += 360;
        }
        BARRIER();
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  iso_scanline 240x136 dbl: "); uart_putdec(us); uart_puts(" us\n");
    }

    /* Sprite blit: 25 sprites */
    {
        static uint32_t spr[32 * 32];
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++)
                spr[y*32+x] = ((x-16)*(x-16)+(y-16)*(y-16) < 200) ? 0xFFFF8800 : 0;

        memset32_neon(OVL_ADDR, 0, LCD_W * LCD_H * 4);
        volatile uint32_t *ovl = (volatile uint32_t *)OVL_ADDR;

        uint32_t reps = 100;
        uint32_t t0 = timer_read();
        for (uint32_t r = 0; r < reps; r++)
            for (int i = 0; i < 25; i++)
                sprite_blit(ovl, LCD_W, spr, 32, 32,
                            20 + (i*17)%400, 10 + (i*13)%220);
        uint32_t us = ticks_to_us(timer_elapsed(t0, timer_read()));
        uart_puts("  25x sprite 32x32:         "); uart_putdec(us / reps);
        uart_puts(" us/frame\n");
    }
}


/* ================================================================
 *  MAIN
 * ================================================================ */

void main(void)
{
    uart_puts("\n\n");
    uart_puts("=============================================\n");
    uart_puts("  Jupiter SDK — Full Diagnostic Benchmark v2\n");
    uart_puts("  V3s Cortex-A7, 64MB DDR2\n");
    uart_puts("=============================================\n\n");

    timer_init();
    mmu_init();

    uart_puts("mmu_init() complete. Initial state:\n");

    /* ---- Part 1: dump everything ---- */
    dump_cp15();

    /* ---- Part 2: try to fix D-cache ---- */
    part2_dcache_experiments();

    /* ---- After best config: full characterization ---- */
    uart_puts("\n** Running remaining tests with current best config **\n");

    part3_latency_sweep();
    part4_bandwidth();
    part5_pollution();
    part6_misc();
    part7_rendering();

    uart_puts("\n=============================================\n");
    uart_puts("  Diagnostic complete.\n");
    uart_puts("=============================================\n\n");

    while (1) __asm__ volatile("wfi");
}
