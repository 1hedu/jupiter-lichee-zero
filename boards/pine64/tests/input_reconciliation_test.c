/* Actual PINE input implementation; only USB/serial/Joybus/time are modeled. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "input.h"
#define N64_JOYBUS_H
static uint32_t n64_joybus_lock(void) { return 0x80; }
static void n64_joybus_unlock(uint32_t saved) { assert(saved == 0x80); }
static void n64_pin_input(void) {}
static uint8_t reply[4];
static int reply_bit, reply_fails;
static unsigned polls, services, initialized_usb;
static uint32_t clock_us, usb_keys, serial_keys;
static unsigned boot_noise_polls;
static void n64_send_byte(uint8_t byte) { assert(byte == 1); reply_bit = 0; }
static void n64_send_stop(void) {}
static int n64_recv_bit(void)
{
    if (reply_fails) return -1;
    assert(reply_bit < 32);
    int bit = reply_bit++;
    return (reply[bit / 8] >> (7 - bit % 8)) & 1u;
}
void pine64_usb_init(void) { ++initialized_usb; }
uint32_t pine64_usb_buttons(void)
{
    ++polls;
    return polls <= boot_noise_polls ? BTN_A : usb_keys;
}
int pine64_usb_connected(void) { return 1; }
uint32_t pine64_serial_buttons(void) { return serial_keys; }
int pine64_serial_connected(void) { return serial_keys != 0; }
void pine64_service(void) { ++services; }
void timer_init(void) {}
uint32_t timer_read(void) { clock_us += 100u; return 0u - clock_us * 24u; }
uint32_t timer_elapsed(uint32_t start, uint32_t end) { return start - end; }
uint32_t ticks_to_us(uint32_t ticks) { return ticks / 24u; }
void uart_puts(const char *text) { (void)text; }

#include "../input.c"

int main(void)
{
    input_init(INPUT_SNES);
    boot_noise_polls = 3;
    input_settle();
    assert(polls == 8 && clock_us >= 8u * 16000u && clock_us < 8u * 17000u);
    input_poll();
    assert(!input_pressed() && !input_held());
    usb_keys = BTN_A | BTN_C_DOWN | BTN_MODE;
    input_poll();
    assert(input_pressed() == usb_keys); /* Preserve every uint32 button bit. */
    input_poll();
    assert(!input_pressed());
    usb_keys = 0;
    input_poll();
    assert(input_released() == (BTN_A | BTN_C_DOWN | BTN_MODE));

    input_init(INPUT_SNES);
    usb_keys = BTN_A;
    polls = 0; boot_noise_polls = 0;
    uint32_t start = clock_us;
    input_settle();
    assert(polls == 60 && clock_us - start < 1100000u);
    input_poll();
    assert(!input_pressed() && input_held() == BTN_A);

    usb_keys = 0;
    input_init(INPUT_N64);
    reply[0] = 0x80; reply[1] = 0x04; reply[2] = 67; reply[3] = (uint8_t)-35;
    input_state_t state = input_poll();
    assert(state.buttons == (BTN_A | BTN_C_DOWN) && state.stick_x == 67 && state.stick_y == -35);
    assert(input_pressed() == state.buttons);
    reply_fails = 1;
    state = input_poll();
    assert(state.buttons == (BTN_A | BTN_C_DOWN) && state.stick_x == 67 && state.stick_y == -35);
    assert(!input_pressed() && !input_released());
    reply_fails = 0;
    memset(reply, 0, sizeof(reply));
    state = input_poll();
    assert(!state.buttons && input_released() == (BTN_A | BTN_C_DOWN));
    assert(initialized_usb == 1 && services > polls);
    puts("PINE input PASS: noisy boot settling, bounded held-key startup, full-width buttons and last-good N64 reply/stick");
    return 0;
}
