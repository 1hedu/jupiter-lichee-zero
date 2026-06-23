# Jupiter SDK — Pin Overlay Configurations

Different hardware setups require different pin assignments. Each overlay
defines a complete pin configuration — like Linux device tree overlays
but for bare-metal GPIO init.

Select one overlay at build time or runtime. Each overlay documents
exactly which peripherals are active and which pins they use.

---

## Overlay 0: All Three Controllers Simultaneously

The original tested config. NES + Genesis + N64 all wired at once.
No YM3438, no Pico. Emulated audio only.

```
NES/SNES:
  PG0       OUT (strobe)              (J3 pin 1)
  PG1       CLK                       (J3 pin 2)
  PG2       DATA                      (J3 pin 3)

Genesis:
  PB0       D0 (Up)                   (2.54mm pin 6)
  PB1       D1 (Down)                 (2.54mm pin 8)
  PB2       D2 (Left)                 (2.54mm pin 7)
  PB3       D3 (Right)                (2.54mm pin 9)
  PG5       D4 (TL: B/A)             (J3 pin 4)
  PB6       D5 (TR: C/Start)         (2.54mm pin 14)
  PG3       SELECT                    (J3 pin 5)

N64:
  PG4       DATA                      (J3 pin 6)

PB4/PB5   DO NOT USE (LCD backlight)
PF0-PF5   free
PE1/PE20  free
```

Supports: NES + Genesis + N64 all at once (tested, working).
PB4/PB5 conflict with LCD backlight — avoided.
PB6 shared with I2C0 SCK — init Si5351 at boot first if needed.

---

## Overlay 1: Controllers Only (YM3438-compatible)

Controllers remapped to PF/PE, freeing PB/PG for future YM3438.

```
PF0    NES OUT / Genesis D0          (SDC0, release after boot)
PF1    NES CLK / Genesis D1
PF2    NES DATA / Genesis D2
PF3    Genesis D3
PF4    Genesis D4
PF5    Genesis D5
PE1    Genesis SELECT                (LCD_DE unused)
PE20   N64 DATA                      (CSI_FIELD unused)

PB0-PB7   free
PG0-PG5   free (J3 header)
```

Supports: NES, SNES, Genesis (3+6 button), N64
NES + N64 simultaneous: yes
Genesis exclusive with NES/SNES (shares PF0-PF2)

---

## Overlay 2: YM3438 + 2× N64

Real FM synthesis + two N64 controllers for 2-player.

```
YM3438:
  PB0-PB7   D0-D7 data bus           (2.54mm header)
  PG0       A0                        (J3 pin 1)
  PG1       A1                        (J3 pin 2)
  PG2       /WR                       (J3 pin 3)
  PG3       /CS                       (J3 pin 5)
  PG4       /IC (reset)               (J3 pin 6)
  PE1       φM clock (8.0 MHz)        (CCU MCLK output, no Si5351)

N64 Controllers:
  PE20      N64 Player 1              (2.54mm pin 5)
  PG5       N64 Player 2              (J3 pin 4)

Free:
  PF0-PF5   available (SDC0)
```

Clock: CCU drives PE1 as CSI_MCLK at 600MHz/75 = 8.0 MHz exactly.
No Si5351 needed. YM3438 datasheet spec: 7.7-8.3 MHz, typ 8.0 MHz.

VGM files from Genesis games assume 7.670453 MHz clock. At 8.0 MHz,
all notes are ~4.3% sharp (~0.7 semitone). Options:
  - Accept the pitch shift (barely noticeable in isolation)
  - Recalculate F-number registers: F_new = F_orig × 7.670453 / 8.0
  - Use Si5351 for exact 7.670453 MHz (requires I2C0 on PB6/PB7,
    conflicts with data bus — init Si5351 at boot, then switch PB6/PB7)

Audio: YM3438 MOL/MOR mixed with V3s HP output through resistive
mixer into shared amplifier (see WIRING_GUIDE.md).

