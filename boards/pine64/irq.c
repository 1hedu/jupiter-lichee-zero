/* PINE A64 GIC-400, nonsecure Group 1, CPU0 only. Recovered from the
 * physically validated SD2 core. Nonsecure accesses preserve TF-A Group 0.
 * Never attempt to reassign IGROUPR or configure a secure interrupt here.
 */
#include "jupiter.h"

#define MAX_IRQ 160u
#ifdef PINE64_CPU_TEST
#define PD(reg) REG32(0x08000000u + (reg))
#define PC(reg) REG32(0x08010000u + (reg))
#else
#define PD(reg) REG32(0x01c81000u + (reg))
#define PC(reg) REG32(0x01c82000u + (reg))
#endif
#define DIST_CTLR PD(0x000u)
#define DIST_TYPER PD(0x004u)
#define ENABLE_SET(n) PD(0x100u + 4u * (n))
#define ENABLE_CLEAR(n) PD(0x180u + 4u * (n))
#define PENDING_CLEAR(n) PD(0x280u + 4u * (n))
#define PRIORITY(n) PD(0x400u + 4u * (n))
#define TARGET(n) PD(0x800u + 4u * (n))
#define CONFIG(n) PD(0xc00u + 4u * (n))
#define CPU_CTLR PC(0x000u)
#define CPU_PMR PC(0x004u)

static void (*irq_table[MAX_IRQ])(void);
static uint32_t irq_count;

void irq_handler(uint32_t irq_id)
{
    if (irq_id >= irq_count)
        return;
    if (irq_table[irq_id])
        irq_table[irq_id]();
    else if (irq_id >= 16u)
        irq_disable(irq_id); /* Unhandled asserted level must not livelock. */
}

void irq_register(uint32_t irq_id, void (*handler)(void))
{
    if (irq_id < MAX_IRQ) {
        irq_table[irq_id] = handler;
        __asm__ volatile("dmb sy" ::: "memory");
    }
}

void irq_init(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
    CPU_CTLR = 0;
    DIST_CTLR = 0;
    __asm__ volatile("dsb sy" ::: "memory");
    irq_count = ((DIST_TYPER & 31u) + 1u) * 32u;
    if (irq_count > MAX_IRQ)
        irq_count = MAX_IRQ;
    for (uint32_t i = 0; i < MAX_IRQ; ++i)
        irq_table[i] = 0;

    /* GIC CPU target IDs need not equal MPIDR IDs. Derive this CPU's
     * target mask from the banked SGI/PPI ITARGETSR word. */
    uint32_t target = TARGET(0);
    target = (target | (target >> 8) | (target >> 16) | (target >> 24)) & 255u;
    if (!target)
        target = 1u;
    target *= 0x01010101u;
    for (uint32_t i = 1; i < irq_count / 32u; ++i) {
        ENABLE_CLEAR(i) = UINT32_MAX;
        PENDING_CLEAR(i) = UINT32_MAX;
    }
    for (uint32_t i = 8; i < irq_count / 4u; ++i) {
        PRIORITY(i) = 0xa0a0a0a0u;
        TARGET(i) = target;
    }
    for (uint32_t i = 2; i < irq_count / 16u; ++i)
        CONFIG(i) = 0; /* A64 peripheral SPIs are level triggered. */
    __asm__ volatile("dsb sy" ::: "memory");
    CPU_PMR = 255u;
    /* In the nonsecure view, bit0 controls Group1 (not secure Group0). */
    DIST_CTLR = 1;
    CPU_CTLR = 1;
    __asm__ volatile("dsb sy\n\tisb" ::: "memory");
}

void irq_enable(uint32_t irq_id)
{
    if (irq_id >= 16u && irq_id < irq_count) {
        __asm__ volatile("dmb sy" ::: "memory");
        ENABLE_SET(irq_id / 32u) = BIT(irq_id % 32u);
        __asm__ volatile("dsb sy" ::: "memory");
    }
}

void irq_disable(uint32_t irq_id)
{
    if (irq_id >= 16u && irq_id < irq_count) {
        ENABLE_CLEAR(irq_id / 32u) = BIT(irq_id % 32u);
        __asm__ volatile("dsb sy" ::: "memory");
    }
}

void irq_global_enable(void)
{
    __asm__ volatile("dsb sy\n\tcpsie i\n\tisb" ::: "memory");
}
