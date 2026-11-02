/* USB HID boot keyboards -> Jupiter's logical controller buttons.
 * USB DMA is owned by TinyUSB; this file only consumes completed reports.
 */
#include "jupiter.h"
#include "input.h"
#include "usb.h"
#include "tusb.h"

typedef struct {
    uint32_t buttons;
    uint8_t address, mounted, pending, reset_down, bad_reports;
} keyboard_t;

static keyboard_t keyboards[CFG_TUH_HID];
static int reset_requested;

static uint32_t key_button(uint8_t key)
{
    switch (key) {
    case HID_KEY_ARROW_UP: case HID_KEY_W: return BTN_UP;
    case HID_KEY_ARROW_DOWN: case HID_KEY_S: return BTN_DOWN;
    case HID_KEY_ARROW_LEFT: case HID_KEY_A: return BTN_LEFT;
    case HID_KEY_ARROW_RIGHT: case HID_KEY_D: return BTN_RIGHT;
    case HID_KEY_ENTER: case HID_KEY_KEYPAD_ENTER:
    case HID_KEY_SPACE: case HID_KEY_Z: return BTN_A;
    case HID_KEY_X: return BTN_B;
    case HID_KEY_C: return BTN_X;
    case HID_KEY_V: return BTN_Y;
    case HID_KEY_Q: return BTN_L;
    case HID_KEY_E: return BTN_R;
    case HID_KEY_1: return BTN_SELECT;
    case HID_KEY_2: return BTN_START;
    case HID_KEY_I: return BTN_C_UP;
    case HID_KEY_K: return BTN_C_DOWN;
    case HID_KEY_J: return BTN_C_LEFT;
    case HID_KEY_L: return BTN_C_RIGHT;
    default: return 0;
    }
}

void pine64_usb_keyboard_clear(void)
{
    for (unsigned i = 0; i < CFG_TUH_HID; ++i)
        keyboards[i] = (keyboard_t){0};
    reset_requested = 0;
}

uint32_t pine64_usb_buttons(void)
{
    uint32_t buttons = 0;
    for (unsigned i = 0; i < CFG_TUH_HID; ++i)
        buttons |= keyboards[i].buttons;
    return buttons;
}

int pine64_usb_connected(void)
{
    for (unsigned i = 0; i < CFG_TUH_HID; ++i)
        if (keyboards[i].mounted)
            return 1;
    return 0;
}

int pine64_usb_reset_requested(void)
{
    int result = reset_requested;
    reset_requested = 0;
    return result;
}

void pine64_usb_keyboard_poll(void)
{
    for (unsigned i = 0; i < CFG_TUH_HID; ++i) {
        keyboard_t *kb = &keyboards[i];
        if (kb->mounted && !kb->pending)
            kb->pending = tuh_hid_receive_report(kb->address, (uint8_t)i);
    }
}

void tuh_hid_mount_cb(uint8_t address, uint8_t instance,
                      uint8_t const *report_descriptor, uint16_t descriptor_len)
{
    (void)report_descriptor;
    (void)descriptor_len;
    if (instance >= CFG_TUH_HID)
        return;
    if (tuh_hid_interface_protocol(address, instance) != HID_ITF_PROTOCOL_KEYBOARD) {
        uart_puts("[usb] HID interface is not a boot keyboard; ignored\n");
        return;
    }
    keyboards[instance] = (keyboard_t){ .address = address, .mounted = 1 };
    uart_puts("[usb] boot keyboard ready on USB port ");
    uart_putdec(pine64_usb_ohci_base() == 0x01c1a400u ? 1u : 2u);
    uart_puts("; arrows/WASD, Enter/Z, Escape/F12 return to menu\n");
    pine64_usb_keyboard_poll();
}

void tuh_hid_umount_cb(uint8_t address, uint8_t instance)
{
    if (instance >= CFG_TUH_HID || keyboards[instance].address != address)
        return;
    keyboards[instance] = (keyboard_t){0};
    uart_puts("[usb] keyboard disconnected\n");
}

void tuh_hid_report_received_cb(uint8_t address, uint8_t instance,
                                uint8_t const *report, uint16_t len)
{
    if (instance >= CFG_TUH_HID)
        return;
    keyboard_t *kb = &keyboards[instance];
    if (!kb->mounted || kb->address != address)
        return;
    kb->pending = 0;

    if (!report || len != 8) {
        /* Never interpret an NKRO/report-ID packet as a boot report.
         * Release stale keys after a failed transfer and retry the host
         * after repeated errors (OHCI endpoint abort is not implemented). */
        kb->buttons = 0;
        kb->reset_down = 0;
        if (!len || ++kb->bad_reports >= 3)
            pine64_usb_hcd_fault("keyboard returned invalid boot reports");
    } else {
        int rollover = 0, reset = 0;
        uint32_t buttons = 0;
        kb->bad_reports = 0;
        for (unsigned i = 2; i < 8; ++i) {
            if (report[i] >= 1 && report[i] <= 3)
                rollover = 1; /* HID rollover/error: keep last valid state. */
            buttons |= key_button(report[i]);
            if (report[i] == HID_KEY_ESCAPE || report[i] == HID_KEY_F12)
                reset = 1;
        }
        if (!rollover) {
            kb->buttons = buttons;
            if (reset && !kb->reset_down)
                reset_requested = 1;
            kb->reset_down = (uint8_t)reset;
        }
    }
    pine64_usb_keyboard_poll();
}
