/* Host stub for the SDK's pmu.h (cycle counter) — test builds only. */
#ifndef HOST_PMU_STUB_H
#define HOST_PMU_STUB_H
#include <stdint.h>
#define PMU_CPU_MHZ 1200u
static uint32_t host_cycles;
static inline uint32_t pmu_cycles(void) { return host_cycles += 1000; }
static inline void delay_us(uint32_t us) { (void)us; }
#endif
