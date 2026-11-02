# PINE A64+ external sound and N64 adapter

These assignments are for the original **PINE A64+ 2 GB**, using the board's
40-pin **Pi-2 connector**. Pin numbers below are physical connector positions,
not Raspberry Pi GPIO numbers. They were checked against PINE64's connector
assignment and the 2 GB Rev C schematic listed under References.

The menu and the software demos use USB keyboard / serial input and the PINE64
audio output. The five `opn2_hw_*` entries additionally require a physical
YM3438 sound chip and its adapter. Controller Pak access requires a physical
N64 controller and accessory on the Joybus wire.

## Pin assignment

| Adapter signal | A64 GPIO | Pi-2 physical pin | Connection |
| --- | --- | ---: | --- |
| YM3438 D0 | PC0 | 19 | 3.3 V to 5 V buffer input |
| YM3438 D1 | PC1 | 21 | 3.3 V to 5 V buffer input |
| YM3438 D2 | PC2 | 23 | 3.3 V to 5 V buffer input |
| YM3438 D3 | PC3 | 24 | 3.3 V to 5 V buffer input |
| YM3438 D4 | PC4 | 32 | 3.3 V to 5 V buffer input |
| YM3438 D5 | PC5 | 33 | 3.3 V to 5 V buffer input |
| YM3438 D6 | PC6 | 36 | 3.3 V to 5 V buffer input |
| YM3438 D7 | PC7 | 11 | 3.3 V to 5 V buffer input |
| YM3438 A0 | PC8 | 12 | 3.3 V to 5 V buffer input |
| YM3438 A1 | PC9 | 35 | 3.3 V to 5 V buffer input |
| YM3438 /WR | PC10 | 38 | 3.3 V to 5 V buffer input |
| YM3438 /CS | PC11 | 40 | 3.3 V to 5 V buffer input |
| YM3438 /IC reset | PC12 | 15 | 3.3 V to 5 V buffer input |
| Si5351 SDA | PH3 | 3 | Open-drain I2C, pull up to 3.3 V |
| Si5351 SCL | PH2 | 5 | Open-drain I2C, pull up to 3.3 V |
| N64 Joybus data | PH9 | 13 | Open-drain, pull up to 3.3 V |
| 3.3 V | — | 1 or 17 | N64 controller / 3.3 V adapter logic |
| Ground | — | 6, 9, 14, 20, 25, 30, 34 or 39 | Common signal ground |

The driver owns PC0 through PC12, PH2, PH3 and PH9. These pins must be free of
other connected devices. Boot microSD uses PF0 through PF5 and is untouched.
Serial console PB8/PB9 and MIDI UART2 PB0/PB1 are also untouched. The original
V3s PB/PG sound adapter and PE20 controller pin assignments do not apply here.

