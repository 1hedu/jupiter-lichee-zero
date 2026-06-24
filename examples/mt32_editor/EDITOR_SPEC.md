# Roland MT-32 Sound Editor Specification

Second of the three planned hardware-instrument editors (FB-01 → MT-32
→ Behringer WaveTerm). UI parity with the FB-01 editor: 480×272 LCD
+ N64 controller, tabbed fullscreen layout, real SysEx wire-format
constructed on-device.

## 1. Hardware reference

| Aspect | Value |
|---|---|
| Synth | Roland LA (Linear Arithmetic), 1987 |
| Polyphony | 32 voices |
| Parts | 8 melodic + 1 rhythm = 9 |
| Patches (writable) | 64 |
| Timbres (writable RAM) | 64 |
| Timbres (ROM) | 128 |
| Rhythm sounds | 85 keys (35-99 → drums) |
| Reverb | 4 modes × 8 times × 8 levels |
| MIDI | DIN-5 IN + OUT (no THRU) |

## 2. Screen inventory (6 tabs, mirrors FB-01 editor layout)

1. **CFG** — MIDI port selection (when Phase 2 hardware lands), master
   tune (432–457 Hz), master volume (0–100), master pan, reverb mode
   (Room/Hall/Plate/TapDelay), reverb time, reverb level, partial
   reserve slots (per-part voice budget).

2. **PARTS** — 8 melodic parts. Each: MIDI channel, assigned patch
   (1–128), volume (0–100), key shift (-24..+24), fine tune (-50..+50),
   bender range (0–24), assign mode (poly1/2/3/4), reverb on/off.

3. **PATCHES** — 64 writable patches. Each: timbre group + number
   (group A/B/M/R, 1–64), key shift, fine tune, bender range, assign
   mode, reverb switch, output level. (Patch is the "what timbre +
   how it plays" abstraction the part references.)

4. **TIMBRES** — 64 RAM timbre slots. Each: 10-char name, structure
   (12 different combinations of 4 partials — pairs of paired/single
   partials, ring-modulated or not), partial mute mask (which of 4 are
   active), sustain mode.

5. **PARTIAL** — partial editor for the current timbre (1 of 4
   partials). PCM wave or synth wave selection, pitch envelope,
   filter envelope, amp envelope, LFO, biases. This is the deep one;
   ~60 parameters per partial.

6. **RHYTHM** — rhythm part. 85 drum-key slots (keys 35–99), each
   assigned: timbre group + number, output level, panpot, reverb
   switch.

## 3. SysEx packet format (Roland MT-32)

### Header template (all DT1 / RQ1 messages)

```
F0 41 10 16 <cmd> <addr_hi> <addr_mid> <addr_low> [data...] <checksum> F7
```

| Byte | Meaning |
|---|---|
| F0 | SysEx start |
| 41 | Roland manufacturer ID |
| 10 | Device ID (unit 0x10 by default) |
| 16 | Model ID (MT-32) |
| cmd | 0x12 = DT1 (data set), 0x11 = RQ1 (request data) |
| addr_hi/mid/low | 3-byte memory address (each ≤ 0x7F) |
| data | Param bytes (each ≤ 0x7F) |
| checksum | `(- (sum_of_address + sum_of_data)) & 0x7F` |
| F7 | SysEx end |

### Address map (high-level)

| Address | Region | Size |
|---|---|---|
| 03 00 00 | Patch temp (the part-active sound, immediate change) | 10 bytes × 9 parts |
| 04 00 00 | Timbre temp area | 246 bytes × 8 parts |
| 05 00 00 | Patch memory (64 patches × 8 bytes) | 512 bytes |
| 08 00 00 | Timbre memory bank A (32 timbres) | 8 KB |
| 09 00 00 | Timbre memory bank B (32 timbres) | 8 KB |
| 10 00 00 | System area (master vol/tune/pan/reverb) | 23 bytes |
| 7F 00 00 | Reset → ROM defaults | 0 bytes |

### Examples

**Master volume to 80**: write 1 byte to system-area offset 16.
```
F0 41 10 16 12 10 00 16 50 6A F7
                              ^^ checksum = -(0x10+0x00+0x16+0x50) & 0x7F = 0x6A
```

**Request full patch dump** (64 patches × 8 bytes = 512 bytes):
```
F0 41 10 16 11 05 00 00 00 04 00 77 F7
```

### Checksum calculation

```c
uint8_t cs = 0;
for (i = 0; i < n_addr_bytes; i++)  cs += addr[i];
for (i = 0; i < n_data_bytes; i++)  cs += data[i];
cs = (0x100 - (cs & 0x7F)) & 0x7F;
```

## 4. File formats

### .syx (raw SysEx)
Same model the FB-01 editor uses — concatenation of DT1 messages with
their F0/F7 framing. Stream-replay loads the dump to MT-32.

### .mid (banks-as-MIDI-files)
Many MT-32 sysex dumps are distributed as standard MIDI files
containing one SysEx event per timbre/patch with appropriate timing
delays so MT-32 has time to process each. Loader needs to ignore non-
SysEx events.

## 5. Cross-cutting

- **Real-time vs deferred**: same as FB-01 — single-param SysEx (DT1
  to temp areas 03/04) on field change; bulk dumps (to patch/timbre
  memory) only on explicit "Send".
- **Memory protect**: MT-32 honors a system-area "memory protect" bit;
  must be cleared before writes to 04/05/08/09 take effect.
- **ROM vs RAM**: timbre groups A and B are ROM (read-only) in stock
  MT-32; groups M (memory) and R (rhythm) are writable. The editor
  presents all 128 timbres in groups but disables Send for ROM.
- **Reset on connect**: optional "send display message" command can
  push a custom 20-char string to the MT-32's LCD as a visual ack.

## 6. Implementation notes for LCD target (480×272)

- Reuse the FB-01 editor's tab-bar / field-renderer / cursor scheme.
- Same N64 button conventions:
  - L/R = cycle tabs (locked while in sub-edit)
  - D-pad = nav cursor / adjust value
  - A = activate / enter sub-edit
  - B = back / exit sub-edit
  - Z = send current context (dump SysEx)
  - C-up = request from device
  - C-down/C-left/C-right = quick actions per tab
- Same Phase 1 / Phase 2 split: SysEx wire-format built locally, logged
  to UART0 hex now, swapped to real MIDI UART when hardware arrives.

## 7. Differences vs FB-01

- **More params per voice**: 4 partials × ~60 params = ~240 + voice-level
  = ~260 params per timbre (vs FB-01's ~85). Layout-wise: dedicated
  PARTIAL tab keeps it tractable; TIMBRES tab shows just name + structure.
- **Wave selection**: each partial picks between synth wave (PCM index)
  or a PCM sample (256 samples ROM). UI needs a sample-browser mode.
- **No FB-01 system channel**: MT-32 routes per-part via the PARTS
  table, not via a single system channel.
- **Different SysEx mfr ID**: F0 41 (Roland) vs F0 43 (Yamaha).
