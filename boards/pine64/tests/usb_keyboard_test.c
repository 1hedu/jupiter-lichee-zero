#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "input.h"
#include "usb.h"
#include "tusb.h"

static uint8_t mock_protocol = HID_ITF_PROTOCOL_KEYBOARD;
static unsigned receives, faults;
void uart_puts(const char *text) { (void)text; }
void uart_putdec(uint32_t value) { (void)value; }
uintptr_t pine64_usb_ohci_base(void) { return 0x01c1a400u; }
void pine64_usb_hcd_fault(const char *reason) { (void)reason; ++faults; }
uint8_t tuh_hid_interface_protocol(uint8_t address, uint8_t instance)
{ (void)address; (void)instance; return mock_protocol; }
bool tuh_hid_receive_report(uint8_t address, uint8_t instance)
{ (void)address; (void)instance; ++receives; return true; }

static void report(uint8_t instance, uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t data[8] = {0, 0, a, b, c, 0, 0, 0};
    tuh_hid_report_received_cb((uint8_t)(instance + 1), instance, data, sizeof(data));
}

int main(void)
{
    pine64_usb_keyboard_clear();
    assert(!pine64_usb_connected() && !pine64_usb_buttons());
    mock_protocol = HID_ITF_PROTOCOL_MOUSE;
    tuh_hid_mount_cb(1, 0, NULL, 0);
    assert(!pine64_usb_connected() && !receives);
    mock_protocol = HID_ITF_PROTOCOL_KEYBOARD;
    tuh_hid_mount_cb(1, 0, NULL, 0);
    assert(pine64_usb_connected() && receives == 1);
    pine64_usb_keyboard_poll();
    assert(receives == 1); // Never enqueue two receives on one endpoint.

    report(0, HID_KEY_W, HID_KEY_ARROW_RIGHT, HID_KEY_Z);
    assert(pine64_usb_buttons() == (BTN_UP | BTN_RIGHT | BTN_A));
    report(0, HID_KEY_A, HID_KEY_Q, HID_KEY_1);
    assert(pine64_usb_buttons() == (BTN_LEFT | BTN_L | BTN_SELECT));
    report(0, HID_KEY_S, HID_KEY_E, HID_KEY_2);
    assert(pine64_usb_buttons() == (BTN_DOWN | BTN_R | BTN_START));
    report(0, HID_KEY_X, HID_KEY_C, HID_KEY_V);
    assert(pine64_usb_buttons() == (BTN_B | BTN_X | BTN_Y));
    report(0, HID_KEY_I, HID_KEY_J, HID_KEY_L);
    assert(pine64_usb_buttons() == (BTN_C_UP | BTN_C_LEFT | BTN_C_RIGHT));
    report(0, HID_KEY_K, HID_KEY_SPACE, 0);
    assert(pine64_usb_buttons() == (BTN_C_DOWN | BTN_A));
    report(0, 1, 1, 1); // HID rollover retains the most recent valid report.
    assert(pine64_usb_buttons() == (BTN_C_DOWN | BTN_A));
    report(0, 0, 0, 0);
    assert(!pine64_usb_buttons());

    report(0, HID_KEY_ESCAPE, HID_KEY_ENTER, 0);
    assert(pine64_usb_buttons() == BTN_A && pine64_usb_reset_requested());
    assert(!pine64_usb_reset_requested());
    report(0, HID_KEY_ESCAPE, 0, 0);
    assert(!pine64_usb_reset_requested()); // Reset is an edge, not repeated.
    report(0, 0, 0, 0);
    report(0, HID_KEY_F12, 0, 0);
    assert(pine64_usb_reset_requested());

    tuh_hid_mount_cb(2, 1, NULL, 0);
    report(0, HID_KEY_W, 0, 0);
    report(1, HID_KEY_D, 0, 0);
    assert(pine64_usb_buttons() == (BTN_UP | BTN_RIGHT));
    tuh_hid_umount_cb(1, 0);
    assert(pine64_usb_connected() && pine64_usb_buttons() == BTN_RIGHT);
    tuh_hid_umount_cb(2, 1);
    assert(!pine64_usb_connected() && !pine64_usb_buttons());

    tuh_hid_mount_cb(1, 0, NULL, 0);
    report(0, HID_KEY_Z, 0, 0);
    uint8_t short_report[7] = {0};
    tuh_hid_report_received_cb(1, 0, short_report, sizeof(short_report));
    assert(!pine64_usb_buttons() && !faults);
    tuh_hid_report_received_cb(1, 0, NULL, 0);
    assert(faults == 1); // Failed transfer cannot leave a held button stuck.
    unsigned before = receives;
    tuh_hid_report_received_cb(2, 0, short_report, sizeof(short_report));
    assert(receives == before); // A stale callback cannot update a new device.
    puts("USB keyboard PASS: chords, mappings, releases, rollover, reset edges, hot-unplug and failed reports");
    return 0;
}
