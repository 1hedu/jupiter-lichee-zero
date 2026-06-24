# Jupiter SDK — Wiring Guide

Pin reference for the **optional peripherals** you can hang off the
Lichee Pi Zero: retro controllers (NES / SNES / Genesis / N64), the
Mars Pico 3D coprocessor, the YM3438 FM chip, the audio mixer
network, and the MIDI breakout.

For **LCD, UART debug, and power** — the three things you wire
before anything in this doc matters — see
[`GETTING_STARTED.md`](GETTING_STARTED.md) section 3.

All pin numbers verified against the Lichee Pi Zero schematic
(`lichee0_base.pdf`).

---

## Lichee Pi Zero Pin Reference

### 2.54mm 30-Pin Header

```
Left Column (odd)          Right Column (even)
─────────────────          ──────────────────
 1: PE1  (LCD_DE)           2: PE21 (UART1_TX)
 3: PE0  (LCD_CLK)          4: PE22 (UART1_RX)
 5: PE20 (CSI_FIELD)        6: PB0  (UART2_TX)
 7: PB2  (UART2_RTS)        8: PB1  (UART2_RX)
 9: PB3  (UART2_CTS)       10: PB4  (PWM0) ← BACKLIGHT!
11: 1V2                    12: PB5  (PWM1)
13: 1V8                    14: PB6  (I2C0_SCK)
15: 3V0                    16: PB7  (I2C0_SDA)
17: MCSI-D0P               18: PB8  (UART0_TX) ← DEBUG!
19: MCSI-D0N               20: PB9  (UART0_RX) ← DEBUG!
21: MCSI-D1P               22: PC1  (SPI_CLK)
23: MCSI-D1N               24: PC3  (SPI_MOSI)
25: MCSI-CKP               26: PC0  (SPI_MISO)
27: MCSI-CKN               28: PC2  (SPI_CS)
29: GND                    30: 5V
```

### J3 SIP8 Header

```
Pin 1: PG0     Pin 5: PG3
Pin 2: PG1     Pin 6: PG4
Pin 3: PG2     Pin 7: GND
Pin 4: PG5     Pin 8: (internal)
```

---

## Part 1: Retro Controllers

All operate at 3.3V. No level shifters needed.

### NES Controller

PF0-PF2 (SDC0 pins — release SD card after boot).

| NES Plug Pin | Signal | GPIO | Physical Location |
|---|---|---|---|
| Pin 3 | OUT (strobe) | PF0 | 2.54mm — see SDC0 pins |
| Pin 2 | CLK | PF1 | 2.54mm — see SDC0 pins |
| Pin 4 | D0 (data) | PF2 | 2.54mm — see SDC0 pins |
| Pin 7 | VCC (3.3V) | — | 2.54mm pin 15 (3V0) |
| Pin 1 | GND | — | 2.54mm pin 29 (GND) |

```
NES Plug (front view):
  ┌─────────────────┐
  │ 1   2   3   4   │
  │ GND CLK OUT D0  │
  │     5   6   7   │
  │         D3  VCC │
  └─────────────────┘
Pins 5-6 are expansion — not needed for standard controller.
```

### SNES Controller

Same 3 wires as NES — same PF0-PF2 pins, 16 bits instead of 8.

| SNES Plug Pin | Signal | GPIO | Physical Location |
|---|---|---|---|
| Pin 3 | OUT (latch) | PF0 | same as NES |
| Pin 2 | CLK | PF1 | same as NES |
| Pin 4 | D0 (data) | PF2 | same as NES |
| Pin 1 | VCC (3.3V) | — | 2.54mm pin 15 (3V0) |
| Pin 7 | GND | — | 2.54mm pin 29 (GND) |

### Genesis Controller (DE-9)

7 signal wires + ground. No VCC needed (passive switches).

**IMPORTANT:** PB4 (pin 10) and PB5 (pin 12) are LCD backlight — DO NOT USE.
D4 and D5 are remapped to PG5 and PB6.

