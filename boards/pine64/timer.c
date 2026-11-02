/* PINE A64+ timer0, 24 MHz crystal down-counter. No PLL or PMIC changes.
 * Recovered from the SD2 build tested on the user's physical board.
 */
#include "jupiter.h"
#include "peripherals.h"
#include "pmu.h"

#define TMR_IRQ_EN REG32(0x01c20c00u)
#define TMR_IRQ_STATUS REG32(0x01c20c04u)
#define TMR0_CTRL REG32(0x01c20c10u)
#define TMR0_INTERVAL REG32(0x01c20c14u)
#define TMR0_CURRENT REG32(0x01c20c18u)
#define TMR_ENABLE BIT(0)
#define TMR_RELOAD BIT(1)
#define TMR_24M BIT(2)
static int timer_ready;

int pine64_timer_ready(void)
{
    return timer_ready;
}

void timer_init(void)
{
    if (timer_ready)
        return; /* Menu and each demo initialize: retain the shared timebase. */
    timer_ready = 0;
    TMR_IRQ_EN &= ~BIT(0);
    TMR0_CTRL = TMR_24M;
    __asm__ volatile("dsb sy" ::: "memory");
    /* More than two timer clock cycles after disabling before reloading. */
    for (volatile unsigned i = 0; i < 128u; ++i)
        (void)TMR0_CTRL;
    TMR0_INTERVAL = UINT32_MAX;
    TMR_IRQ_STATUS = BIT(0);
    TMR0_CTRL = TMR_24M | TMR_RELOAD | TMR_ENABLE;
    __asm__ volatile("dsb sy" ::: "memory");
    unsigned limit = 1000000u;
    while (TMR0_CTRL & TMR_RELOAD) {
        if (!--limit) {
            uart_puts("[pine64/timer] reload timeout\n");
            return;
        }
    }
    uint32_t first = TMR0_CURRENT;
    limit = 1000000u;
    while (TMR0_CURRENT == first) {
        if (!--limit) {
            uart_puts("[pine64/timer] counter did not start\n");
            return;
        }
    }
    timer_ready = 1;
}

uint32_t timer_read(void) { return TMR0_CURRENT; }
uint32_t timer_elapsed(uint32_t start, uint32_t end) { return start - end; }
uint32_t ticks_to_us(uint32_t ticks) { return ticks / 24u; }
uint32_t ticks_to_ms(uint32_t ticks) { return ticks / 24000u; }

static void require_timer(void)
{
    if (!timer_ready)
        timer_init();
    if (!timer_ready) {
        uart_puts("[pine64/timer] cannot provide timed delays; halting\n");
        for (;;)
            __asm__ volatile("wfi");
    }
}

void pine64_delay_us(uint32_t us)
{
    require_timer();
    /* Limit each interval to one second to make uint32 tick wrap safe. */
    while (us) {
        uint32_t part = us > 1000000u ? 1000000u : us;
        uint32_t start = timer_read();
        while (timer_elapsed(start, timer_read()) < part * 24u)
            ;
        us -= part;
    }
}

void pine64_delay_ns(uint32_t ns)
{
    if (!ns)
        return;
    require_timer();
    uint32_t ticks = (uint32_t)(((uint64_t)ns * 24u + 999u) / 1000u);
    uint32_t start = timer_read();
    while (timer_elapsed(start, timer_read()) < ticks)
        ;
}

static uint32_t measured_pmu_cycles;
static uint32_t measured_timer_ticks;

void pine64_pmu_init(void)
{
    require_timer();
    /* All accesses are privileged EL1; do not needlessly grant EL0 access.
     * Stop/count-filter/reset/enable the cycle counter without changing
     * event counters or enabling any PMU interrupt. */
    uint32_t cc = 0x80000000u;
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 2" :: "r"(cc) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c9, c14, 2" :: "r"(cc) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c14, c15, 7" :: "r"(0) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 3" :: "r"(cc) : "memory");
    uint32_t pmcr;
    __asm__ volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(pmcr));
    pmcr &= ~((1u << 3) | (1u << 5)); /* D=0: count each cycle; DP=0. */
    pmcr |= 5u;                     /* E=1, C=1 (cycle reset). */
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 0\n\tisb" :: "r"(pmcr) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 1\n\tisb" :: "r"(cc) : "memory");
    uint32_t start_tick = timer_read();
    uint32_t start_cycle = pmu_cycles();
    while (timer_elapsed(start_tick, timer_read()) < 240000u)
        ;
    uint32_t end_cycle = pmu_cycles();
    measured_timer_ticks = timer_elapsed(start_tick, timer_read());
    measured_pmu_cycles = end_cycle - start_cycle;
    if (measured_pmu_cycles == 0) {
        uart_puts("[pine64/pmu] cycle counter did not start; timing uses 24 MHz timer\n");
        return;
    }
    uart_puts("[pine64/pmu] measured CPU MHz=");
    uart_putdec(pine64_pmu_mhz());
    uart_puts("; delays use 24 MHz crystal\n");
}

uint32_t pine64_pmu_mhz(void)
{
    if (!measured_timer_ticks)
        return 0;
    return (uint32_t)(((uint64_t)measured_pmu_cycles * 24u + measured_timer_ticks / 2u) /
                      measured_timer_ticks);
}

uint32_t pine64_cycles_to_us(uint32_t cycles)
{
    if (!measured_pmu_cycles)
        return 0;
    return (uint32_t)(((uint64_t)cycles * measured_timer_ticks) /
                      ((uint64_t)measured_pmu_cycles * 24u));
}
