/*
 * Virtual Nintendo 64 controller — single-wire Joybus protocol.
 *
 * Wired to one GPIO pin via two qemu_irq paths:
 *   data_out (gpio-out, board ties to PIO ext_in for the data pin) —
 *            we drive it: 1 = release (pull-up wins), 0 = pull low.
 *   host-observation gpio-in — fires on every host-driven level change.
 *
 * Protocol bit cell = 4 µs. A "1" is 1 µs low + 3 µs high; a "0" is
 * 3 µs low + 1 µs high. The host always pulls low to start a cell;
 * both sides idle high.
 *
 * Commands handled:
 *   0x00 Info       — TX 1 byte,  RX (us → host) 3 bytes
 *   0x01 BtnState   — TX 1 byte,  RX 32 bits of button + stick state
 *   0x02 Read32     — TX 3 bytes (cmd + 2-byte addr), RX 33 bytes (32 + CRC)
 *   0x03 Write32    — TX 35 bytes (cmd + addr + 32 data),  RX 1 byte CRC
 *
 * Backing: 32 KB Controller Pak buffer (in-memory; lost on reset).
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "sysemu/reset.h"
#include "ui/input.h"
#include "ui/console.h"
#include "qapi/qapi-types-ui.h"
#include "hw/input/virt-n64-pad.h"

/* ------------------------------------------------------------------ */
/* Button-bit positions inside the 32-bit BtnState response (MSB-first  */
/* on the wire = bit 31 first).                                         */
/* ------------------------------------------------------------------ */
#define N64_BIT_A           31
#define N64_BIT_B           30
#define N64_BIT_Z           29
#define N64_BIT_START       28
#define N64_BIT_UP          27
#define N64_BIT_DOWN        26
#define N64_BIT_LEFT        25
#define N64_BIT_RIGHT       24
#define N64_BIT_L           21
#define N64_BIT_R           20
#define N64_BIT_C_UP        19
#define N64_BIT_C_DOWN      18
#define N64_BIT_C_LEFT      17
#define N64_BIT_C_RIGHT     16

#define N64_RX_SETTLE_NS    20000     /* gap before TX after RX done */
#define N64_BIT_LOW_ONE_NS   1000
#define N64_BIT_HIGH_ONE_NS  3000
#define N64_BIT_LOW_ZERO_NS  3000
#define N64_BIT_HIGH_ZERO_NS 1000
#define N64_STOP_LOW_NS      1000

/* RX bit decision threshold: low pulse below this is a 1, above is a 0. */
#define N64_BIT_THRESH_NS    2000

/* ------------------------------------------------------------------ */
/* Keyboard mapping                                                     */
/* ------------------------------------------------------------------ */
static const struct {
    QKeyCode qcode;
    int      n64_bit;
} n64_keymap[] = {
    { Q_KEY_CODE_UP,      N64_BIT_UP      },
    { Q_KEY_CODE_DOWN,    N64_BIT_DOWN    },
    { Q_KEY_CODE_LEFT,    N64_BIT_LEFT    },
    { Q_KEY_CODE_RIGHT,   N64_BIT_RIGHT   },
    { Q_KEY_CODE_Z,       N64_BIT_A       },
    { Q_KEY_CODE_X,       N64_BIT_B       },
    { Q_KEY_CODE_SHIFT,   N64_BIT_Z       },
    { Q_KEY_CODE_RET,     N64_BIT_START   },
    { Q_KEY_CODE_Q,       N64_BIT_L       },
    { Q_KEY_CODE_W,       N64_BIT_R       },
    { Q_KEY_CODE_I,       N64_BIT_C_UP    },
    { Q_KEY_CODE_K,       N64_BIT_C_DOWN  },
    { Q_KEY_CODE_J,       N64_BIT_C_LEFT  },
    { Q_KEY_CODE_L,       N64_BIT_C_RIGHT },
};

static void n64_kbd_event(DeviceState *dev, QemuConsole *src, InputEvent *evt)
{
    VirtN64PadState *s = VIRT_N64_PAD(dev);

    if (evt->type != INPUT_EVENT_KIND_KEY) {
        return;
    }
    InputKeyEvent *key = evt->u.key.data;
    if (key->key->type != KEY_VALUE_KIND_QCODE) {
        return;
    }
    QKeyCode qc = key->key->u.qcode.data;
    bool down = key->down;

    for (size_t i = 0; i < ARRAY_SIZE(n64_keymap); i++) {
        if (n64_keymap[i].qcode == qc) {
            uint32_t mask = 1u << n64_keymap[i].n64_bit;
            if (down) {
                s->button_bits |= mask;
            } else {
                s->button_bits &= ~mask;
            }
            return;
        }
    }
}