**The YM3438 and Mercury Pico adapters are mutually exclusive connections.**
Both use PC0, PC2 and PC3 on Pi-2 pins 19, 23 and 24. Disconnect the YM
adapter before using Mercury, and disconnect the Pico signal wires before
running a YM3438 demo. Changing menu entries does not physically isolate
shared data/clock/chip-select wires. The current Mercury host is transmit
only: PC1/MISO and PC4/READY are not used or reconfigured. Keep the user's
existing Pico firmware and choose its documented preset pins; the default
SpotPear board receives on GP4, GP6 and GP5. See [README.md](README.md#mercury-with-the-existing-pico--spotpear-firmware).

## YM3438 adapter

Use two **74HCT245** buffers powered at 5 V: one for D0–D7, and one for the
five control signals and, when needed, the 3.3 V Si5351 clock. Put PINE64
signals on the buffers' A side and YM3438 signals on the B side. Fix `DIR`
high for A-to-B transmission and `/OE` low. HCT's TTL-compatible inputs
accept a 3.3 V logic high while supplying the chip's 5 V logic levels.

Keep the YM3438 `/RD` input tied high on the **chip side**. No YM3438 data,
control or clock signal returning at 5 V may be connected directly to a
PINE64 GPIO. Use short wiring, a common ground and local 100 nF supply
decoupling at each IC. Follow the chip/adapter's analog output circuit for
its stereo audio connection; the digital GPIO cable does not bring that
audio into the PINE64 headphone jack.

The live, GB-PSG, NES-PSG and crystal demos retain the original hybrid
arrangement: FM comes from the external YM3438 analog outputs, while the
software PSG comes from the board audio output. Hearing both requires an
external audio mixer or an adapter which combines those outputs correctly.

### Clock sources

| Menu source | Required clock | Firmware behavior |
| --- | --- | --- |
| `opn2_hw_live`, `opn2_hw_input`, `opn2_hw_gb`, `opn2_hw_nes` | Si5351A, **25 MHz crystal**, address `0x60`; CLK0 to YM3438 phi-M through the buffer | Programs CLK0 to nominal 7,670,454 Hz, verifies register reads and PLLA lock |
| `opn2_hw_xtal` | External **8 MHz oscillator module** to YM3438 phi-M | Makes no SoC or Si5351 clock changes; retains the original NTSC-to-8-MHz F-number compensation |

Power the Si5351 I2C side at 3.3 V and use pull-ups to **3.3 V** on SDA/SCL
(module pull-ups are sufficient if correctly supplied). Keep its CLK1/CLK2
unconnected; this driver dedicates the generator to the demo and disables
other outputs. The actual CLK0 accuracy follows the 25 MHz crystal. A
27 MHz crystal module needs different PLL parameters and is not supported
by this fixed table.

The PINE64 port uses the Si5351 clock for the first four entries explicitly.
The V3s CSI MCLK selector is rejected on this board: A64 PE1 is on the camera
connector and its separate power rail. The exposed I2S MCLK shares the SoC
audio PLL. This port keeps that PLL available for the simultaneous software
audio path instead of repurposing it as the external chip clock.

### What the status actually proves

`[si5351/pine64] I2C ACK, register readback and PLLA lock OK` means the
generator answered the expected I2C transactions and accepted the programmed
clock configuration. It does **not** measure CLK0 at the chip's phi-M pin.

`[ym3438/pine64] ... write interface configured` means the GPIO bus is ready.
The adapter is write-only because `/RD` is tied high. Firmware cannot detect
whether a YM3438 is plugged in, powered or producing analog sound. The
external-oscillator entry likewise cannot detect or measure that oscillator.

If the Si5351 does not answer, its bus is stuck, a register readback differs,
or PLLA fails to lock, the menu shows a clock-unavailable notice and returns
before starting demo audio. Each clock-stretch wait is limited to 1 ms;
initialization and PLL-lock polling are each limited to 100 retries. There
is no repeated unbounded missing-chip probe. The bus is released on error.

## N64 controller and Controller Pak

Connect the controller's data signal to **PH9 / Pi-2 pin 13**, its supply to
3.3 V, and its ground to a Pi-2 ground. Add a 2.2–4.7 kOhm external pull-up
from data to **3.3 V**. The driver's weak internal pull-up provides an idle
level; the external resistor is needed for clean 4 microsecond Joybus cells.
Do not use a 5 V pull-up or a push-pull level shifter on this bidirectional
wire.

The driver preloads the GPIO output latch low and switches between output
LOW and input/released. Send pulses, receive sampling and timeouts use the
fixed 24 MHz timer, so they do not assume the V3s 1.2 GHz CPU frequency.
The existing Joybus lock/unlock helpers preserve the IRQ mask around each
complete request/reply transaction. A missing controller times out; a
stuck-low reply is rejected instead of being accepted as a string of zeros.

The menu's USB/serial controls remain available. They do not emulate an N64
memory accessory: the Controller Pak demo talks to the physical controller
on PH9 when an accessory operation is requested.

## Validation

The A64 driver sources and all five hardware demo translation units have
been compiled for Cortex-A53 in AArch32 mode. `tests/external_test.c` runs
the actual C drivers against a GPIO-level host model. It checks successful
Si5351 programming, NACKs, stuck clocks, frozen-timer poll limits, readback
failure, loss of PLL lock, masked YM writes, reset/strobe timing, N64 byte
transmission/reception, and absent/stuck-low controller handling. It also
checks that the external sound driver preserves the SD pin bank and CCU.

These checks verify the port's protocol and MMIO logic. This adapter has
not yet been tested on the user's physical PINE64 with a YM3438, Si5351 or
N64 controller attached. Oscilloscope checks of clock/strobe/receive timing
and an audible test remain the hardware validation step.

## References

- [PINE64 connector pin assignment, 15 February 2016](https://files.pine64.org/doc/Pine%20A64%20Schematic/Pine%20A64%20Pin%20Assignment%20160215.pdf), page 1 Pi-2 and page 2 Euler.
- [PINE A64+ 2 GB Rev C schematic](https://files.pine64.org/doc/Pine%20A64%20Schematic/Pine%20A64plus%202GB%20Rev%20C-20160113_Release.pdf), sheets 9, 10, 12 and 17.
- [Allwinner A64 User Manual v1.1](https://linux-sunxi.org/images/b/b4/Allwinner_A64_User_Manual_V1.1.pdf), PIO, timer and clock register descriptions.
- [Skyworks AN619 rev. 0.8](https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/application-notes/AN619.pdf), Si5351 programming equations, status and output registers.
- [TI SN74HCT245 data sheet](https://www.ti.com/lit/ds/symlink/sn74hct245.pdf), input levels, direction and output enable.
- [RP2040 datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf), section 4.4, pages 501–502: SPI TX does not automatically become high impedance when deselected.
