#ifndef JUPITER_PINE64_USB_H
#define JUPITER_PINE64_USB_H

#include <stdbool.h>
#include <stdint.h>

void pine64_usb_init(void);
void pine64_usb_poll(void);
uint32_t pine64_usb_buttons(void);
int pine64_usb_connected(void);
int pine64_usb_reset_requested(void);

/* Board hooks for the vendored, polled OHCI controller. One controller is
 * active at a time, with logical root port zero on either physical socket. */
uintptr_t pine64_usb_ohci_base(void);
bool pine64_usb_wait32(volatile uint32_t *reg, uint32_t mask,
                       uint32_t value, uint32_t timeout_ms);
void pine64_usb_hcd_fault(const char *reason);
void pine64_usb_keyboard_clear(void);
void pine64_usb_keyboard_poll(void);

static inline void pine64_usb_dma_barrier(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

/* U-Boot armv8_switch_to_el1_m enables EL1PCTEN and EL1PCEN. This is
 * CNTPCT, the always-running physical counter, independent of CPU PLLs. */
static inline uint64_t pine64_usb_counter(void)
{
    uint32_t lo, hi;
    __asm__ volatile("isb\n\tmrrc p15, 0, %0, %1, c14"
                     : "=r"(lo), "=r"(hi) :: "memory");
    return ((uint64_t)hi << 32) | lo;
}

#endif
