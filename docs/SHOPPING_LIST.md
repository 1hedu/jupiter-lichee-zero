# Jupiter SDK — Shopping List

## Immediate (order now)

| Part | Marking | Qty | For |
|---|---|---|---|
| 8MHz 5V TTL crystal oscillator module (DIP-8) | — | 1 | YM3438 clock (replaces CCU MCLK, frees PE1) |
| 15-pin FPC breakout board (1mm pitch) | — | 1 | TC358743 CSI → Lichee Pi wiring |

## Audio Mixing (have resistors? check stash)

| Part | Marking | Qty | For |
|---|---|---|---|
| 22kΩ resistor | 22K | 2 | YM3438 MOL/MOR to amp (attenuate FM) |
| 10kΩ resistor | 10K | 2 | V3s HP_L/HP_R to amp |
| 100nF / 0.1μF ceramic cap | 104 | 2 | DC blocking before amp input |
| 10kΩ resistor | 10K | 2 | LPF per YM3438 output channel |
| 680pF ceramic cap | 681 | 2 | LPF per YM3438 output channel (fc ≈ 23kHz) |

## Already Have

| Part | Status |
|---|---|
| TC358743 HDMI-to-CSI board | Have it |
| YM3438 (DIP-24) × 1 | Wired and working |
| Amplifier | Working (tested with FM output) |
| N64 controller | Wired and working |
| NES controller | Wired and working |
| Genesis controller | Wired and working |
| HDMI breakout connector | Need to check |

## Future — Genesis MIDI Module

| Part | Marking | Qty | For |
|---|---|---|---|
| YM3438 (DIP-24) | — | 2 more | 18-voice FM (3 chips total) |
| 8MHz oscillator module | — | 0 | Shared from single oscillator |
| 6N138 optocoupler | — | 1 | MIDI input isolation |
| 1N4148 diode | — | 1 | MIDI input protection |
| 5-pin DIN jack | — | 1 | MIDI connector |
| 220Ω resistor | 221 | 1 | MIDI input |
| 4.7kΩ resistor | 4K7 | 1 | 6N138 pull-up |
| 100nF / 0.1μF ceramic cap | 104 | 4 more | Decoupling (2 per extra chip) |
| 22kΩ resistor | 22K | 4 more | Audio mixer (2 per extra chip) |
| 10kΩ resistor | 10K | 4 more | LPF (2 per extra chip) |
| 680pF ceramic cap | 681 | 4 more | LPF (2 per extra chip) |

## Future — Mars (Pico 3D Coprocessor)

| Part | Marking | Qty | For |
|---|---|---|---|
| HDMI breakout connector | — | 1 | PicoDVI output from Pico |
| 270Ω resistor | 271 | 8 | TMDS impedance matching (GP0-GP7) |

## Capacitor Cross-Reference

| Value | Ceramic marking | Also sold as |
|---|---|---|
| 100nF | 104 | 0.1μF |
| 680pF | 681 | — |
| 10μF | 106 | — |
| 10nF | 103 | 0.01μF |
| 1nF | 102 | 1000pF |
