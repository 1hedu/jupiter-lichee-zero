/*
 * Jupiter SDK — mmu.c
 * MMU + D-cache for Cortex-A7 (ARMv7-A short descriptor, 1MB sections)
 *
 * Memory map (Option A — framebuffers stay uncached):
 *   0x00000000-0x01FFFFFF  Device      Peripherals (DE2, CCU, TCON, UART…)
 *   0x40000000-0x41FFFFFF  Cached WBWA Code, data, stack, tile maps
 *   0x42000000-0x43FFFFFF  Uncached    Framebuffers (DE2 reads from DRAM)
 *
 * The DE2 display engine is a bus master that reads framebuffer data
 * directly from physical DRAM. It does not snoop the CPU cache.
 * Keeping framebuffers uncached means every CPU write is immediately
 * visible to DE2 — no flush required, no tearing from stale cache.
 *
 * Code/data/stack go cached so tile map lookups, game logic, and all
 * the loop/arithmetic overhead run at L1 speed instead of DDR2 speed.
 */
#include "jupiter.h"

/* ---- L1 page table: 4096 × 4B = 16KB, 16KB-aligned ---- */
static uint32_t __attribute__((aligned(16384))) page_table[4096];

/* ---- Section descriptor bits (ARMv7-A short descriptor) ---- */
/* Ref: ARM ARM DDI 0406C.b, Table B3-10 (TRE=0, no TEX remap)
 * Verified against: NuttX armv7-a/mmu.h, ARM AN4838 Table 4,
 * STM32 appnote, Cortex-A7 TRM §5.2.1.
 *
 * Bit layout for 1MB section descriptor:
 *   [1:0]=10  Section  [2]=B  [3]=C  [4]=XN  [8:5]=Domain
 *   [11:10]=AP[1:0]  [14:12]=TEX  [15]=AP[2]  [31:20]=Base addr
 */
#define SECT            0x2             /* bits [1:0] = 10 → Section */
#define SECT_B          (1 << 2)        /* Bufferable */
#define SECT_C          (1 << 3)        /* Cacheable */
#define SECT_XN         (1 << 4)        /* Execute Never */
#define SECT_AP_FULL    (0x3 << 10)     /* AP[1:0]=11, AP[2]=0 → full R/W */
#define SECT_TEX(t)     ((t) << 12)     /* Type Extension field */

/* Memory types (SCTLR.TRE=0, no TEX remap)
 *
 *  TEX  C  B  | Type
 *  000  0  1  | Device (shareable)        — peripherals
 *  001  1  1  | Normal, WB/WA             — code, data, stack
 *  001  0  0  | Normal, Non-Cacheable     — framebuffers (DE2 DMA-visible)
 */
#define MT_DEVICE   (SECT | SECT_AP_FULL | SECT_XN | SECT_B)
    /* TEX=000 C=0 B=1 → Device memory. */

#define MT_CACHED   (SECT | SECT_AP_FULL | SECT_TEX(1) | SECT_C | SECT_B)
    /* TEX=001 C=1 B=1 → Normal, Write-Back, Write-Allocate. */

#define MT_UNCACHED (SECT | SECT_AP_FULL | SECT_TEX(1) | SECT_XN)
    /* TEX=001 C=0 B=0 → Normal, Non-Cacheable. XN because no code here. */

/* ---- D-cache invalidate (must be done before first enable) ---- */
static void invalidate_dcache_all(void)
{
    uint32_t ccsidr, sets, ways, log2_linesize, way_shift;

    /* Select L1 data cache (CSSELR = 0, level 1, data) */
    __asm__ volatile("mcr p15, 2, %0, c0, c0, 0" :: "r"(0));
    __asm__ volatile("isb");

    /* Read cache geometry from CCSIDR */
    __asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(ccsidr));

    log2_linesize = (ccsidr & 0x7) + 4;           /* log2(line bytes) */
    sets          = ((ccsidr >> 13) & 0x7FFF) + 1; /* number of sets  */
    ways          = ((ccsidr >> 3) & 0x3FF) + 1;   /* associativity   */
    way_shift     = (ways <= 1) ? 0 : __builtin_clz(ways - 1); /* bit position */

    /* Invalidate every line by set/way (DCISW) */
    for (uint32_t w = 0; w < ways; w++)
        for (uint32_t s = 0; s < sets; s++) {
            uint32_t val = (w << way_shift) | (s << log2_linesize);
            __asm__ volatile("mcr p15, 0, %0, c7, c6, 2" :: "r"(val));
        }

    __asm__ volatile("dsb");
}

/* ---- Public API ---- */

