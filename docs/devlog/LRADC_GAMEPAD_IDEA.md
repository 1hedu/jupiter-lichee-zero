# LRADC Custom Gamepad — Future Idea

The V3s LRADC (KEY-ADC) is a 6-bit ADC that reads voltage levels from a
resistor ladder. One analog pin, multiple buttons, each creating a unique
voltage. Currently used for 4 board buttons (VOL+, VOL-, SELECT, START).

## What It Can Do

- Custom Jupiter gamepad on a **single pin**
- 6-8 buttons with a redesigned resistor ladder
- 250Hz sample rate (4ms response — fine for gaming)
- D-pad + A/B/Start/Select on one wire

## What It Cannot Do

- Interface with any existing retro controller (NES, SNES, Genesis, N64)
- Those all need digital GPIO with specific timing protocols
- An ADC can't do serial clock/data or parallel reads

## Limitations

- Limited simultaneous button detection (resistor ladder gives one voltage)
- Clever resistor values can encode specific combos but not arbitrary ones
- 6-bit resolution = 64 voltage levels max
- Resistor tolerance makes >8-10 reliable button combos impractical

## If We Do This

- Redesign the resistor network for gamepad layout
- Use the existing KEYADC0 pin (right 2.54mm header)
- Software: read ADC, map voltage ranges to button bitmask
- Zero GPIO cost — frees all digital pins for other peripherals
