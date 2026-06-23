/*
 * Virtual Nintendo 64 controller — single-wire open-drain protocol
 *
 * Attaches to one GPIO pin (the "data" line). On real hardware the host
 * sends an 8-bit command, then releases the line; the controller replies
 * with N bytes of pulse-width-coded data (1µs low + 3µs high = bit 1,
 * 3µs low + 1µs high = bit 0).
 *
 * This model skips command decoding: after the host's first falling edge
 * we wait a fixed settle time, then respond to a 0x01 (read-state)
 * command with the current button + stick state as 32 bits.
 *
 * Keyboard maps to the controller via QEMU's input subsystem.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_INPUT_VIRT_N64_PAD_H
#define HW_INPUT_VIRT_N64_PAD_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "ui/input.h"

#define TYPE_VIRT_N64_PAD "virt-n64-pad"
OBJECT_DECLARE_SIMPLE_TYPE(VirtN64PadState, VIRT_N64_PAD)

typedef enum {
    N64_IDLE,
    N64_RX,         /* host is mid-command, decoding bits as they arrive */
    N64_RX_DONE,    /* command decoded, waiting for host stop bit + settle */
    N64_TX_LOW,
    N64_TX_HIGH,
    N64_TX_STOP,
} VirtN64State;

/* N64 Joybus commands the pad needs to handle. 0x01 is the
 * read-button-state command — the only one our pre-cpak version handled.
 * The 0x00/0x02/0x03 set is what cpak.c uses for the Controller Pak. */
#define N64_CMD_INFO     0x00
#define N64_CMD_BTNSTATE 0x01
#define N64_CMD_READ32   0x02
#define N64_CMD_WRITE32  0x03

/* Real cpak addresses run 0x0000..0xFFFF (16-bit). The lower 32 KB is
 * user data; the upper 32 KB is config/status space, but Jupiter's
 * probe writes block 0x400 (offset 0x8000) and expects readback, so we
 * back the full 64 KB and let it behave like flat RAM. */
#define N64_CPAK_SIZE    (64 * 1024)
#define N64_CPAK_BLOCK   32

struct VirtN64PadState {
    /*< private >*/
    DeviceState parent_obj;
    /*< public >*/

    /* Wired externally to the target PIO's input line for the data pin
     * via qemu_irq. The pad pulls the pin low via qemu_set_irq(line, 0)
     * (since the PIO ORs ext_in, 1=asserted high). To pull low we have
     * to clear the bit; to "release to high" (external pull-up on real
     * HW) we set it high. */
    qemu_irq data_out;

    /* Current button state, bits laid out as in the N64 response:
     *   byte 0: A Z(7..0)  B Z Start DU DD DL DR
     *   byte 1: Rsv Rsv L R Cu Cd Cl Cr
     *   byte 2: stick X (int8)
     *   byte 3: stick Y (int8)
     */
    uint32_t button_bits;
    int8_t   stick_x, stick_y;

    /* Protocol FSM */
    VirtN64State state;
    QEMUTimer   *tmr;

    /* RX side — host is bit-banging command bytes at us. We timestamp
     * each falling edge of the line and measure the rising-edge delta
     * (≤2 µs ≈ bit 1, ≈3 µs ≈ bit 0). */
    uint8_t      rx_bytes[40];     /* max command = 35 B (write32) */
    int          rx_bits;          /* bits decoded so far */
    int          rx_expected_bits; /* set after first byte decoded */
    int64_t      rx_low_ns;        /* timestamp of most recent falling edge */

    /* TX side — generalized to a byte buffer for cpak responses up to
     * 33 B (read32) instead of a hard 32-bit register. */
    uint8_t      tx_bytes[40];
    int          tx_total_bits;
    int          tx_bit_idx;
    bool         current_bit;

    /* For detecting host edges. */
    bool prev_host_level;

    /* 32 KB Controller Pak backing storage. */
    uint8_t      cpak[N64_CPAK_SIZE];

    QemuInputHandlerState *kbd_handler;
};

#endif /* HW_INPUT_VIRT_N64_PAD_H */
