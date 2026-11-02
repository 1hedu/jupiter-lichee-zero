/* Cortex-A53 AArch32, short-descriptor identity map. The first-boot build
 * using this layout passed real PINE A64+ CPU/MMU/NEON/display/SD checks.
 * TF-A configures CPUECTLR.SMPEN; never write Cortex-A7 ACTLR.SMP here.
 */
#include "jupiter.h"
#include <stdint.h>

static uint32_t page_table[4096] __attribute__((aligned(16384)));
#define MT_DEVICE  0x00010c16u /* Shareable Device, XN, AP=11, domain 0, section */
#define MT_CACHED  0x00011c0eu /* Shareable Normal WB/WA, AP=11, TEX=001 C=B=1 */
#define MT_UNCACHED 0x00011c12u /* Shareable Normal non-cacheable, XN, TEX=001 */

static void invalidate_dcache_all(void)
{
    uint32_t clidr;
    __asm__ volatile("mrc p15, 1, %0, c0, c0, 1" : "=r"(clidr));
    for (uint32_t level = 0; level < 7u; ++level) {
        uint32_t type = (clidr >> (3u * level)) & 7u;
        if (type == 0)
            break;
        if (type < 2u || type > 4u)
            continue;
        uint32_t selector = level << 1;
        __asm__ volatile("mcr p15, 2, %0, c0, c0, 0\n\tisb" :: "r"(selector) : "memory");
        uint32_t ccsidr;
        __asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(ccsidr));
        uint32_t line_shift = (ccsidr & 7u) + 4u;
        uint32_t last_way = (ccsidr >> 3) & 1023u;
        uint32_t last_set = (ccsidr >> 13) & 32767u;
        uint32_t way_shift = last_way ? (uint32_t)__builtin_clz(last_way) : 0;
        for (uint32_t way = 0; way <= last_way; ++way) {
            for (uint32_t set = 0; set <= last_set; ++set) {
                uint32_t operand = selector | (set << line_shift) | (way << way_shift);
                __asm__ volatile("mcr p15, 0, %0, c7, c6, 2" :: "r"(operand) : "memory");
            }
        }
    }
    __asm__ volatile("mcr p15, 2, %0, c0, c0, 0\n\tdsb sy\n\tisb" :: "r"(0) : "memory");
}

void mmu_init(void)
{
    uint32_t sctlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    if (sctlr & 1u)
        return; /* Demo entry may initialize again: never invalidate dirty state. */
    if (sctlr & 4u) {
        uart_puts("[pine64/mmu] refusing init with D-cache already active\n");
        for (;;)
            __asm__ volatile("wfi");
    }
    for (unsigned i = 0; i < 4096u; ++i)
        page_table[i] = 0;
    for (uint32_t i = 0; i < 0x020u; ++i)
        page_table[i] = (i << 20) | MT_DEVICE;
    for (uint32_t i = 0x400u; i < 0xc00u; ++i)
        page_table[i] = (i << 20) | MT_CACHED;
    page_table[0x580] = 0x58000000u | MT_UNCACHED; /* USB descriptors/data */
#ifdef PINE64_CPU_TEST
    /* QEMU virt GIC and console for the architecture-only test. */
    page_table[0x080] = 0x08000000u | MT_DEVICE;
    page_table[0x090] = 0x09000000u | MT_DEVICE;
#endif
#ifdef PINE64_USB_QEMU_TEST
    page_table[0x100] = 0x10000000u | MT_DEVICE; /* QEMU PCI OHCI BAR */
    page_table[0x3f0] = 0x3f000000u | MT_DEVICE; /* QEMU PCI ECAM */
#endif
    sctlr &= ~((1u << 28) | (1u << 29) | (1u << 19)); /* TRE, AFE, WXN */
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0\n\tisb" :: "r"(sctlr) : "memory");
    uint32_t ttbr0 = (uint32_t)(uintptr_t)page_table | (1u << 6) | (1u << 3) | (1u << 1);
    /* IRGN[0]=bit6 (inner WBWA), RGN=01 (outer WBWA), S=1. Bit0 is IRGN[1]. */
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 2" :: "r"(0) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" :: "r"(ttbr0) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(1) : "memory"); /* domain0 client */
    __asm__ volatile("mcr p15, 0, %0, c8, c7, 0\n\tdsb sy\n\tisb" :: "r"(0) : "memory");
    invalidate_dcache_all();
    sctlr |= (1u << 0) | (1u << 2) | (1u << 12);
    __asm__ volatile("dsb sy\n\tmcr p15, 0, %0, c1, c0, 0\n\tisb" :: "r"(sctlr) : "memory");
    uart_puts("[pine64/mmu] AArch32, 2 GB mapped, L1/L2 cache active\n");
}

static uint32_t dcache_line_size(void)
{
    uint32_t ctr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));
    return 4u << ((ctr >> 16) & 15u);
}

void dcache_clean_range(uint32_t addr, uint32_t size)
{
    if (!size || size - 1u > UINT32_MAX - addr)
        return;
    uint32_t line = dcache_line_size();
    uint32_t last = (addr + size - 1u) & ~(line - 1u);
    for (uint32_t p = addr & ~(line - 1u);; p += line) {
        __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(p) : "memory");
        if (p == last)
            break;
    }
    __asm__ volatile("dsb sy" ::: "memory");
}

void dcache_invalidate_range(uint32_t addr, uint32_t size)
{
    if (!size || size - 1u > UINT32_MAX - addr)
        return;
    uint32_t line = dcache_line_size();
    uint32_t first = addr & ~(line - 1u);
    uint32_t end_byte = addr + size - 1u;
    uint32_t last = end_byte & ~(line - 1u);
    for (uint32_t p = first;; p += line) {
        /* Preserve unrelated dirty bytes on partial boundary lines. For
         * DMA destinations callers must still clean/invalidate before DMA. */
        if ((p == first && addr != first) ||
            (p == last && (end_byte & (line - 1u)) != line - 1u))
            __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(p) : "memory");
        else
            __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" :: "r"(p) : "memory");
        if (p == last)
            break;
    }
    __asm__ volatile("dsb sy" ::: "memory");
}
