# FB-01 Sound Editor Specification

## 1. Screen / Window Inventory

The editor presents 5 major tabbed screens within a single main window (632x632 pixels):

1. **Configuration Tab** — MIDI port selection, virtual keyboard settings, and global system parameters. Choose MIDI IN/OUT/CTRL devices; set virtual keyboard MIDI channel and velocity; enable port routing (IN→OUT and CTRL→OUT). Embed a QConfig widget for global and set configuration.

2. **Automations Tab** — Automation recording and playback. Embed a QAutomation widget.

3. **Banks Tab** — 8 voice bank librarian (4 banks per page, 2 pages). Each bank displays 48 voices in a list with name and style. Operations: select voice, copy/paste/exchange between banks, load to/from hardware, page navigation.

4. **Current Set Tab** — 8 instruments per set, split across 2 pages (4 instruments per page). Each instrument is a vertical strip showing channel, notes, bank/voice selection, transpose, detune, LFO enable, poly/mono, portamento, pitch bender, PMD control, volume (slider), and pan (slider).

5. **Current Voice Tab** — Full voice editor. Three sections:
   - Left: Voice name, style (dropdown: Piano, Keys, Organ, Guitar, Bass, Orch, Brass, Wood, Synth, Pad, Ethnic, Bells, Rythm, SFX, Other), feedback (0..7), transpose (-128..127), mono/poly button, portamento (0..127), pitch bender range (0..12), PMD control (dropdown).
   - Center: Algorithm selection (1..8) with visual diagram, algorithm number display.
   - Right: LFO configuration (LFO speed 0..255, wave dropdown, load/sync buttons, AMD 0..127, AMS 0..3, PMD 0..127, PMS 0..7).
   - Bottom of info panel: Author, Comments (metadata not transmitted).

6. **Current Operators Tab** — 4 operator editors (2x2 grid). Each operator widget shows:
   - On/Off button
   - Envelope graph (ADSR: Attack Rate, Decay1 Rate, Sustain Level, Decay2 Rate, Release Rate)
   - Level Velocity (0..31), Attack Velocity (0..3)
   - Coarse (0..3), Multiple (0..15), Fine (-4..3)
   - Carrier/Modulator button
   - Output level slider (0..127), Level curve combo (-Lin, +Lin, -Exp, +Exp)
   - Level Depth (0..15), Rate Depth (0..3), Adjust Volume (0..15)

Plus a virtual on-screen keyboard widget at the bottom for note input.

---

## 2. Per-Screen Field Tables

### Configuration Screen

| Field | Type | Range | Display | Default | SysEx Param ID |
|-------|------|-------|---------|---------|-----------------|
| MIDI IN Device | Dropdown | enumerated | device name | (first) | N/A (UI only) |
| MIDI OUT Device | Dropdown | enumerated | device name | (first) | N/A (UI only) |
| MIDI CTRL Device | Dropdown | enumerated | device name | (first) | N/A (UI only) |
| IN→OUT Relay | Checkbox | bool | Enable/Disable | true | N/A (UI only) |
| CTRL→OUT Relay | Checkbox | bool | Enable/Disable | true | N/A (UI only) |
| Virtual Keyboard MIDI Channel | QButton spinbox | 1..16 | 1..16 | 1 | N/A (UI only) |
| Virtual Keyboard Velocity | QButton spinbox | 0..127 | 0..127 | 100 | N/A (UI only) |
| Keyboard Layout | Radio buttons | QWERTY / AZERTY | label | QWERTY | N/A (UI only) |

### Configuration → Global Config Subpanel

| Field | Type | Range | Display | Default | SysEx Param ID |
|-------|------|-------|---------|---------|-----------------|
| System Channel | QButton spinbox | 1..16 | 1..16 | 1 | 0x00 (F1 system param) |
| Set Number | QButton spinbox | 1..20 | 1..20 | 1 | N/A (UI state only) |
| Master Detune | QButton spinbox | -64..63 | -64..63 | 0 | 0x01 (F1 system param) |
| Memory Protect | Checkbox | bool | Enable/Disable | false | 0x02 (F1 system param) |
| Master Volume | QSlider | 0..127 | 0..127 | 127 | 0x07 (channel CC) |

### Voice Editor Screen

