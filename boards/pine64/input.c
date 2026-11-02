/* PINE64 controls: USB HID keyboard and UART0 supply logical retro-pad
 * buttons; a physical N64 controller on PH9 supplies Joybus/Controller Pak.
 * No SD card pins are repurposed as GPIO. */
#include "jupiter.h"
#include "input.h"
#include "n64_joybus.h"
#include "runtime.h"

extern void pine64_usb_init(void);
extern uint32_t pine64_usb_buttons(void);
extern int pine64_usb_connected(void);
static int input_type, initialized;
static uint32_t curr_buttons, prev_buttons;
static input_state_t last_n64;

static input_state_t physical_n64(void)
{
    input_state_t st = {0};

    /* Critical section: bit-bang timing is jitter-sensitive; if any IRQ
     * fires mid-transmission the bits drift. Audio mix tick @ ~1.87 ms
     * absolutely overlaps the 1+ ms read window. Mask IRQs across the
     * whole transaction. */
    uint32_t saved = n64_joybus_lock();

    n64_send_byte(0x01);
    n64_send_stop();

    uint8_t data[4] = {0};
    int got_all = 1;
    for (int byte = 0; byte < 4 && got_all; byte++) {
        for (int bit = 7; bit >= 0; bit--) {
            int v = n64_recv_bit();
            if (v < 0) { got_all = 0; break; }
            if (v) data[byte] |= (1 << bit);
        }
    }

    n64_joybus_unlock(saved);

    if (!got_all) {
        /* A missed reply must not manufacture release/press edges. Keep
         * the newest branch's last-good-pad behavior, including the stick. */
        st = last_n64;
        st.connected = 0;
        return st;
    }
    st.connected = 1;

    if (data[0] & 0x80) st.buttons |= BTN_A;
    if (data[0] & 0x40) st.buttons |= BTN_B;
    if (data[0] & 0x20) st.buttons |= BTN_Z;
    if (data[0] & 0x10) st.buttons |= BTN_START;
    if (data[0] & 0x08) st.buttons |= BTN_UP;
    if (data[0] & 0x04) st.buttons |= BTN_DOWN;
    if (data[0] & 0x02) st.buttons |= BTN_LEFT;
    if (data[0] & 0x01) st.buttons |= BTN_RIGHT;

    if (data[1] & 0x20) st.buttons |= BTN_L;
    if (data[1] & 0x10) st.buttons |= BTN_R;
    if (data[1] & 0x08) st.buttons |= BTN_C_UP;
    if (data[1] & 0x04) st.buttons |= BTN_C_DOWN;
    if (data[1] & 0x02) st.buttons |= BTN_C_LEFT;
    if (data[1] & 0x01) st.buttons |= BTN_C_RIGHT;

    st.stick_x = (int8_t)data[2];
    st.stick_y = (int8_t)data[3];
    last_n64 = st;

    return st;
}

static input_state_t virtual_pad(void)
{
    pine64_service();
    input_state_t state = {0};
    state.buttons = pine64_usb_buttons() | pine64_serial_buttons();
    state.connected = pine64_usb_connected() || pine64_serial_connected();
    state.stick_x = (state.buttons & BTN_RIGHT ? 80 : 0) - (state.buttons & BTN_LEFT ? 80 : 0);
    state.stick_y = (state.buttons & BTN_UP ? 80 : 0) - (state.buttons & BTN_DOWN ? 80 : 0);
    return state;
}

void input_init(int type)
{
    input_type = type;
    curr_buttons = prev_buttons = 0;
    if (!initialized) {
        initialized = 1;
        pine64_usb_init();
        n64_pin_input();
        uart_puts("[input] USB keyboard / UART keys; optional N64 on PH9\n");
    }
}

input_state_t input_poll_nes(void) { return virtual_pad(); }
input_state_t input_poll_snes(void) { return virtual_pad(); }
input_state_t input_poll_genesis(void) { return virtual_pad(); }
input_state_t input_poll_n64(void)
{
    input_state_t state = virtual_pad();
    input_state_t n64 = physical_n64();
    state.buttons |= n64.buttons;
    if (n64.connected) state.connected = 1;
    if (n64.stick_x || n64.stick_y) { state.stick_x = n64.stick_x; state.stick_y = n64.stick_y; }
    return state;
}

input_state_t input_poll(void)
{
    if (!initialized) input_init(INPUT_N64);
    input_state_t state = input_type == INPUT_N64 ? input_poll_n64() : virtual_pad();
    prev_buttons = curr_buttons;
    curr_buttons = state.buttons;
    return state;
}
uint32_t input_pressed(void) { return curr_buttons & ~prev_buttons; }
uint32_t input_released(void) { return prev_buttons & ~curr_buttons; }
uint32_t input_held(void) { return curr_buttons; }

void input_settle(void)
{
    /* Same five clean polls / ~one-second bound as the latest V3s input
     * driver. Service USB and serial while waiting on the crystal timer. */
    timer_init();
    unsigned clean = 0;
    for (unsigned poll = 0; poll < 60u && clean < 5u; ++poll) {
        input_state_t state = input_poll();
        clean = state.buttons ? 0 : clean + 1;
        uint32_t start = timer_read();
        unsigned bound = 2000000u;
        while (ticks_to_us(timer_elapsed(start, timer_read())) < 16000u && --bound)
            pine64_service();
        if (!bound) return; /* A failed clock must not wedge the launcher. */
    }
}