static const QemuInputHandler n64_input_handler = {
    .name  = "Virtual N64 pad",
    .mask  = INPUT_EVENT_MASK_KEY,
    .event = n64_kbd_event,
};

/* ------------------------------------------------------------------ */
/* CRC (8-bit, polynomial 0x85, 33 iterations: 32 data + 1 zero augment) */
/* ------------------------------------------------------------------ */
static uint8_t cpak_data_crc(const uint8_t *buf)
{
    uint8_t crc = 0;
    for (int i = 0; i < N64_CPAK_BLOCK + 1; i++) {
        uint8_t byte = (i < N64_CPAK_BLOCK) ? buf[i] : 0;
        for (int b = 7; b >= 0; b--) {
            int hi = (crc & 0x80) ? 1 : 0;
            crc = (uint8_t)(crc << 1);
            if (byte & (1u << b)) crc ^= 1;
            if (hi) crc ^= 0x85;
        }
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/* Protocol FSM                                                         */
/* ------------------------------------------------------------------ */

static void pad_drive(VirtN64PadState *s, int level)
{
    qemu_set_irq(s->data_out, level);
}

static void schedule_ns(VirtN64PadState *s, uint64_t ns)
{
    timer_mod(s->tmr,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + (int64_t)ns);
}

/* Pack the 32-bit BtnState response — wire format byte 0..3:
 *   byte 0: A B Z Start D-Up D-Down D-Left D-Right
 *   byte 1: 0 0 L R C-Up C-Down C-Left C-Right
 *   byte 2: stick X (int8)
 *   byte 3: stick Y (int8)
 */
static uint32_t build_btnstate(VirtN64PadState *s)
{
    return s->button_bits
         | ((uint32_t)(uint8_t)s->stick_x << 8)
         |  (uint32_t)(uint8_t)s->stick_y;
}

/* Append `nbits` of `value` to the TX buffer, MSB first. */
static void tx_append(VirtN64PadState *s, uint64_t value, int nbits)
{
    for (int i = nbits - 1; i >= 0; i--) {
        int bit = (value >> i) & 1;
        int total = s->tx_total_bits;
        int byte_idx = total / 8;
        int bit_idx  = 7 - (total % 8);
        if (byte_idx >= (int)sizeof s->tx_bytes) return;
        if (bit) s->tx_bytes[byte_idx] |=  (1u << bit_idx);
        else     s->tx_bytes[byte_idx] &= ~(1u << bit_idx);
        s->tx_total_bits++;
    }
}

/* Walk the assembled rx_bytes, decode the joybus command, populate
 * tx_bytes with the response. Always primes a TX even on bogus commands
 * (empty response = pad goes IDLE quickly). */
static void dispatch_command(VirtN64PadState *s)
{
    /* Reset TX buffer. */
    memset(s->tx_bytes, 0, sizeof s->tx_bytes);
    s->tx_total_bits = 0;
    s->tx_bit_idx    = 0;

    if (s->rx_bits < 8) {
        return;  /* nothing decoded */
    }

    uint8_t cmd = s->rx_bytes[0];

    switch (cmd) {
    case N64_CMD_INFO: {
        /* Standard controller (0x05 0x00) with pak inserted (status 0x01). */
        tx_append(s, 0x05, 8);
        tx_append(s, 0x00, 8);
        tx_append(s, 0x01, 8);
        break;
    }
    case N64_CMD_BTNSTATE: {
        tx_append(s, build_btnstate(s), 32);
        break;
    }
    case N64_CMD_READ32: {
        if (s->rx_bits < 24) break;          /* malformed */
        uint16_t addr = ((uint16_t)s->rx_bytes[1] << 8) | s->rx_bytes[2];
        uint16_t block = addr >> 5;          /* drop low 5 bits (checksum) */
        uint32_t off = (uint32_t)block * N64_CPAK_BLOCK;
        uint8_t blk[N64_CPAK_BLOCK];
        if (off + N64_CPAK_BLOCK <= sizeof s->cpak) {
            memcpy(blk, s->cpak + off, N64_CPAK_BLOCK);
        } else {
            memset(blk, 0, sizeof blk);
        }
        for (int i = 0; i < N64_CPAK_BLOCK; i++) tx_append(s, blk[i], 8);
        tx_append(s, cpak_data_crc(blk), 8);
        break;
    }
    case N64_CMD_WRITE32: {
        if (s->rx_bits < 8 + 16 + 32 * 8) break;
        uint16_t addr = ((uint16_t)s->rx_bytes[1] << 8) | s->rx_bytes[2];
        uint16_t block = addr >> 5;
        uint32_t off = (uint32_t)block * N64_CPAK_BLOCK;
        const uint8_t *data = &s->rx_bytes[3];
        if (off + N64_CPAK_BLOCK <= sizeof s->cpak) {
            memcpy(s->cpak + off, data, N64_CPAK_BLOCK);
        }
        tx_append(s, cpak_data_crc(data), 8);
        break;
    }
    default:
        /* Unknown command — empty response. */
        break;
    }
}

/* Begin transmission of whatever's queued in tx_bytes. */
static void begin_tx(VirtN64PadState *s)
{
    if (s->tx_total_bits == 0) {
        s->state = N64_IDLE;
        pad_drive(s, 1);
        return;
    }
    s->tx_bit_idx = 0;
    int byte_idx = 0;
    int bit_idx  = 7;
    s->current_bit = (s->tx_bytes[byte_idx] >> bit_idx) & 1;
    s->state = N64_TX_LOW;
    pad_drive(s, 0);
    schedule_ns(s, s->current_bit ? N64_BIT_LOW_ONE_NS
                                  : N64_BIT_LOW_ZERO_NS);
}

static void pad_tick(void *opaque)
{
    VirtN64PadState *s = opaque;

    switch (s->state) {
    case N64_RX_DONE:
        /* Settle elapsed; ship the response. */
        dispatch_command(s);
        begin_tx(s);
        return;

    case N64_TX_LOW:
        pad_drive(s, 1);
        s->state = N64_TX_HIGH;
        schedule_ns(s, s->current_bit ? N64_BIT_HIGH_ONE_NS
                                      : N64_BIT_HIGH_ZERO_NS);
        return;

    case N64_TX_HIGH:
        s->tx_bit_idx++;
        if (s->tx_bit_idx >= s->tx_total_bits) {
            /* Stop bit: 1 µs low, then back to idle. */
            pad_drive(s, 0);
            s->state = N64_TX_STOP;
            schedule_ns(s, N64_STOP_LOW_NS);
            return;
        }
        {
            int byte_idx = s->tx_bit_idx / 8;
            int bit_idx  = 7 - (s->tx_bit_idx % 8);
            s->current_bit = (s->tx_bytes[byte_idx] >> bit_idx) & 1;
        }
        pad_drive(s, 0);
        s->state = N64_TX_LOW;
        schedule_ns(s, s->current_bit ? N64_BIT_LOW_ONE_NS
                                      : N64_BIT_LOW_ZERO_NS);
        return;

    case N64_TX_STOP:
        pad_drive(s, 1);
        s->state = N64_IDLE;
        return;

    case N64_IDLE:
    case N64_RX:
        return;
    }
}

/* How many command bits we expect after seeing the first byte. */
static int expected_bits_for(uint8_t cmd)
{
    switch (cmd) {
    case N64_CMD_INFO:     return 8;
    case N64_CMD_BTNSTATE: return 8;
    case N64_CMD_READ32:   return 8 + 16;
    case N64_CMD_WRITE32:  return 8 + 16 + 32 * 8;
    default:               return 8;
    }
}

/* Receive one decoded bit into the rx_bytes buffer. */
static void rx_push_bit(VirtN64PadState *s, int bit)
{
    int byte_idx = s->rx_bits / 8;
    int bit_idx  = 7 - (s->rx_bits % 8);
    if (byte_idx >= (int)sizeof s->rx_bytes) return;
    if (bit) s->rx_bytes[byte_idx] |=  (1u << bit_idx);
    else     s->rx_bytes[byte_idx] &= ~(1u << bit_idx);
    s->rx_bits++;

    /* After the first byte, decide how many more bits to expect. */
    if (s->rx_bits == 8 && s->rx_expected_bits < 0) {
        s->rx_expected_bits = expected_bits_for(s->rx_bytes[0]);
    }

    if (s->rx_expected_bits > 0 && s->rx_bits >= s->rx_expected_bits) {
        /* Command bytes all in. The host will follow with a stop bit;
         * ignore it. Fire the response after a settle gap. */
        s->state = N64_RX_DONE;
        schedule_ns(s, N64_RX_SETTLE_NS);
    }
}

static void pad_host_level(void *opaque, int line, int level)
{
    VirtN64PadState *s = opaque;
    bool prev = s->prev_host_level;
    s->prev_host_level = level;

    /* During TX (or once command is fully received) ignore host edges. */
    if (s->state != N64_IDLE && s->state != N64_RX) {
        return;
    }

    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    if (prev && !level) {
        /* Falling edge: host pulled the line low. Start a new bit cell. */
        s->rx_low_ns = now;
        if (s->state == N64_IDLE) {
            s->state = N64_RX;
            s->rx_bits = 0;
            s->rx_expected_bits = -1;
            memset(s->rx_bytes, 0, sizeof s->rx_bytes);
        }
    } else if (!prev && level) {
        /* Rising edge: low duration tells us the bit value. */
        int64_t low_ns = now - s->rx_low_ns;
        int bit = (low_ns < N64_BIT_THRESH_NS) ? 1 : 0;
        rx_push_bit(s, bit);
    }
}

/* ------------------------------------------------------------------ */
/* QOM                                                                  */
/* ------------------------------------------------------------------ */

static void virt_n64_pad_reset(void *opaque)
{
    VirtN64PadState *s = opaque;
    s->button_bits = 0;
    s->stick_x = s->stick_y = 0;
    s->state = N64_IDLE;
    s->prev_host_level = true;
    s->rx_bits = 0;
    s->rx_expected_bits = -1;
    s->tx_total_bits = 0;
    s->tx_bit_idx    = 0;
    /* Note: we deliberately do NOT clear cpak[] here — that survives a
     * machine reset like a battery-backed save would on real hardware.
     * Recreate the device (relaunch QEMU) for a fresh pak. */
    if (s->tmr) {
        timer_del(s->tmr);
    }
    pad_drive(s, 1);
}

static void virt_n64_pad_realize(DeviceState *dev, Error **errp)
{
    VirtN64PadState *s = VIRT_N64_PAD(dev);

    s->tmr = timer_new_ns(QEMU_CLOCK_VIRTUAL, pad_tick, s);

    qdev_init_gpio_in(dev, pad_host_level, 1);
    qdev_init_gpio_out(dev, &s->data_out, 1);

    s->kbd_handler = qemu_input_handler_register(dev, &n64_input_handler);
    qemu_input_handler_activate(s->kbd_handler);

    /* Initial cpak content: all 0xFF (matches a freshly-formatted pak's
     * data area). The cpak filesystem code can recognize zero-init too,
     * but 0xFF more closely matches a never-written EEPROM cell. */
    memset(s->cpak, 0xFF, sizeof s->cpak);

    /* Plain TYPE_DEVICE — sysbus reset chain doesn't reach us. Hook the
     * machine-level reset directly. */
    qemu_register_reset(virt_n64_pad_reset, s);
}

static void virt_n64_pad_unrealize(DeviceState *dev)
{
    VirtN64PadState *s = VIRT_N64_PAD(dev);
    qemu_unregister_reset(virt_n64_pad_reset, s);
    if (s->kbd_handler) {
        qemu_input_handler_unregister(s->kbd_handler);
    }
    if (s->tmr) {
        timer_free(s->tmr);
    }
}

static void virt_n64_pad_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize   = virt_n64_pad_realize;
    dc->unrealize = virt_n64_pad_unrealize;
    dc->desc = "Virtual N64 controller (single-wire pad with cpak)";
}

static const TypeInfo virt_n64_pad_type_info = {
    .name          = TYPE_VIRT_N64_PAD,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(VirtN64PadState),
    .class_init    = virt_n64_pad_class_init,
};

static void virt_n64_pad_register_types(void)
{
    type_register_static(&virt_n64_pad_type_info);
}

type_init(virt_n64_pad_register_types)