| DE-9 Pin | Signal | SELECT HIGH | SELECT LOW | GPIO | Physical Location |
|---|---|---|---|---|---|
| Pin 1 | D0 | Up | Up | PF0 | SDC0 pin |
| Pin 2 | D1 | Down | Down | PF1 | SDC0 pin |
| Pin 3 | D2 | Left | 0 | PF2 | SDC0 pin |
| Pin 4 | D3 | Right | 0 | PF3 | SDC0 pin |
| Pin 6 | D4 (TL) | B | A | PF4 | SDC0 pin |
| Pin 9 | D5 (TR) | C | Start | PF5 | SDC0 pin |
| Pin 7 | SELECT | (output) | (output) | PE1 | 2.54mm pin 1 |
| Pin 8 | GND | — | — | — | 2.54mm pin 29 |
| Pin 5 | VCC | — | — | — | *not connected* |

```
DE-9 plug (front view, looking at pins):
  ┌───────────────────┐
  │ 1   2   3   4   5 │
  │ D0  D1  D2  D3 VCC│
  │   6   7   8   9   │
  │  D4  SEL GND  D5  │
  └───────────────────┘
NOTE: Pin numbering reverses from solder side.
```

PF0-PF5 are the SD card (TF) pins. They must be released after boot
before controllers can be used. Genesis shares PF0-PF2 with NES/SNES —
use one or the other, not both.

### N64 Controller

1 signal wire + power + ground. Native 3.3V.

| N64 Plug Pin | Signal | GPIO | Physical Location |
|---|---|---|---|
| Pin 1 | Data | PE20 | 2.54mm pin 5 |
| Pin 2 | GND | — | 2.54mm pin 29 |
| Pin 3 | VCC (3.3V) | — | 2.54mm pin 15 (3V0) |

```
N64 plug (front view):
  ┌─────────┐
  │ 1  2  3 │
  │ D GND V │
  └─────────┘
```

### All Controllers — Pin Summary

```
GPIO    Location           Used By
────    ────────           ───────
PF0     SDC0 (post-boot)   NES OUT / Genesis D0
PF1     SDC0               NES CLK / Genesis D1
PF2     SDC0               NES DATA / Genesis D2
PF3     SDC0               Genesis D3
PF4     SDC0               Genesis D4
PF5     SDC0               Genesis D5
PE1     2.54mm pin 1       Genesis SELECT
PE20    2.54mm pin 5       N64 DATA
```

NES + N64 simultaneously: yes (PF0-PF2 + PE20, no overlap).
Genesis + NES/SNES: no (shares PF0-PF2).
All controllers + YM3438: yes (controllers on PF/PE, YM3438 on PB/PG).

---

## Part 2: Jupiter Mars (Pico 3D Coprocessor)

```
┌──────┐  HDMI   ┌──────────┐  ribbon  ┌──────────┐  wires  ┌──────────┐
│ Pico │────────►│ TC358743 │─────────►│FPC break-│────────►│ Lichee Pi│
│(DVI) │  cable  │  board   │  15-pin  │out board │ 7 wires │  2.54mm  │
└──────┘         └──────────┘   FPC    └──────────┘         └──────────┘
```

### Pico DVI Output (PicoDVI)

| Pico GPIO | Signal | HDMI Pin |
|---|---|---|
| GP0 | TMDS D2+ (Blue) | Pin 1 |
| GP1 | TMDS D2- | Pin 3 |
| GP2 | TMDS D1+ (Green) | Pin 4 |
| GP3 | TMDS D1- | Pin 6 |
| GP4 | TMDS D0+ (Red) | Pin 7 |
| GP5 | TMDS D0- | Pin 9 |
| GP6 | TMDS CLK+ | Pin 10 |
| GP7 | TMDS CLK- | Pin 12 |
| GND | Ground | Pins 2,5,8,11,13 |