void mmu_init(void)
{
    /* Idempotent: if MMU already on, skip. A second init would invalidate
     * the D-cache without flushing, destroying dirty stack/global writes. */
    uint32_t sctlr_chk;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr_chk));
    if (sctlr_chk & 1) {
        uart_puts("[mmu] already on, skipping re-init\n");
        return;
    }

    uart_puts("[mmu] building page table...\n");
    /* Default: all 4096 entries = fault (0) */
    for (int i = 0; i < 4096; i++)
        page_table[i] = 0;

    /* 0x00000000–0x01FFFFFF: Peripherals → Device */
    for (uint32_t i = 0x000; i <= 0x01F; i++)
        page_table[i] = (i << 20) | MT_DEVICE;

    /* 0x01D00000: SRAM_C — Normal Non-Cacheable when remapped to CPU.
     * Device memory prevents burst/speculative access. SRAM is zero-wait-state
     * on-die memory, so caching adds no benefit, but Normal memory type allows
     * the CPU to do burst reads/writes and out-of-order access. */
    page_table[0x01D] = (0x01D << 20) | MT_UNCACHED;

    /* 0x40000000–0x41FFFFFF: Code, data, stack → Cached WB/WA */
    for (uint32_t i = 0x400; i <= 0x41F; i++)
        page_table[i] = (i << 20) | MT_CACHED;

    /* 0x42000000–0x43FFFFFF: Framebuffers + remaining DRAM → Cached WB/WA
     * DE2 reads framebuffer from physical DRAM, not through CPU cache.
     * We MUST flush (clean) the back buffer before swap so DE2 sees it.
     * Benefit: pixel writes accumulate in L1 at ~GB/s, then burst-flush.
     */
    for (uint32_t i = 0x420; i <= 0x43F; i++)
        page_table[i] = (i << 20) | MT_CACHED;

    /* TTBR0: page table base + cacheable table walks.
     * IRGN=01 (Inner WB/WA), RGN=01 (Outer WB/WA).
     * Without this, every TLB miss goes to raw DRAM (~200ns). */
    uint32_t ttbr0 = (uint32_t)page_table | (1 << 3) | (1 << 0); /* RGN=01, IRGN=01 */
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" :: "r"(ttbr0));

    /* TTBCR = 0: TTBR0 covers full 4GB, short descriptor format */
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 2" :: "r"(0));

    /* DACR: all domains = Manager (no permission faults in bare metal) */
    __asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(0xFFFFFFFF));

    /* Invalidate TLB */
    __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" :: "r"(0));

    /* Invalidate D-cache — no dirty lines, cache was off until now */
    invalidate_dcache_all();

    __asm__ volatile("dsb");
    __asm__ volatile("isb");


    /* ACTLR.SMP (bit 6): JOIN THE CACHE COHERENCY DOMAIN.
     * On Cortex-A7, the SMP bit must be set for the D-cache to function,
     * even on a single-core chip. With SMP=0, SCTLR.C=1 has no effect —
     * every load goes straight to DRAM at ~272 cycles. With SMP=1,
     * L1 D-cache engages: loads hit at ~9 cycles, bandwidth jumps 74x.
     * This one bit was the difference between 90ms and 2ms per frame. */
    {
        uint32_t actlr;
        __asm__ volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(actlr));
        actlr |= (1 << 6);     /* SMP -- cache coherency participation */
        __asm__ volatile("mcr p15, 0, %0, c1, c0, 1" :: "r"(actlr));
        __asm__ volatile("isb");
    }

    /* Enable MMU (bit 0) + D-cache (bit 2), I-cache (bit 12) stays on */
    uint32_t sctlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    sctlr |= (1 << 0);     /* M — MMU on */
    sctlr |= (1 << 2);     /* C — D-cache on */
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(sctlr));
    __asm__ volatile("dsb");
    __asm__ volatile("isb");

    /* Readback verification */
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    uart_puts("[mmu] SCTLR=0x"); uart_puthex(sctlr);
    uart_puts(" M="); uart_putdec(sctlr & 1);
    uart_puts(" C="); uart_putdec((sctlr >> 2) & 1);
    uart_puts(" I="); uart_putdec((sctlr >> 12) & 1);
    {
        uint32_t actlr;
        __asm__ volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(actlr));
        uart_puts(" SMP="); uart_putdec((actlr >> 6) & 1);
    }
    uart_puts("\n");
}

/* ---- D-cache clean (flush dirty lines to DRAM) ---- */

void dcache_clean_range(uint32_t addr, uint32_t size)
{
    /* Read cache line size from CTR (Cache Type Register)
     * DminLine = CTR[19:16] = log2(words per cache line)
     * Line bytes = 4 << DminLine
     * Cortex-A7: DminLine=4 → 64 bytes per line.
     */
    uint32_t ctr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));
    uint32_t line_size = 4U << ((ctr >> 16) & 0xF);

    /* Align start address down to cache line boundary */
    uint32_t end = addr + size;
    addr &= ~(line_size - 1);

    /* DCCMVAC: Data Cache Clean by MVA to Point of Coherency
     * Writes dirty cache lines back to DRAM without invalidating.
     * DE2 (bus master) reads from DRAM, so this makes pixels visible.
     */
    for (; addr < end; addr += line_size)
        __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(addr));

    __asm__ volatile("dsb");
}
