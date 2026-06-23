# `virt-n64-pad` — virtual N64 controller

Source: [`src/hw/input/virt-n64-pad.c`](../../src/hw/input/virt-n64-pad.c)
Header: [`src/include/hw/input/virt-n64-pad.h`](../../src/include/hw/input/virt-n64-pad.h)

A virtual Nintendo 64 controller that talks the single-wire **Joybus**
protocol over one GPIO pin. Wired to PE20 by `lichee-zero` machine init
to match Jupiter's `lib/input.c` convention.

This is not an MMIO device — it has no memory region. It only has GPIO
and host-keyboard input.

## Wiring

The device exposes:

- **One gpio-out** (line 0). Drives the data pin: 0 = pull low, 1 =
  release (external pull-up wins). Wired to the PIO's `ext_in` for
  PE20.
- **One gpio-in** (line 0). Fires whenever the PIO's effective output
  level for PE20 changes. Drives the protocol FSM.

## Joybus protocol

Single-wire, half-duplex, open-drain idle-high. Every bit cell is 4 µs:

| Bit | Low pulse | High pulse |
|-----|-----------|------------|
| `1` | 1 µs       | 3 µs        |
| `0` | 3 µs       | 1 µs        |

The host (Jupiter) starts each bit by pulling low. The pad and host
take turns: host sends an N-byte command, the pad responds with M-byte
data, the same line carries both.

This emulator **requires `-icount`** to decode bit cells. Without it,
virtual time advances in ~20 µs chunks and every bit's low-pulse-width
rounds to the same value.

## RX path (host → pad)

1. On a falling edge of the data line, timestamp `clock_get_ns(VIRTUAL)`.
2. On the matching rising edge, compute `low_ns`:
   - `low_ns < 2000` ⇒ bit `1`
   - `low_ns ≥ 2000` ⇒ bit `0`
3. Shift into `rx_bytes[]`. The first 8 bits are the command opcode;
   that determines how many more bits to expect.

## TX path (pad → host)

After RX completes and the host's stop-bit-plus-settle period passes
(`N64_RX_SETTLE_NS = 20 µs`), we walk `tx_bytes[]` bit by bit:

1. Pull low (`qemu_set_irq(data_out, 0)`).
2. Hold low for 1 or 3 µs depending on the bit.
3. Release (`qemu_set_irq(data_out, 1)`).
4. Hold high for 3 or 1 µs.
5. Repeat for each bit; final stop bit (1 µs low + idle).

Each transition is a `QEMUTimer` expiry on the virtual clock.

## Commands

| Op   | Name      | TX (host→pad) | RX (pad→host) | Behavior                       |
|------|-----------|---------------|---------------|--------------------------------|
| 0x00 | `INFO`    | 1 byte        | 3 bytes       | Returns `0x05 0x00 0x02` ("controller w/ Cpak inserted") |
| 0x01 | `BTN_STATE` | 1 byte      | 4 bytes       | A/B/Z/Start/D-pad/L/R/C-buttons + stick X/Y |
| 0x02 | `READ32`  | 3 bytes (cmd + 2-byte addr) | 33 bytes (32 + CRC-8) | Reads from Controller Pak |
| 0x03 | `WRITE32` | 35 bytes (cmd + addr + 32 data) | 1 byte CRC-8  | Writes to Controller Pak |

CRC-8 polynomial is `0x85`, the N64 OS expectation.

## Controller Pak

The N64 Controller Pak is a 32 KB peripheral that plugs into the
controller's expansion port for save data. The address space the OS
uses runs `0x0000..0xFFFF` (16-bit addresses, block size 32 bytes), but
Jupiter's probe writes block `0x400` (offset `0x8000`, into what's
normally the config / status space). To make that work, we back the
full 64 KB and treat it as flat RAM:

```c
#define N64_CPAK_SIZE    (64 * 1024)
#define N64_CPAK_BLOCK   32
```

The buffer is in-memory only; persistence (save to a host file) is a
future item.

## Keyboard mapping

| Host key      | N64 button |
|---------------|------------|
| ↑ ↓ ← →       | D-pad      |
| Z             | A          |
| X             | B          |
| Shift (left)  | Z trigger  |
| Enter         | Start      |
| Q / W         | L / R      |
| I / K / J / L | C-Up/Down/Left/Right |

QEMU input subsystem callback updates `button_bits` directly.

## Reset

`N64_IDLE` state, RX buffer cleared. Keyboard handler reset. The
gpio-out level is set high (line released). The Controller Pak buffer
is **not** wiped — debugging save bugs is easier across resets, and a
real Cpak has battery-backed RAM anyway.

## Known limitations

- **No analog stick.** `stick_x` / `stick_y` are kept but always 0
  in the BTN_STATE response. Add an axis mapping if you want analog.
- **Only "Cpak inserted" mode.** Doesn't simulate "no Cpak" or "Rumble
  Pak" alternatives.
- **No persistence.** Pak contents lost on QEMU exit.
- **Single pad.** Real N64 has 4 ports; we wire one.
