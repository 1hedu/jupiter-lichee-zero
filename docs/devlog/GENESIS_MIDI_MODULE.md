# Genesis MIDI Module — Plan

A bare-metal MIDI FM synthesizer using the Lichee Pi Zero + YM3438 chips.
The anti-mt32-pi: instead of Roland LA synthesis, it's Yamaha FM. Plug in
a MIDI cable, hear Genesis.

---

## Architecture

```
MIDI IN ──► 6N138 optocoupler ──► UART1 RX (PE22, pin 4)
                                       │
                                  Lichee Pi Zero
                                  ┌────────────┐
                                  │ MIDI parser │
                                  │ Voice alloc │
                                  │ Patch bank  │
                                  │ Note→F-Num  │
                                  └─────┬──────┘
                                        │ register writes
                          ┌─────────────┼─────────────┐
                     YM3438 #0     YM3438 #1     YM3438 #2
                     (ch 1-6)      (ch 7-12)     (ch 13-18)
                       │  │          │  │           │  │
                      MOL MOR      MOL MOR        MOL MOR
                       └──┼──────────┼──┼───────────┘  │
                          └──────────┼──┘              │
                           Resistor mixer ◄────────────┘
                                │
                              Amp ──► Speakers
```

---

## Hardware

### Three YM3438 Chips — Shared Bus

All chips share data bus and control lines. Only /CS is separate.

| Signal | GPIO | Shared? |
|---|---|---|
| D0-D7 | PB0-PB7 | Shared across all 3 chips |
| A0 | PG0 (J3 pin 1) | Shared |
| A1 | PG1 (J3 pin 2) | Shared |
| /WR | PG2 (J3 pin 3) | Shared |
| /IC | PG4 (J3 pin 6) | Shared |
| φM clock | PE1 (CCU MCLK 8MHz) | Shared |
| /CS chip 0 | PG3 (J3 pin 5) | Individual |
| /CS chip 1 | PG5 (J3 pin 4) | Individual |
| /CS chip 2 | PF0 (SDC0, post-boot) | Individual |
| /RD (all) | Tied to +5V | Shared |

Each chip: +5V power, 100nF decoupling on Vcc and AVcc, AGND to ground.
All three φM inputs from the same PE1 MCLK output (8.0 MHz, CCU 24/3).

### Audio Mixing

Six analog outputs (3 chips × stereo) mixed through resistors:

```
YM3438 #0 MOL ──22kΩ──┐
YM3438 #1 MOL ──22kΩ──┼── Amp L in
YM3438 #2 MOL ──22kΩ──┘

YM3438 #0 MOR ──22kΩ──┐
YM3438 #1 MOR ──22kΩ──┼── Amp R in
YM3438 #2 MOR ──22kΩ──┘
```

Equal resistors = equal mix. DC blocking cap (100nF) before each amp input.
LPF per chip output: 10kΩ + 680pF (fc ≈ 23kHz) before the mixing resistors.

### MIDI Input Circuit

Standard opto-isolated MIDI input on UART1 (PE22 = RX, 31250 baud):

```
MIDI DIN pin 5 ──── 220Ω ──┬── 6N138 pin 2 (anode)
MIDI DIN pin 4 ──────────── 6N138 pin 3 (cathode)
                            │
              +3.3V ──4.7kΩ─┤── 6N138 pin 6 (output) ──► PE22 (UART1_RX)
                            │
                    GND ──── 6N138 pin 5

6N138 pin 8 = +3.3V, pin 7 = 10kΩ to +3.3V (pull-up)
```

MIDI thru: PE21 (UART1_TX) can echo received bytes for daisy-chaining.
Total cost: 6N138 (~$0.50) + 1N4148 diode + 3 resistors.

### Backlight Fix

PB4/PB5 (YM3438 D4/D5) conflict with LCD backlight boost converter.
Wire PT4101 EN pin directly to 3.3V for always-on backlight.

---

## Software Components

### 1. MIDI Parser (~200 lines)

Parse MIDI byte stream at 31250 baud from UART1.

```c
typedef struct {
    uint8_t status;         /* running status */
    uint8_t channel;        /* 0-15 */
    uint8_t data[2];        /* up to 2 data bytes */
    int     bytes_needed;   /* remaining data bytes for current message */
} midi_parser_t;

void midi_init(void);
void midi_process_byte(uint8_t byte);

/* Callbacks: */
void midi_note_on(uint8_t ch, uint8_t note, uint8_t vel);
void midi_note_off(uint8_t ch, uint8_t note);
void midi_program_change(uint8_t ch, uint8_t program);
void midi_pitch_bend(uint8_t ch, int16_t bend);
void midi_cc(uint8_t ch, uint8_t cc, uint8_t val);
void midi_all_notes_off(uint8_t ch);
```