Backlight: wire LCD backlight A/K directly to 3.3V power (always on).
PB4/PB5 freed for YM3438 D4/D5.

---

## Overlay 3: YM3438 + Si5351 + 2× N64

Same as Overlay 1 but with exact Genesis clock from Si5351.

```
YM3438:
  PB0-PB7   D0-D7 data bus
  PG0-PG4   A0, A1, /WR, /CS, /IC

Clock:
  PB6       I2C0 SCK (Si5351 init at boot, then → YM3438 D6)
  PB7       I2C0 SDA (Si5351 init at boot, then → YM3438 D7)
  Si5351 CLK0 → YM3438 φM pin (7.670453 MHz, free-runs after config)

N64 Controllers:
  PE20      N64 Player 1
  PG5       N64 Player 2

PE1         free (or Genesis SELECT if adding controller support)
PF0-PF5    free
```

Boot sequence:
  1. Init I2C0 on PB6/PB7
  2. Configure Si5351 for 7.670453 MHz on CLK0
  3. Switch PB6/PB7 to GPIO output (YM3438 D6/D7)
  4. Si5351 free-runs — no further I2C needed

---

## Overlay 4: Mars (Pico Coprocessor) + Controllers

3D polygon coprocessor + retro controllers, no YM3438.

```
Pico (via TC358743 HDMI-to-CSI bridge):
  PC0       SPI CS (to Pico GP9)
  PC1       SPI CLK (to Pico GP10)
  PC3       SPI MOSI (to Pico GP8)
  MCSI-D0P/N  MIPI CSI-2 Data 0      (2.54mm pins 17/19)
  MCSI-D1P/N  MIPI CSI-2 Data 1      (2.54mm pins 21/23)
  MCSI-CKP/N  MIPI CSI-2 Clock       (2.54mm pins 25/27)
  PE21      I2C1 SCK (TC358743 config)
  PE22      I2C1 SDA (TC358743 config)

Controllers:
  PF0-PF5   NES/SNES/Genesis          (SDC0, release after boot)
  PE1       Genesis SELECT
  PE20      N64 DATA

Free:
  PB0-PB7   available
  PG0-PG5   available (J3)
```

---

## Overlay 5: Mars + YM3438 + 2× N64

The full stack — 3D coprocessor, real FM, two controllers.

```
Pico:
  PC0-PC3   SPI to Pico
  MCSI-*    MIPI CSI-2 (TC358743)
  PE21/PE22 I2C1 (TC358743)

YM3438:
  PB0-PB7   D0-D7 data bus
  PG0-PG4   A0, A1, /WR, /CS, /IC
  PE1       φM clock (8.0 MHz from CCU MCLK)

N64:
  PE20      Player 1
  PG5       Player 2

Free:
  PF0-PF5   available
```

Everything simultaneously. No conflicts.

---

## Pin Quick Reference

| Pin | OVL 0 | OVL 1 | OVL 2 | OVL 4 | OVL 5 |
|---|---|---|---|---|---|
| PB0-PB7 | Genesis+free | free | YM3438 D0-D7 | free | YM3438 D0-D7 |
| PG0-PG4 | NES+Gen ctrl | free | YM3438 ctrl | free | YM3438 ctrl |
| PG5 | Gen D4 | free | N64 P2 | free | N64 P2 |
| PF0-PF5 | free | Controllers | free | Controllers | free |
| PE1 | free | Gen SELECT | YM3438 φM | Gen SELECT | YM3438 φM |
| PE20 | free | N64 | N64 P1 | N64 | N64 P1 |
| PE21/22 | free | free | free | TC358743 I2C | TC358743 I2C |
| PC0-PC3 | free | free | free | SPI to Pico | SPI to Pico |
| MCSI | free | free | free | TC358743 CSI | TC358743 CSI |
| PB8/PB9 | UART0 | UART0 | UART0 | UART0 | UART0 |
