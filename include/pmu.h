/*
 * Jupiter SDK — ARM PMU cycle counter
 * Direct PMCCNTR access for bare-metal ARM profiling. PINE A64 delay
 * helpers use the 24 MHz crystal timer; CPU speed is never assumed.
 */
#ifndef PMU_H
#define PMU_H

#include <stdint.h>

#ifdef JUPITER_BOARD_PINE64
/* PMU register accesses match Arm Cortex-A53 TRM DDI 0500J §12.5.
 * PMCCFILTR is initialized explicitly: U-Boot is not its owner anymore.
 * PMCCNTR remains a CPU-cycle counter, useful for real cycle profiling.
 */
void pine64_pmu_init(void);
uint32_t pine64_pmu_mhz(void);
uint32_t pine64_cycles_to_us(uint32_t cycles);
void pine64_delay_us(uint32_t us);
void pine64_delay_ns(uint32_t ns);

static inline void pmu_init(void) { pine64_pmu_init(); }
static inline uint32_t pmu_cycles(void)
{
    uint32_t v;
    __asm__ volatile("isb\n\tmrc p15, 0, %0, c9, c13, 0" : "=r"(v) :: "memory");
    return v;
}
#define PMU_CPU_MHZ (pine64_pmu_mhz())
static inline void delay_us(uint32_t us) { pine64_delay_us(us); }
static inline void delay_ns(uint32_t ns) { pine64_delay_ns(ns); }
#else
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

#endif /* JUPITER_BOARD_PINE64 */
#endif /* PMU_H */
