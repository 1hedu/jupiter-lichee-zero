# MIDI Hardware Bring-up Guide

Wires up real MIDI IN/OUT on the Lichee Pi Zero via UART1 (PE21 = TX,
PE22 = RX). Required for the FB-01 / MT-32 / WaveTerm editors to
drive actual synth hardware. Used by Overlay 6.

---

## Bill of Materials

| Part | Qty | Notes |
|---|---|---|
| 5-pin DIN-5 socket | 2 | One for IN, one for OUT |
| 6N138 optocoupler (DIP-8) | 1 | MIDI input isolation (required by MIDI spec) |
| 1N4148 diode | 1 | Reverse-current protection on optocoupler |
| 220Ω resistor | 4 | Two on IN, two on OUT |
| 4.7kΩ resistor | 1 | 6N138 output pull-up |
| 74HCT04 hex inverter (DIP-14) | 1 | Optional but recommended: 3.3V → 5V buffer on TX. Skip if you're driving a short cable to a cooperative receiver. |

Total cost: ~$5 in parts from any electronics supplier.

---

## Wiring

### MIDI OUT (V3s → external synth)

```
                                          DIN-5 (looking at solder side)
PE21 (UART1_TX, 3.3V) ──► 74HCT04 ──┬─ 220Ω ──┤ pin 5 (data)
                                    │
                          +5V ──── 220Ω ─────┤ pin 4
                          GND ─────────────── ┤ pin 2 (shield)
                                              └─ pin 1, 3 unused
```

If you skip the 74HCT04 (TX direct from PE21 via 220Ω), the signal is
3.3V instead of the 5V the MIDI spec calls for — works in practice
with most receivers over short runs, but the inverter buffer is the
safer choice for long cables or pickier devices.

### MIDI IN (external synth → V3s)

```
DIN-5 (looking at solder side)
pin 4 ──────────► 6N138 pin 2 (anode)
pin 5 ── 220Ω ──► 6N138 pin 3 (cathode)

           ┌──── 1N4148 ────► 6N138 pin 2 (reverse protection)
           │
           ▼
6N138 pin 3 ◄─────┘

+3.3V ──── 4.7kΩ ──┬──► 6N138 pin 6 (output)  ──► PE22 (UART1_RX, 3.3V)
                   │
+3.3V ─────────────┴──► 6N138 pin 8 (VCC)
GND   ────────────────► 6N138 pin 5 (GND)
```

The 6N138 inverts; the MIDI IN spec accounts for this (idle is
"current flowing" which the opto sees as logic high — UART expects
that idle level too, so the inversion cancels out at the receiver).

---

## Bring-up Sequence

1. **Build the OUT side first** — fewer parts, easier to verify. Wire
   the DIN-5 socket and the 220Ω resistors. Skip the 74HCT04 for the
   first test if you don't have one yet.
2. **Connect MIDI OUT to a known-good MIDI input** — a USB MIDI cable
   into your computer running a MIDI monitor (e.g. MIDI-OX on
   Windows, SnoizeMIDIMonitor on Mac). Anything that displays raw
   incoming bytes.
3. **Flash the FB-01 editor, press Z on the VOICE tab.** Should see a
   SysEx packet appear in your MIDI monitor (138 bytes starting with
   F0 43 75 ...).
4. **Confirm UART0 log matches** — the editor logs the same bytes to
   UART0 for cross-check.
5. **Build the IN side** — wire the 6N138 + DIN-5 + resistors.
6. **Loopback test** — physically connect your MIDI OUT DIN to your
   MIDI IN DIN with a standard MIDI cable. Flash the editor, press
   Z. The UART0 log should now show both `[midi tx]` (your send) AND
   `[midi rx] sysex N bytes` (the echoed-back packet).
7. **Wire up the real synth** — connect Lichee MIDI OUT → synth MIDI
   IN. Press Z to send a voice. Synth should accept the patch (you
   may need to confirm on its panel depending on the synth's memory
   protect setting).

---

## Verification Checks

- **Idle voltage on PE21**: ~3.3V (UART idle is high)
- **Idle voltage on PE22**: ~3.3V (opto idle is current-flowing, output
  goes high via pull-up)
- **Pressing Z (send) on the editor**: PE21 toggles for the packet
  duration; if you scope it, you'll see 10-bit UART frames at 31250
  baud (~32 µs per byte, ~5 ms for a 138-byte voice dump).

---

## Common Failure Modes

| Symptom | Likely Cause | Fix |
|---|---|---|
| Synth sees nothing | TX wire reversed / DIN pin 4 vs 5 swapped | Check pinout |
| Garbled bytes received | Baud rate wrong, or noise on wire | Verify 31250 baud, shorten cable, add ferrite |
| Editor sees no RX | 6N138 not powered, or pull-up missing | Check pin 8 = 3.3V, pin 6 has 4.7kΩ to 3.3V |
| RX dropping bytes mid-SysEx | Ring buffer overflow | `RX_RING_SIZE` is 1024 in `lib/midi.c` — bump if needed |
| `midi_init` hangs at boot | UART1 clock gate not enabled | Check `CCU_BUS_CLK_GATE3` bit 17 = 1 after init |

---

## Related Files

- `lib/midi.c` — driver implementation
- `include/midi.h` — public API
- `include/v3s.h` — UART1 register + IRQ definitions
- `docs/PIN_OVERLAYS.md` — Overlay 6 pin assignments
- `examples/fb01_editor/main.c` — first editor wired to real MIDI
- `examples/mt32_editor/main.c` — second editor
- `examples/waveterm/main.c` — third editor (Behringer Wave target)