Handle: note on/off, program change, pitch bend, CC (volume, pan,
sustain pedal, all notes off, all sound off). Ignore sysex initially.

### 2. Voice Allocator (~150 lines)

Map MIDI channels (0-15) to YM3438 FM channels (0-17 across 3 chips).

```c
#define NUM_CHIPS       3
#define VOICES_PER_CHIP 6
#define TOTAL_VOICES    (NUM_CHIPS * VOICES_PER_CHIP)  /* 18 */

typedef struct {
    uint8_t  midi_ch;     /* MIDI channel this voice is playing, or 0xFF */
    uint8_t  note;        /* MIDI note number */
    uint8_t  velocity;    /* note velocity */
    uint32_t timestamp;   /* when this voice was allocated (for stealing) */
    uint8_t  active;      /* key is currently on */
} voice_t;

voice_t voices[TOTAL_VOICES];
```

Allocation strategy:
- On note-on: find a free voice, assign it. If no free voice, steal the
  oldest voice (lowest timestamp) from the same MIDI channel first, then
  from any channel.
- On note-off: find the voice playing this channel+note, key off.
- On program change: update the patch for that MIDI channel (doesn't
  affect currently sounding notes until next note-on).

Voice-to-chip mapping:
- Voice 0-5 → chip 0, FM channels 0-5
- Voice 6-11 → chip 1, FM channels 0-5
- Voice 12-17 → chip 2, FM channels 0-5

```c
int voice_chip(int voice) { return voice / VOICES_PER_CHIP; }
int voice_ch(int voice)   { return voice % VOICES_PER_CHIP; }
```

### 3. FM Patch Bank (~500 lines of data)

Each patch defines all 4 operators + algorithm + feedback:

```c
typedef struct {
    uint8_t algorithm;     /* 0-7 */
    uint8_t feedback;      /* 0-7 */
    struct {
        uint8_t mul;       /* 0-15: frequency multiple */
        uint8_t tl;        /* 0-127: total level (volume) */
        uint8_t ar;        /* 0-31: attack rate */
        uint8_t dr;        /* 0-31: decay rate */
        uint8_t sr;        /* 0-31: sustain rate */
        uint8_t rr;        /* 0-15: release rate */
        uint8_t sl;        /* 0-15: sustain level */
        uint8_t ks;        /* 0-3: key scale */
        uint8_t dt;        /* 0-7: detune */
        uint8_t ams;       /* 0-1: AM sensitivity */
    } op[4];
} fm_patch_t;
```

Sources for patches:
- **GEMS** instrument format (extracted from Genesis game ROMs)
- **TFI** format (DefleMask tracker exports)
- **OPN2 Bank Editor** patch collections
- **VGM rips** — extract patch data from register writes in VGM files
- Hand-tuned General MIDI mappings (community projects like GenMDM)

Initial bank: 128 patches mapping to General MIDI program numbers.
This is the biggest effort — curating/converting 128 FM patches that
sound reasonable for each GM instrument category.

### 4. Note-to-Frequency Table (~128 entries)

Precomputed MIDI note → YM3438 F-Number + Block:

```c
typedef struct {
    uint16_t fnum;    /* 11-bit F-Number */
    uint8_t  block;   /* 3-bit block (octave) */
} note_freq_t;

/* For 8.0 MHz clock:
 * freq = (fnum × 8000000) / (144 × 2^(21-block))
 *
 * MIDI note 69 = A4 = 440 Hz:
 * fnum = 440 × 144 × 2^(21-block) / 8000000
 * At block 4: fnum = 440 × 144 × 131072 / 8000000 = 1038
 */
const note_freq_t note_table[128];
```

Precompute all 128 notes at init, or use a lookup table.
Pitch bend: interpolate between adjacent note entries.

### 5. Multi-Chip Driver Extension (~100 lines)

Extend ym3438_hw_write to address specific chips:

```c
/* Select which chip to talk to */
void ym3438_hw_select(int chip);  /* assert /CS for this chip */

/* Write to a specific chip */
void ym3438_hw_chip_write(int chip, uint8_t port, uint8_t addr, uint8_t data);

/* Key on/off for a specific chip + channel */
void ym3438_hw_key_on(int chip, int ch);
void ym3438_hw_key_off(int chip, int ch);

/* Load a patch into a chip's channel */
void ym3438_hw_load_patch(int chip, int ch, const fm_patch_t *patch);

/* Set frequency for a chip's channel */
void ym3438_hw_set_freq(int chip, int ch, uint16_t fnum, uint8_t block);

/* Set total level (velocity) for a chip's channel carrier operators */
void ym3438_hw_set_velocity(int chip, int ch, uint8_t velocity,
                             const fm_patch_t *patch);
```

### 6. UART1 Driver (~50 lines)

31250 baud UART on PE21/PE22:

