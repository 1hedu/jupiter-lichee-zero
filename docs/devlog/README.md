# Devlog

Working notes, status reports, brainstorms, and reverse-engineering
findings produced while building the SDK. **Not part of the user-facing
documentation set** — these are kept for historical context, lore, and
"this is why the code looks like this" answers when something breaks.

| Document | What it is |
|---|---|
| `PROGRESS.md` | Historical optimization log + per-subsystem benchmark notes. Includes register dumps and "we tried X, got Y" entries. |
| `HANDOFF.md` | Memo to the next dev — design notes for input + audio bring-up, with code sketches. Some content is now superseded by the actual implementation. |
| `V3S_HARDWARE_STATUS.md` | One-page status report on what V3s features are wired up vs. dormant. |
| `V3S_HARDWARE_SECRETS.md` | Reverse-engineering notes on V3s hardware blocks (NEON, CedarVE, crypto, audio). Catalog of feasibility for advanced tricks. |
| `MT32_AUDIO_STATUS.md` | Status report from MT-32 emulator integration. |
| `GENESIS_MIDI_MODULE.md` | Design doc for an unreleased 3-chip FM synthesizer module. |
| `YM3438_CRYSTAL_CLOCK.md` | Hardware tweak: oscillator solution for YM3438 to eliminate clock software. |
| `LRADC_GAMEPAD_IDEA.md` | Brainstorm: single-pin gamepad via resistor ladder. |

For user-facing reference, see `../GAME_DEV_TRICK_BIBLE.md`,
`../JUPITER_HARDWARE_REFERENCE.md`, etc. one level up.
