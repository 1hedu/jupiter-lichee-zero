/*
 * Jupiter SDK — ARM PMU cycle counter
 * Direct PMCCNTR access for bare-metal Cortex-A7 profiling.
 */
#ifndef PMU_H
#define PMU_H

#include <stdint.h>

/* Enable cycle counter. Call once from privileged mode (SVC) at boot. */
static inline void pmu_init(void)
{
    /* PMUSERENR: allow user-mode reads of PMCCNTR */
    __asm__ volatile("mcr p15, 0, %0, c9, c14, 0" :: "r"(1));
    /* PMCNTENSET: enable cycle counter (bit 31) */
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(0x80000000u));
    /* PMCR: enable (bit 0) + reset CCNT (bit 2), no divider */
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(0x05));
}

/* Read the 32-bit cycle counter. Wraps every ~3.6s at 1.2GHz. */
static inline uint32_t pmu_cycles(void)
{
    uint32_t v;
    __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(v));
    return v;
}

/* Spin-wait helpers using the PMU cycle counter. CPU is hardcoded at
 * 1.2 GHz (Jupiter PLL setup); change CPU_MHZ if that ever changes. */
#define PMU_CPU_MHZ 1200u

static inline void delay_us(uint32_t us)
{
    uint32_t start  = pmu_cycles();
    uint32_t target = us * PMU_CPU_MHZ;
    while ((pmu_cycles() - start) < target);
}

static inline void delay_ns(uint32_t ns)
{
    uint32_t start  = pmu_cycles();
    uint32_t target = (ns * PMU_CPU_MHZ) / 1000u;
    if (target < 2) target = 2;
    while ((pmu_cycles() - start) < target);
}

#endif /* PMU_H */