270Ω series resistors on each TMDS line (GP0-GP7).

### SPI Display List (V3s → Pico)

| Pico GPIO | Signal | V3s GPIO | Lichee Pi 2.54mm Pin |
|---|---|---|---|
| GP8 | SPI1 RX | PC3 (SPI_MOSI) | Pin 24 |
| GP10 | SPI1 SCK | PC1 (SPI_CLK) | Pin 22 |
| GP9 | SPI1 CSn | PC0 (SPI_MISO*) | Pin 26 |
| GND | Ground | GND | Pin 29 |

*PC0 is SPI_MISO on the V3s but we use it as chip select here.
The V3s is SPI master — configure PC0-PC3 as SPI0 in master mode.
Verify V3s SPI0 pin functions match before wiring.

### TC358743 → V3s MIPI CSI-2

15-pin FPC breakout → 7 wires → Lichee Pi 2.54mm header.

| TC358743 FPC Pin | Signal | Lichee Pi 2.54mm Pin |
|---|---|---|
| Pin 3 | D0+ | Pin 17 (MCSI-D0P) |
| Pin 2 | D0- | Pin 19 (MCSI-D0N) |
| Pin 6 | D1+ | Pin 21 (MCSI-D1P) |
| Pin 5 | D1- | Pin 23 (MCSI-D1N) |
| Pin 9 | CLK+ | Pin 25 (MCSI-CKP) |
| Pin 8 | CLK- | Pin 27 (MCSI-CKN) |
| Pin 1,4,7,10 | GND | Pin 29 (GND) |

### TC358743 I2C Configuration

Uses I2C1 (TWI1) on PE21/PE22 — avoids conflict with Genesis D5 on PB6.

| TC358743 FPC Pin | Signal | Lichee Pi 2.54mm Pin |
|---|---|---|
| Pin 11 | SDA | Pin 4 (PE22 / TWI1_SDA) |
| Pin 12 | SCL | Pin 2 (PE21 / TWI1_SCK) |

I2C0 (PB6/PB7) remains available for Si5351 clock generator.

### TC358743 Power

Check your specific board — most have onboard regulators.
If not, supply 3.3V from Lichee Pi 2.54mm pin 15 (3V0).

---

## Complete Pin Map

| GPIO | Location | Used By |
|---|---|---|
| **Controllers (PF + PE)** | | |
| PF0 | SDC0 | NES OUT / Genesis D0 |
| PF1 | SDC0 | NES CLK / Genesis D1 |
| PF2 | SDC0 | NES DATA / Genesis D2 |
| PF3 | SDC0 | Genesis D3 |
| PF4 | SDC0 | Genesis D4 |
| PF5 | SDC0 | Genesis D5 |
| PE1 | 2.54mm pin 1 | Genesis SELECT |
| PE20 | 2.54mm pin 5 | N64 DATA |
| **YM3438 FM Chip (PB + PG)** | | |
| PB0-PB7 | 2.54mm pins 6-16 | D0-D7 data bus |
| PG0 | J3 pin 1 | A0 |
| PG1 | J3 pin 2 | A1 |
| PG2 | J3 pin 3 | /WR |
| PG3 | J3 pin 5 | /CS |
| PG4 | J3 pin 6 | /IC (reset) |
| **I2C** | | |
| PB6 | 2.54mm pin 14 | I2C0 SCK (Si5351) |
| PB7 | 2.54mm pin 16 | I2C0 SDA (Si5351) |
| PE21 | 2.54mm pin 2 | I2C1 SCK (TC358743) |
| PE22 | 2.54mm pin 4 | I2C1 SDA (TC358743) |
| **SPI to Pico** | | |
| PC0 | 2.54mm pin 26 | SPI CS |
| PC1 | 2.54mm pin 22 | SPI CLK |
| PC3 | 2.54mm pin 24 | SPI MOSI |
| **MIPI CSI-2 (TC358743)** | | |
| MCSI-D0P/N | 2.54mm pins 17/19 | Data Lane 0 |
| MCSI-D1P/N | 2.54mm pins 21/23 | Data Lane 1 |
| MCSI-CKP/N | 2.54mm pins 25/27 | Clock |
| **Reserved** | | |
| PB4 | 2.54mm pin 10 | YM3438 D4 / LCD backlight |
| PB5 | 2.54mm pin 12 | YM3438 D5 / LCD backlight |
| PB8/PB9 | 2.54mm pins 18/20 | UART0 (debug serial) |
| PE0 | 2.54mm pin 3 | LCD_CLK (required) |
| PE2-PE23 | FPC40 | LCD RGB data + sync |

