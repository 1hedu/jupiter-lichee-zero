/* Architecture/OHCI integration fixture. The production TinyUSB HCD,
 * host stack, keyboard decoder, startup and uncached DMA map run against
 * QEMU's PCI OHCI + USB keyboard. Only PCI discovery, console and clock
 * hooks replace Allwinner-specific hardware. This is not a PHY test. */
#include "jupiter.h"
#include "input.h"
#include "usb.h"
#include "tusb.h"
#include "host/hcd.h"

static uint32_t counter_khz;
static int fault;

void uart_puts(const char *s)
{
    volatile uint32_t *uart = (volatile uint32_t *)0x09000000u;
    while (*s) {
        while (uart[0x18 / 4] & (1u << 5))
            ;
        uart[0] = (uint8_t)*s++;
    }
}
void uart_putdec(uint32_t n)
{
    char text[11];
    unsigned i = 10;
    text[i] = 0;
    do { text[--i] = '0' + n % 10u; n /= 10u; } while (n);
    uart_puts(text + i);
}

static void finish(int status) __attribute__((noreturn));
static void finish(int status)
{
    uint32_t block[2] = {0x20026u, (uint32_t)status};
    register uint32_t op __asm__("r0") = 0x20u;
    register uint32_t arg __asm__("r1") = (uint32_t)(uintptr_t)block;
    __asm__ volatile("svc 0xab" : "+r"(op) : "r"(arg) : "memory");
    for (;;) __asm__ volatile("wfi");
}

uintptr_t pine64_usb_ohci_base(void) { return 0x10000000u; }
uint32_t tusb_time_millis_api(void)
{ return (uint32_t)(pine64_usb_counter() / counter_khz); }
void tusb_time_delay_ms_api(uint32_t ms)
{
    uint32_t start = tusb_time_millis_api();
    while (tusb_time_millis_api() - start < ms) {}
}
bool pine64_usb_wait32(volatile uint32_t *reg, uint32_t mask,
                       uint32_t value, uint32_t timeout_ms)
{
    uint32_t start = tusb_time_millis_api();
    do {
        pine64_usb_dma_barrier();
        if ((*reg & mask) == value) return true;
    } while (tusb_time_millis_api() - start < timeout_ms);
    return false;
}
void hcd_int_enable(uint8_t rhport) { (void)rhport; pine64_usb_dma_barrier(); }
void hcd_int_disable(uint8_t rhport) { (void)rhport; pine64_usb_dma_barrier(); }
void pine64_usb_hcd_fault(const char *reason)
{ uart_puts(reason); uart_puts("\n"); fault = 1; }

int main(void)
{
    uint32_t frequency;
    __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(frequency));
    counter_khz = frequency / 1000u;
    if (!counter_khz) finish(1);
    mmu_init();
    uintptr_t config = 0x3f008000u; /* Bus 0, device 1, function 0. */
    if (REG32(config) == UINT32_MAX || (REG32(config + 8) >> 8) != 0x0c0310u) {
        uart_puts("USB QEMU FAIL: OHCI PCI device absent\n");
        finish(1);
    }
    REG32(config + 0x10) = 0x10000000u; /* BAR0 */
    REG32(config + 0x04) = 6;           /* Memory decode + bus mastering. */
    pine64_usb_dma_barrier();
    pine64_usb_keyboard_clear();
    const tusb_rhport_init_t init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_FULL};
    if (!tuh_rhport_init(0, &init)) {
        uart_puts("USB QEMU FAIL: OHCI initialization\n");
        finish(1);
    }
    uint32_t start = tusb_time_millis_api();
    int announced = 0, pressed = 0;
    while (tusb_time_millis_api() - start < 15000u && !fault) {
        hcd_int_handler(0, false);
        tuh_task_ext(0, false);
        pine64_usb_keyboard_poll();
        if (pine64_usb_connected() && !announced) {
            announced = 1;
            uart_puts("USB QEMU: keyboard enumerated; waiting for W+Z chord\n");
        }
        if (pine64_usb_buttons() == (BTN_UP | BTN_A) && !pressed) {
            pressed = 1;
            uart_puts("USB QEMU: key chord received through interrupt DMA\n");
        }
        if (pressed && !pine64_usb_buttons()) {
            if (!tuh_deinit(0)) break;
            uart_puts("USB QEMU PASS: enumeration, setup/control DMA, HID interrupt DMA, W+Z and release\n");
            finish(0);
        }
    }
    uart_puts("USB QEMU FAIL: host timeout or fault; keyboard connected=");
    uart_putdec(pine64_usb_connected());
    uart_puts(" OHCI control="); uart_putdec(REG32(0x10000004u));
    uart_puts(" status="); uart_putdec(REG32(0x1000000cu));
    uart_puts(" port="); uart_putdec(REG32(0x10000054u));
    uart_puts(" frame="); uart_putdec(hcd_frame_number(0));
    uart_puts("\n");
    finish(1);
}
