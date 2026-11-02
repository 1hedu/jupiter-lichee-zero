/* Main-context input service and reset-to-menu. No asynchronous unwinding
 * through audio IRQs or C++ synthesizers: restart through the same firmware. */
#include "jupiter.h"
#include "input.h"
#include "runtime.h"

extern void pine64_usb_poll(void);
extern int pine64_usb_reset_requested(void);

static uint32_t serial_mask, key_tick[19], escape_tick;
static unsigned ansi_state, ansi_number;
static int serial_seen, active_demo, servicing;

static void key(uint32_t mask, uint32_t now)
{
    serial_seen = 1;
    serial_mask |= mask;
    for (unsigned bit = 0; bit < 19; ++bit)
        if (mask & (1u << bit)) key_tick[bit] = now;
}

static void serial_byte(uint8_t byte, uint32_t now)
{
    if (ansi_state == 1) {
        if (byte == '[' || byte == 'O') { ansi_state = 2; ansi_number = 0; return; }
        ansi_state = 0;
    } else if (ansi_state == 2) {
        if (byte >= '0' && byte <= '9') {
            ansi_number = ansi_number * 10u + byte - '0';
            if (ansi_number > 999u) ansi_state = 0;
            return;
        }
        ansi_state = 0;
        switch (byte) {
        case 'A': key(BTN_UP, now); return;
        case 'B': key(BTN_DOWN, now); return;
        case 'C': key(BTN_RIGHT, now); return;
        case 'D': key(BTN_LEFT, now); return;
        case '~': if (ansi_number == 24u && active_demo) pine64_system_reset(); return;
        default: return;
        }
    }
    if (byte == 27) { ansi_state = 1; escape_tick = now; return; }
    if (byte >= 'A' && byte <= 'Z') byte += 'a' - 'A';
    switch (byte) {
    case 'w': key(BTN_UP, now); break;
    case 's': key(BTN_DOWN, now); break;
    case 'a': key(BTN_LEFT, now); break;
    case 'd': key(BTN_RIGHT, now); break;
    case '\r': case '\n': case ' ': case 'z': key(BTN_A, now); break;
    case 'x': case 8: case 127: key(BTN_B, now); break;
    case 'c': key(BTN_X | BTN_C, now); break;
    case 'v': key(BTN_Y, now); break;
    case 'q': key(BTN_L, now); break;
    case 'e': key(BTN_R, now); break;
    case 'r': key(BTN_Z, now); break;
    case '1': key(BTN_SELECT, now); break;
    case '2': key(BTN_START, now); break;
    case '3': key(BTN_MODE, now); break;
    case 'i': key(BTN_C_UP, now); break;
    case 'k': key(BTN_C_DOWN, now); break;
    case 'j': key(BTN_C_LEFT, now); break;
    case 'l': key(BTN_C_RIGHT, now); break;
    }
}

void pine64_service(void)
{
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    if ((cpsr & 31u) != 19u || servicing) return;
    servicing = 1;
    pine64_usb_poll();
    if (pine64_usb_reset_requested() && active_demo) pine64_system_reset();
    uint32_t now = timer_read();
    for (unsigned count = 0; count < 128u && (UART0_LSR & UART0_LSR_DR); ++count)
        serial_byte((uint8_t)UART0_RBR, now);
    if (ansi_state && ticks_to_us(timer_elapsed(escape_tick, now)) > 50000u) {
        unsigned state = ansi_state;
        ansi_state = 0;
        if (state == 1 && active_demo) pine64_system_reset();
    }
    for (unsigned bit = 0; bit < 19; ++bit)
        if ((serial_mask & (1u << bit)) &&
            ticks_to_us(timer_elapsed(key_tick[bit], now)) > 100000u)
            serial_mask &= ~(1u << bit);
    servicing = 0;
}

uint32_t pine64_serial_buttons(void) { return serial_mask; }
int pine64_serial_connected(void) { return serial_seen; }
void pine64_demo_active(int active) { active_demo = active; }

void pine64_system_reset(void)
{
    uart_puts("\n[menu] returning through firmware reset...\n");
    pine64_audio_stop();
    __asm__ volatile("cpsid if\n\tdsb sy" ::: "memory");
    register uint32_t function __asm__("r0") = 0x84000009u; /* PSCI SYSTEM_RESET */
    __asm__ volatile("smc #0" : "+r"(function) :: "r1", "r2", "r3", "r12", "lr", "memory");
    /* TF-A normally never returns. A64's documented sun6i watchdog is
     * a fallback if firmware does not implement PSCI SYSTEM_RESET. */
    WDOG_IRQ_EN = 0;
    WDOG_CFG = (WDOG_CFG & ~3u) | 1u;
    WDOG_MODE = 1u; /* shortest interval, whole-system reset */
    WDOG_CTRL = (0x0a57u << 1) | 1u;
    __asm__ volatile("dsb sy" ::: "memory");
    for (;;) __asm__ volatile("wfi");
}
