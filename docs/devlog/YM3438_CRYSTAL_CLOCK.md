# YM3438 Crystal Oscillator Clock — Plan

## The Insight

An 8MHz crystal oscillator module wired directly to the YM3438 φM pin
eliminates ALL clock-related GPIO and software complexity:
- No PE1 (CCU MCLK) needed → PE1 freed for Genesis SELECT or other use
- No Si5351 I2C setup → no boot-time I2C dance, no PB6/PB7 sharing
- No CCU register configuration → no CSI bus gate, reset, DRAM gate
- No video_init() ordering constraints → init in any order
- Zero software, zero GPIO, zero pin conflicts

## Parts

| Part | Description | Cost |
|---|---|---|
| 8MHz crystal oscillator module | DIP-8 or DIP-14 can, 5V, TTL output | ~$0.50 |

NOT a bare crystal (those need two pins + external caps + inverter).
A crystal oscillator MODULE has everything built in — just power it
and it outputs a clean square wave.

Common part numbers:
- ECS-2200B-080 (DIP-8)
- SG-8002JA 8.000MHz
- Any "8MHz 5V TTL oscillator" on AliExpress/DigiKey

## Wiring

```
Crystal Oscillator Module (DIP-8):
  Pin 1: NC (or enable — tie to VCC if present)
  Pin 4: GND
  Pin 5: Output → YM3438 pin 24 (φM)
  Pin 8: VCC (+5V)

  100nF decoupling cap between pin 8 and pin 4.
```

One chip, one cap, two power wires, one signal wire. Done.

```
+5V ──┬──────── Oscillator VCC (pin 8)
    100nF
GND ──┴──────── Oscillator GND (pin 4)

Oscillator OUT (pin 5) ──── YM3438 φM (pin 24)
```

## What This Frees

With the clock off the V3s entirely, PE1 returns to the GPIO pool.
This changes the overlay structure significantly:

### Updated Overlay 2: YM3438 + Crystal + All Controllers

```
YM3438:
  PB0-PB7    D0-D7 data bus
  PG0-PG4    A0, A1, /WR, /CS, /IC
  8MHz osc   → φM (pin 24) — NO V3s GPIO USED

Controllers (restored — PE1 available again):
  PF0-PF5    NES/SNES/Genesis D0-D5
  PE1        Genesis SELECT
  PE20       N64 Player 1
  PG5        N64 Player 2

Free:
  PE21/PE22  available (I2C1 or UART1 for MIDI)
  PC0-PC3    available (SPI for Pico Mars)
```

Controllers AND YM3438 AND video ALL work simultaneously.
No pin conflicts. No software clock init. No overlay tradeoffs.

### Updated MIDI Module (3 chips)

```
YM3438 × 3:
  PB0-PB7    shared D0-D7
  PG0-PG2    A0, A1, /WR (shared)
  PG4        /IC (shared)
  PG3        /CS chip 0
  PG5        /CS chip 1
  PF0        /CS chip 2
  8MHz osc   → all three φM pins (shared clock)

MIDI:
  PE21       UART1 TX (MIDI thru)
  PE22       UART1 RX (MIDI in)

Controller:
  PE20       N64 (panic button)

Audio:
  3× MOL/MOR → resistor mixer → amp
```

## Software Changes

Remove from ym3438_hw.c:
- `ym3438_ccu_mclk_init()` function
- CSI bus gate / reset / DRAM gate writes
- PE1 pin mux configuration
- `ym3438_hw_set_clock()` API (clock source is now always external)
- The clock frequency measurement code

The entire clock section becomes a no-op. `ym3438_hw_init()` just
does GPIO setup + chip reset. The oscillator free-runs from power-on.

## F-Number Scaling

Still needed. VGM files assume 7.670453 MHz. At 8.0 MHz, notes are
~4.3% sharp. The F-Number scaling in `ym3438_hw_vgm_write()` stays:

  F_new = F_orig × 7.670453 / 8.0 = F_orig × 0.9588

If exact pitch is ever needed, swap the 8MHz oscillator for a
7.6704 MHz unit (available as custom-frequency oscillators, ~$2).

## Comparison

| Approach | Cost | GPIO used | Software | Pin conflicts |
|---|---|---|---|---|
| Si5351 | $2 | PB6/PB7 (I2C) | I2C init at boot | PB6 shared with data bus |
| CCU MCLK on PE1 | $0 | PE1 | CCU register config | PE1 lost, video ordering |
| **8MHz oscillator** | **$0.50** | **none** | **none** | **none** |