| Field | Type | Range | Display | Default | SysEx Param ID |
|-------|------|-------|---------|---------|-----------------|
| Name | Text | string (≤7 chars) | text | "" | N/A (metadata) |
| Style | Dropdown | Piano..Other (16 values) | label | Piano | 0x01 (voice param) |
| Algorithm | QButton spinbox | 1..8 | 1..8 | 1 | 0x00 (voice param) |
| Feedback | QButton spinbox | 0..7 | 0..7 | 0 | 0x02 (voice param) |
| Transpose | QButton spinbox | -128..127 | -128..127 | 0 | 0x03 (voice param) |
| Mono/Poly | Button toggle | Mono / Poly | Mono/Poly | Poly | 0x04 (voice param) |
| Portamento Time | QButton spinbox | 0..127 | 0..127 | 0 | 0x05 (voice param) |
| Pitch Bender Range | QButton spinbox | 0..12 | 0..12 semitones | 0 | 0x06 (voice param) |
| PMD Control | Dropdown | Not assigned / After touch / Mod wheel / Breath / Foot | label | Not assigned | 0x07 (voice param) |
| LFO Speed | QButton spinbox | 0..255 | 0..255 | 0 | 0x08 (voice param) |
| LFO Wave | Dropdown | Saw / Square / Triangle / S&H | label | Saw | 0x09 (voice param) |
| LFO Load | Button toggle | Load / Don't Load | Load state | false | 0x0A (voice param) |
| LFO Sync | Button toggle | Sync / Free | Sync state | false | 0x0B (voice param) |
| AMD | QButton spinbox | 0..127 | 0..127 | 0 | 0x0C (voice param) |
| AMS | QButton spinbox | 0..3 | 0..3 | 0 | 0x0D (voice param) |
| PMD | QButton spinbox | 0..127 | 0..127 | 0 | 0x0E (voice param) |
| PMS | QButton spinbox | 0..7 | 0..7 | 0 | 0x0F (voice param) |
| Enable Operator 1..4 | Button toggle | On / Off | On/Off state | true | 0x10..0x13 (voice param) |

### Operator Editor Screen (per operator, 4 instances)

| Field | Type | Range | Display | Default | SysEx Param ID |
|-------|------|-------|---------|---------|-----------------|
| Output Level | QSlider | 0..127 | 0..127 dB | 0 | 0x00 (op param) |
| Output Level Curve | Dropdown | -Lin / +Lin / -Exp / +Exp | label | -Lin | 0x01 (op param) |
| Level Velocity Sensitivity | QButton spinbox | 0..31 | 0..31 | 0 | 0x02 (op param) |
| Level Depth (pitch dependent) | QButton spinbox | 0..15 | 0..15 | 0 | 0x03 (op param) |
| Adjust Total Level | QButton spinbox | 0..15 | 0..15 | 0 | 0x04 (op param) |
| Fine Detune | QButton spinbox | -4..3 | -4..3 | 0 | 0x05 (op param) |
| Multiple | QButton spinbox | 0..15 | 0..15 (×ratio) | 0 | 0x06 (op param) |
| Rate Depth (pitch dependent) | QButton spinbox | 0..3 | 0..3 | 0 | 0x07 (op param) |
| Attack Rate | QButton spinbox | 0..31 | 0..31 | 0 | 0x08 (op param) |
| Carrier / Modulator | Button toggle | Car / Mod | Carrier/Modulator | false | 0x09 (op param) |
| Attack Rate Velocity Sensitivity | QButton spinbox | 0..3 | 0..3 | 0 | 0x0A (op param) |
| Decay1 Rate | QButton spinbox | 0..31 | 0..31 | 0 | 0x0B (op param) |
| Coarse Detune | QButton spinbox | 0..3 | 0..3 (octave offset) | 0 | 0x0C (op param) |
| Decay2 Rate | QButton spinbox | 0..31 | 0..31 | 0 | 0x0D (op param) |
| Sustain Level | QButton spinbox | 0..15 | 0..15 | 0 | 0x0E (op param) |
| Release Rate | QButton spinbox | 0..15 | 0..15 | 0 | 0x0F (op param) |

---

## 3. SysEx Packet Specifications

### SysEx Header Template (All FB-01 SysEx messages)

\\\
F0 43 75 <ch> <fc> <ad> <ad> [data...] F7
\\\

Where:
- F0 = SysEx start
- 43 75 = Yamaha FB-01 manufacturer ID
- <ch> = MIDI system channel (0=ch1, 1=ch2, ... 15=ch16)
- <fc> = Function code (data type):
  - 0x00 = Voice data (single voice)
  - 0x01 = Instrument/Set data
  - 0x08 = Voice parameter request
  - 0x10 = System (config) data
  - 0x20 = Bulk dump request
- <ad> = Address bytes (interpretation varies by function code)
- [data...] = Parameter bytes (7-bit encoded)
- F7 = SysEx end

### Voice Dump Request

Trigger: User clicks "Get current voice" or "Load voice from FB-01"