Controllers + YM3438 + Pico coprocessor + LCD — all simultaneously, zero conflicts.

PF0-PF5 are SDC0 (TF card) — release after boot before using controllers.
PB6/PB7 shared between YM3438 data bus and I2C0 — init Si5351 at boot first.
NES + N64 simultaneously: yes. Genesis exclusive with NES/SNES (shares PF0-PF2).

### YM3438 Wiring (DIP-24)

```
            YM3438 (DIP-24, top view)
            ┌──────────┐
      GND  1│          │24  φM (master clock)
       D0  2│          │23  Vcc (+5V)
       D1  3│          │22  AVcc (+5V)
       D2  4│          │21  MOL (audio L)
       D3  5│          │20  MOR (audio R)
       D4  6│          │19  AGND
       D5  7│          │18  A1
       D6  8│          │17  A0
       D7  9│          │16  /IC (reset)
      /RD 10│          │15  /CS
      /WR 11│          │14  TEST
      GND 12│          │13  /IRQ
            └──────────┘
```

| YM3438 Pin | Signal | Lichee Pi | Notes |
|---|---|---|---|
| 1 (GND) | Ground | 2.54mm pin 29 | |
| 2 (D0) | Data bit 0 | PB0 — 2.54mm pin 6 | |
| 3 (D1) | Data bit 1 | PB1 — 2.54mm pin 8 | |
| 4 (D2) | Data bit 2 | PB2 — 2.54mm pin 7 | |
| 5 (D3) | Data bit 3 | PB3 — 2.54mm pin 9 | |
| 6 (D4) | Data bit 4 | PB4 — 2.54mm pin 10 | See backlight fix below |
| 7 (D5) | Data bit 5 | PB5 — 2.54mm pin 12 | See backlight fix below |
| 8 (D6) | Data bit 6 | PB6 — 2.54mm pin 14 | Shared with I2C0 SCK |
| 9 (D7) | Data bit 7 | PB7 — 2.54mm pin 16 | Shared with I2C0 SDA |
| 10 (/RD) | Read strobe | Tie to **+5V** | Never read from chip |
| 11 (/WR) | Write strobe | PG2 — J3 pin 3 | Active low |
| 12 (GND) | Ground | 2.54mm pin 29 | |
| 13 (/IRQ) | Interrupt out | *not connected* | Unused |
| 14 (TEST) | Test pin | *not connected* | Leave floating |
| 15 (/CS) | Chip select | PG3 — J3 pin 5 | Active low |
| 16 (/IC) | Reset | PG4 — J3 pin 6 | Active low, pulse at boot |
| 17 (A0) | Address bit 0 | PG0 — J3 pin 1 | |
| 18 (A1) | Address bit 1 | PG1 — J3 pin 2 | |
| 19 (AGND) | Analog ground | GND | Separate from digital GND if possible |
| 20 (MOR) | Audio right | Headphone/amp R | Analog output |
| 21 (MOL) | Audio left | Headphone/amp L | Analog output |
| 22 (AVcc) | Analog +5V | +5V | Decouple with 100nF to AGND |
| 23 (Vcc) | Digital +5V | +5V | Decouple with 100nF to GND |
| 24 (φM) | Master clock | Si5351 CLK0 | 7.670453 MHz (NTSC) or 7.600489 MHz (PAL) |