```c
void uart1_init(void);        /* 31250 baud, 8N1 */
int  uart1_readable(void);    /* bytes available? */
uint8_t uart1_getc(void);     /* read one byte */
void uart1_putc(uint8_t c);   /* MIDI thru */
```

V3s UART1 base: 0x01C25400. Same register layout as UART0.
PE21 = function 4 (UART1_TX), PE22 = function 4 (UART1_RX).
Clock: APB2 bus clock, gate at BUS_CLK_GATING3 bit 17.

---

## Main Loop

```c
while (1) {
    /* Process all available MIDI bytes */
    while (uart1_readable()) {
        midi_process_byte(uart1_getc());
        uart1_putc(byte);  /* MIDI thru */
    }

    /* N64 controller for live tweaks (optional) */
    input_state_t pad = input_poll();
    if (input_pressed() & BTN_START) { /* panic: all notes off */ }
    if (input_pressed() & BTN_A)     { /* cycle through patch banks */ }

    /* Status on UART0 every second */
    if (timer_elapsed(...) >= 1000) {
        print active voices, current patches, etc.
    }
}
```

No video rendering. No vblank sync. The main loop just processes MIDI
bytes as fast as they arrive and writes YM3438 registers. Latency is
essentially the UART byte time (~320μs per byte at 31250 baud) plus
the register write time (~12μs).

---

## Pin Overlay

New overlay for MIDI module:

```
YM3438 bus:
  PB0-PB7    D0-D7 (shared)
  PG0        A0 (shared)
  PG1        A1 (shared)
  PG2        /WR (shared)
  PG4        /IC (shared)
  PE1        φM clock (8MHz CCU MCLK, shared)

Chip select:
  PG3        /CS chip 0
  PG5        /CS chip 1
  PF0        /CS chip 2

MIDI:
  PE22       UART1 RX (MIDI IN)
  PE21       UART1 TX (MIDI THRU)

Controller (optional):
  PE20       N64 DATA (panic button, patch select)

Audio:
  Chip 0 MOL/MOR ──LPF──22kΩ──┐
  Chip 1 MOL/MOR ──LPF──22kΩ──┼── Amp
  Chip 2 MOL/MOR ──LPF──22kΩ──┘

Backlight: PT4101 EN wired to 3.3V (always on)
/RD all chips: tied to +5V
```

---

## Stretch Goals

- **MIDI channel 10 = drums**: use DAC mode on one chip's channel 6
  for 8-bit PCM drum samples stored in DRAM
- **LFO control**: map MIDI CC1 (mod wheel) to YM3438 LFO depth
- **Stereo panning**: map MIDI CC10 (pan) to YM3438 L/R output select
  register ($B4-$B6 bits 6-7)
- **Pitch bend**: real-time F-Number interpolation on active voices
- **SysEx patch upload**: receive custom FM patches over MIDI SysEx
- **LCD display**: show active channels, patch names, MIDI activity
  (need to solve PB4/PB5 backlight conflict first)
- **Velocity curves**: map MIDI velocity to TL with configurable curves
- **Sustain pedal**: CC64 holds notes past key-off
- **Second N64 controller on PG5**: ... but PG5 is /CS chip 1. Would
  need to move /CS1 to PF1 to free PG5 for controller.
- **PSG channels**: add SN76489 (or software PSG through V3s codec)
  for percussion/noise/square wave patches
- **USB MIDI**: V3s USB OTG as MIDI device — no optocoupler needed

---

## Bill of Materials

| Part | Qty | Cost | Notes |
|---|---|---|---|
| YM3438 (DIP-24) | 3 | $30 | ~$10 each |
| 6N138 optocoupler | 1 | $0.50 | MIDI input isolation |
| 1N4148 diode | 1 | $0.05 | MIDI input protection |
| 100nF ceramic caps | 6 | $0.30 | Decoupling (2 per chip) |
| 10kΩ resistors | 6 | $0.10 | LPF (2 per chip) |
| 680pF ceramic caps | 6 | $0.10 | LPF (2 per chip) |
| 22kΩ resistors | 6 | $0.10 | Audio mixer |
| 100nF caps | 2 | $0.05 | DC blocking (stereo) |
| 220Ω resistor | 1 | $0.02 | MIDI input |
| 4.7kΩ resistor | 1 | $0.02 | 6N138 pull-up |
| 5-pin DIN jack | 1 | $1.00 | MIDI connector |
| Stereo amp module | 1 | $2-5 | PAM8403 or similar |
| **Total** | | **~$35** | Plus Lichee Pi Zero ($6) |

---

## Name Ideas

- **GenMIDI** (taken by the Arduino project)
- **MegaSynth**
- **Jupiter FM**
- **OPN2-pi** (parallel to mt32-pi)
- **Mars FM** (continuing the planetary theme)
