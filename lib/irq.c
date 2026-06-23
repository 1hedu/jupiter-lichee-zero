/*
 * irq.c — GIC interrupt controller setup for V3s
 *
 * Initializes the ARM GIC-400, enables specific IRQs,
 * and dispatches to registered C handlers from the ASM vector.
 */
#include "jupiter.h"

#define MAX_IRQ 160

static void (*irq_table[MAX_IRQ])(void);

/* Called from start.S IRQ vector */
void irq_handler(uint32_t irq_id)
{
    if (irq_id < MAX_IRQ && irq_table[irq_id])
        irq_table[irq_id]();
}

void irq_register(uint32_t irq_id, void (*handler)(void))
{
    if (irq_id < MAX_IRQ)
        irq_table[irq_id] = handler;
}

void irq_init(void)
{
    /* Zero the handler table */
    for (int i = 0; i < MAX_IRQ; i++)
        irq_table[i] = 0;

    /* Distributor: disable, configure, re-enable */
    GICD_CTLR = 0;

    /* Set all SPIs to level-triggered, target CPU0, priority 0xA0 */
    for (int i = 1; i < 5; i++) {       /* ISENABLER 1-4 (IDs 32-159) */
        GICD_ICENABLER(i) = 0xFFFFFFFF; /* Disable all first */
    }
    for (int i = 8; i < 40; i++) {       /* Priority for IDs 32-159 */
        GICD_IPRIORITYR(i) = 0xA0A0A0A0;
    }
    for (int i = 8; i < 40; i++) {       /* Target CPU0 for IDs 32-159 */
        GICD_ITARGETSR(i) = 0x01010101;
    }

    /* Enable distributor */
    GICD_CTLR = 1;

    /* CPU interface: enable, accept all priorities */
    GICC_PMR = 0xFF;    /* Priority mask: accept everything */
    GICC_CTLR = 1;      /* Enable CPU interface */
}

void irq_enable(uint32_t irq_id)
{
    GICD_ISENABLER(irq_id / 32) = BIT(irq_id % 32);
}

void irq_disable(uint32_t irq_id)
{
    GICD_ICENABLER(irq_id / 32) = BIT(irq_id % 32);
}

/* Call this after GIC init + handler registration to unmask IRQs at CPU */
void irq_global_enable(void)
{
    __asm__ volatile("cpsie i" ::: "memory");
}