**Power:** +5V to pins 22 and 23. 100nF ceramic decoupling caps on both,
close to the chip. 3.3V GPIO from the V3s drives the data/control pins
directly — the YM3438 has TTL-compatible inputs (VIH = 2.0V).

**Clock:** Si5351 CLK0 provides the master clock at 7.670453 MHz (NTSC
Genesis frequency). Configured over I2C0 (PB6/PB7) at boot before
PB6/PB7 switch to YM3438 data bus duty. The Si5351 free-runs after
configuration — no ongoing I2C needed.

**Audio output:** MOL (pin 21) and MOR (pin 20) are analog. Route to
a shared stereo amplifier alongside the V3s headphone output:

```
V3s HP_L ──10kΩ──┐
                  ├── Amp L in
YM3438 MOL ─10kΩ─┘

V3s HP_R ──10kΩ──┐
                  ├── Amp R in
YM3438 MOR ─10kΩ─┘
```

Four resistors, one amp. Both sources mix analog — no ADC, no
latency, stereo preserved. V3s software audio + real FM out the
same speaker.

NOTE: The YM3438 DAC output is ~4.9Vpp (nearly rail-to-rail 5V,
9-bit DAC, per datasheet). The V3s headphone output is ~2.8Vpp
typical (1Vrms, Allwinner codec). The YM3438 is nearly 2x hotter.
Use a larger resistor on the YM3438 side to attenuate it down:
try 22kΩ from YM3438 and 10kΩ from V3s HP, or adjust to taste.
The YM3438 output needs a reconstruction LPF on MOL/MOR to remove
DAC aliasing artifacts (~53kHz sampling images).

Stock Genesis Model 2 (VA0) uses a 2nd-order Sallen-Key active LPF:
  2× 10kΩ + 5600pF shunt + 1000pF feedback, BA10358 op-amp
  fc ≈ 6.7kHz, Q ≈ 1.18 — known to sound muffled

Recommended for Jupiter (cleaner, preserves high frequencies):
  Simple passive RC per channel: 10kΩ + 680pF → fc ≈ 23kHz
  Just kills DAC aliasing, doesn't color the sound.

### PB4/PB5 Backlight Fix

PB4 (PWM0) and PB5 (PWM1) route through the FPC40 to the LCD panel's
backlight control. The YM3438 data bus toggles these pins at MHz speeds
during register writes, which would flicker the backlight.

The backlight is driven by a **PT4101** boost converter (SOT-23-5 package)
on the Lichee Pi Zero board, near the FPC40 LCD connector. It boosts
3.3V→18V to drive the backlight LEDs in series. PB4 connects to pin 4
(EN) of the PT4101.

```
PT4101 pinout (SOT-23-5, top view):
  ┌─────┐
  │1   5│   Pin 1: SW (inductor)
  │2   4│   Pin 2: GND
  │  3  │   Pin 3: FB (current sense)
  └─────┘   Pin 4: EN ← PB4 connects here
            Pin 5: OV (overvoltage)
            Pin 6: IN (5V power)
```

Nearby components: L2 (2.2μH inductor), D1 (1N5819 diode),
R21 (5.1Ω current sense), C11 (10μF/50V output cap).

**Fix:** solder a wire from PT4101 pin 4 (EN) to a nearby 3.3V pad.
This keeps the boost converter always enabled. PB4 still connects to
EN via its board trace, but the 3.3V wire holds EN high regardless of
what PB4 does. The YM3438 data bus can toggle PB4 freely — the wire
overrides it.

No trace cuts needed. One wire. Find the PT4101 (smallest SOT-23 near
the FPC40 connector with the inductor and diode nearby), solder to its
pin 4 pad.