Bytes:
\\\
F0 43 75 <ch> 0x08 <bk> <vc> F7
\\\

Where:
- <ch> = system channel (from config)
- <bk> = bank number (1-7 for user; 0 for internal)
- <vc> = voice number (1-48)

### Voice Data Dump Send

Trigger: User clicks "Send current voice"

Bytes (full voice):
\\\
F0 43 75 <ch> 0x00 <bk> <vc> [voice_bytes...] <checksum> F7
\\\

Voice bytes layout (91 bytes + checksum):
- Offset 0x09: 20 voice global parameters
- Offset 0x29: 4 operators × 16 params each = 64 bytes
- Offset 0x89: checksum (one's complement)

### Bank Dump Request

Trigger: User clicks "Get current bank"

Bytes:
\\\
F0 43 75 <ch> 0x20 0x00 0x00 F7
\\\

Response: FB-01 streams 48 sequential voice dumps.

### Checksum Calculation

\\\
checksum = (-sum_of_all_param_bytes) & 0x7F
\\\

---

## 4. Bank / File Format

### .SYX File Format

FB-01 .syx files are raw concatenations of SysEx bulk dumps with no header wrapper.

### Bank File Structure

Two forms exist:

**Standalone voice dump** (one voice → one .syx file): `VOICE_LEN_SYSEX = 0x8B = 139 bytes`. Layout:
- 8 bytes header (F0 43 75 ch 0x00 bk vc + ...)
- 0x80 (128) bytes parameter block
- 1 byte checksum
- 1 byte F7 footer

**Bank dump** (full 48-voice bank → one .syx file): `BANK_LEN_SYSEX = 0x18DB = 6363 bytes`. The bank wraps the 48 voices in a single SysEx envelope with its own header and checksum; each voice inside the bank uses the in-bank format `BANKVOICE_LEN_SYSEX = 0x83 = 131 bytes` (smaller header, no per-voice F0/F7 framing — they share the bank's envelope). Bank-level params live at `BANK_OFF_PARAM = 0x09`, length `BANK_LEN_PARAM = 0x40` bytes.

Reference constants from `core/bank.h` and `core/voice.h`. Don't ship "48 × 139" math — the in-bank format is different from the standalone export.

---

## 5. Cross-Cutting Concerns

### Real-Time vs. Deferred Transmission

- **Real-time (immediate):** Single-parameter SysEx on field change (operator params, voice params, instrument params)
- **Deferred (explicit send):** Full voice/bank/set bulk dumps only when user clicks "Send current voice", etc.
- **MIDI routing:** IN→OUT and CTRL→OUT relays forward notes in real-time.

### Connection States & UI Behavior

- **No MIDI device:** All "send" and "get" menu items disabled. Voice/bank/set editor panels require MIDI selection.
- **Device selected, FB-01 not responding:** "Get" operations timeout (10 seconds); user sees error dialog.
- **Connection regained:** Refresh MIDI devices list via Configuration tab button.

### Undo / Edit Buffer

- **No explicit undo/redo.** Edits modify in-memory model in real-time.
- **Discard edits:** Use File > Initialize or reload from disk.
- **Swap/Exchange:** Copy/paste/exchange between voice banks or swap instrument configurations.

### File Load Restrictions

- **.syx** files are raw SysEx. **.fb01** files contain full set/config metadata.
- **Voice vs. Set vs. Bank:** Determined by file size and context.

### Channel Mapping

- **System channel:** Configured in Config tab; used in all SysEx address bytes. Default = 1.
- **Instrument MIDI channels:** Each of 8 instruments has its own channel (1-16). Used for note-on/off only.
- **Virtual keyboard:** Separate channel (1-16, configurable).

---

## 6. Implementation Notes for LCD Target (480x272)

For the N64 controller rebuild, consider:

1. **Single fullscreen view** of one editor at a time (voice → operators → instruments → banks), navigable via D-Pad
2. **Small font** for compact labels and parameter names
3. **Joypad button mapping:**
   - D-Pad Left/Right: Move between screens/instruments
   - D-Pad Up/Down: Scroll parameter lists or adjust values
   - A/B buttons: Confirm/Cancel
   - C-buttons: Quick access to Send/Get/Initialize
   - Z: Shift/Alt mode for range selection

4. **Eliminate sliders** in favor of numeric spinbox input or +/- buttons
5. **Merge banks/instruments/voices** into a single hierarchical menu (Bank > Voice > Instrument > Parameters)
6. **Confirm dialogs** for destructive operations (send, load, exchange)
7. **Real-time feedback:** Highlight changed parameters before committing SysEx

This spec provides all the data model, layout, and MIDI protocol information needed to port the editor to constrained hardware.
