# Jupiter SDK — Handoff Document

## What Exists (March 2026)

The visual pipeline is complete and proven at 60fps on bare metal:

- **7 library modules:** uart, timer, mem, video, tiles, mmu, sprite
- **9 examples**, each proving one capability, each leaving SDK behind it
- **Template** defaults to best config: MMU, D-cache, both layers double-buffered, 60fps

The flagship demo (`mode7_sprites`) runs a Mode 7 affine vortex floor with
sky gradient on VI0, plus 25 alpha-keyed sprites on a tear-free
double-buffered UI0 overlay, all hardware-composited at locked 60fps with
3.7ms headroom. Total frame: 13.0ms.

**What's missing to make this a game platform:** input and audio.

---

## Part 1: Input (GPIO Buttons)

### Overview

This is the simpler of the two tasks. The Lichee Pi Zero exposes GPIO pins
on the 2×15 2.54mm headers. Reading a button is: configure pin as input
with pull-up, read the port data register, debounce in software. No
interrupts needed — poll once per frame at 60fps.

### Hardware

The V3s GPIO controller lives at `PIO_BASE` (0x01C20800). Each port
(A-G) has a block of registers at offset `port * 0x24`:

```
Offset  Register     Purpose
0x00    CFG0         Pin config [0:7] — 4 bits per pin
0x04    CFG1         Pin config [8:15]
0x08    CFG2         Pin config [16:23]
0x0C    CFG3         Pin config [24:31]
0x10    DATA         Read/write pin state (1 bit per pin)
0x14    DRV0         Drive strength [0:15]
0x18    DRV1         Drive strength [16:31]
0x1C    PULL0        Pull-up/down [0:15] — 2 bits per pin
0x20    PULL1        Pull-up/down [16:31]
```

Pin config values: `0` = input, `1` = output, function codes 2-7 vary.
Pull values: `00` = disabled, `01` = pull-up, `10` = pull-down.

### Recommended Pin Assignment

Port B and Port G pins are available on the 2.54mm headers. Suggested
mapping for a 6-button gamepad (d-pad + A + B):

```
Button    Pin    Port/Bit    Header Position
------    ---    --------    ---------------
UP        PG0    G.0         Check board silkscreen
DOWN      PG1    G.1
LEFT      PG2    G.2
RIGHT     PG3    G.3
A         PG4    G.4
B         PG5    G.5
```

**IMPORTANT:** Verify against actual Lichee Pi Zero pinout. Port G pins
PG0-PG5 are also used for MMC1 (second SD slot). If the dock board is
attached, these may conflict. Port B pins (PB0-PB3) are alternatives
but PB8/PB9 are UART0 TX/RX — don't use those.

### Wiring

Each button connects its GPIO pin to GND through a normally-open switch.
The pin is configured with internal pull-up, so:
- Button released → pin reads 1 (pulled high)
- Button pressed → pin reads 0 (shorted to ground)

No external resistors needed — the V3s has internal pull-ups.

### Software Design

#### Register Definitions (add to v3s.h)

```c
/* Port G: offset 6 * 0x24 = 0xD8 from PIO_BASE */
#define PG_CFG0     REG32(PIO_BASE + 0xD8)
#define PG_DATA     REG32(PIO_BASE + 0xE8)
#define PG_PULL0    REG32(PIO_BASE + 0xF4)
```

#### API Design (lib/input.c + jupiter.h)

```c
/* Button bit masks */
#define BTN_UP      BIT(0)
#define BTN_DOWN    BIT(1)
#define BTN_LEFT    BIT(2)
#define BTN_RIGHT   BIT(3)
#define BTN_A       BIT(4)
#define BTN_B       BIT(5)

/* Initialize GPIO pins as input with pull-up */
void input_init(void);

/* Read current button state. Returns bitmask.
 * Bit = 1 means pressed (active-low inverted for sanity).
 * Call once per frame. Includes 2-frame debounce. */
uint32_t input_read(void);

/* Convenience: edge detection */
uint32_t input_pressed(void);   /* buttons pressed THIS frame (not last) */
uint32_t input_released(void);  /* buttons released THIS frame */
```

#### Implementation Notes

```c
static uint32_t prev_state = 0;
static uint32_t curr_state = 0;
static uint32_t raw_prev = 0;

void input_init(void)
{
    /* Configure PG0-PG5 as input (0x000000 in CFG0 bits [23:0]) */
    PG_CFG0 = (PG_CFG0 & 0xFF000000);  /* clear bits [23:0] = input */

    /* Enable pull-ups on PG0-PG5 (01 per pin in PULL0) */
    PG_PULL0 = (PG_PULL0 & ~0x00000FFF) | 0x00000555;
}

uint32_t input_read(void)
{
    /* Read port, invert (active-low → 1=pressed), mask to 6 buttons */
    uint32_t raw = (~PG_DATA) & 0x3F;

    /* 2-frame debounce: only accept if raw matches previous raw */
    prev_state = curr_state;
    if (raw == raw_prev)
        curr_state = raw;
    raw_prev = raw;

    return curr_state;
}

uint32_t input_pressed(void)  { return curr_state & ~prev_state; }
uint32_t input_released(void) { return ~curr_state & prev_state; }
```

#### Template Integration

```c
static void game_frame(uint32_t bg_addr, volatile uint32_t *ovl, uint32_t frame)
{
    uint32_t btn = input_read();

    if (btn & BTN_LEFT)  player_x -= 2;
    if (btn & BTN_RIGHT) player_x += 2;
    if (input_pressed() & BTN_A) player_jump();

    /* ... render ... */
}
```

### Estimated Effort

This is a few hours of work: register setup, read function, debounce,
one test example with UART output showing button state. The only risk
is pin assignment — need to verify which Port G pins are actually routed
to the header on the specific Lichee Pi Zero board revision in hand.

### Test Example Plan

`examples/input_test/main.c` — reads buttons, prints state to UART,
and moves the hero sprite from the sprites demo in response to d-pad.
First playable interaction on the platform.

---

## Part 2: Audio System

### Philosophy

The VHC manifesto puts a dedicated ADSP-21565 as "the ear" — a
self-contained audio computer. The VHC0 has no dedicated audio
processor; the V3s's integrated codec is a simple DAC fed by DMA.

The audio system must be designed so the game speaks to an **abstraction
layer** — "play sound X on channel N at volume V" — not to the hardware
directly. When the real VHC hardware ships, only the mixer backend
changes (from "CPU software mix" to "send command to ADSP"). Game code
doesn't change. This is the forward-thinking the manifesto demands.

### Hardware: V3s Audio Codec

The V3s has an integrated audio codec with:
- **DAC** with stereo output (we use mono summed to SPKP/SPKN)
- **Class-D speaker amplifier** on SPKP/SPKN differential pins
  (drives 8Ω 1-2W speaker directly, no external amp needed)
- **DMA engine** (sun6i-compatible) for autonomous sample transfer
- **Audio PLL** for clock generation

Speaker pins are on the 1.27mm half-holes of the Lichee Pi Zero.
Wire SPKP and SPKN directly to speaker terminals.

Key register blocks:
- **Audio codec:** base address from V3s datasheet (needs verification,
  likely 0x01C22C00 based on sun4i-codec driver)
- **DMA:** 0x01C02000 (sun6i DMA engine)
- **CCU:** audio PLL at CCU_BASE + 0x0008

Linux kernel references:
- `sound/soc/sunxi/sun4i-codec.c` — codec register map
- `drivers/dma/sun6i-dma.c` — DMA engine
- Device tree: `sun8i-v3s.dtsi` codec and DMA nodes

### Audio Modes

Three modes, selectable at init time. All modes output through the same
DMA-fed ring buffer at 22050 Hz, 16-bit signed mono.

#### Mode 1: PCM Playback (4 channels)

The simplest mode. 1 music channel + 3 SFX channels.

```
Channel 0: Music  — long PCM stream, loops
Channel 1: SFX A  — one-shot or looping
Channel 2: SFX B  — one-shot or looping
Channel 3: SFX C  — one-shot or looping
```

Sound data is `const int16_t[]` arrays compiled into the binary.
The mixer sums all active channels with per-channel volume (0-255)
into the output ring buffer.

**Good for:** games with pre-rendered music (MOD/XM converted to PCM),
simple sound effects. Minimal code. Easiest to get working first.

**Memory cost:** 22050 samples/sec × 2 bytes = ~43 KB per second of
audio. A 30-second music loop = ~1.3 MB. Fits in 64MB RAM easily.

#### Mode 2: Game Boy APU Emulation (4 channels)

Models the classic Game Boy sound hardware:

```
Channel 0: Pulse A   — square wave, variable duty (12.5/25/50/75%)
Channel 1: Pulse B   — square wave, variable duty
Channel 2: Wave      — 32-sample programmable waveform
Channel 3: Noise     — LFSR noise generator
```

Each channel has: frequency, volume, envelope (attack/decay), length
timer. All synthesis is computed in software — the CPU generates
samples on the fly into the ring buffer.

**Good for:** chiptune music, retro SFX. Tiny memory footprint (music
is note data, not PCM). The aesthetic is specific and beloved.

**Implementation:** each channel is a struct with phase accumulator,
envelope state, and output function. The mixer ticks all 4 channels
per sample and sums to the output buffer. At 22050 Hz, that's 22050
channel-ticks per second per channel = ~88K ticks/sec total. Trivial
for a 1.2 GHz CPU.

**Reference:** The Game Boy APU is extremely well-documented. Pandocs
(gbdev.io) has the complete register map and behavior specification.

#### Mode 3: SNES SPC700 Style (8 channels)

Models the SNES audio approach: 8 channels of sample-based playback
with pitch control, ADSR envelopes, and optional noise.

```
Channels 0-7: BRR-style sample playback
              - Source: compressed or raw PCM sample
              - Pitch: 16-bit, allows any playback rate
              - Volume: left/right (we sum to mono)
              - ADSR envelope: attack, decay, sustain, release
              - Noise: channel can switch to noise mode
```

Music is driven by a simple tracker/sequencer that sends commands
to channels on a tick grid:

```
Tick  Ch  Command
0     0   NOTE_ON(C4, instrument=0, velocity=127)
0     1   NOTE_ON(E4, instrument=1, velocity=100)
4     2   NOTE_ON(kick_drum, velocity=127)
8     0   NOTE_OFF
8     0   NOTE_ON(D4, instrument=0, velocity=127)
...
```

**Good for:** the full SNES sound experience. Rich music with real
instrument samples. The iconic sound of Chrono Trigger, F-Zero,
Secret of Mana.

**Implementation:** more complex than Mode 2. Each channel needs a
phase accumulator with fractional stepping (pitch), an ADSR state
machine, and sample interpolation. The sequencer runs from a compact
pattern format (similar to MOD/XM tracker formats).

**Memory cost:** instrument samples are small (a piano note might be
512 samples looped = 1KB). A full instrument set for a game might
be 50-200 KB. Music patterns are tiny (a few KB per song).

### Mixer Architecture

All three modes feed into the same output path:

```
                    ┌─────────────┐
  Mode 1 (PCM)  ──→│             │
  Mode 2 (APU)  ──→│  Software   │──→ Ring Buffer ──→ DMA ──→ DAC ──→ Speaker
  Mode 3 (SPC)  ──→│  Mixer      │    (2 × 1024     (auto)   (codec)  (SPKP/N)
                    │             │     samples)
                    └─────────────┘
                    Called from main
                    loop or timer ISR
```

#### Ring Buffer

Two-half design (ping-pong):

```
Ring buffer: 2048 samples (2 × 1024 half-buffers)
DMA plays one half while CPU fills the other.
At 22050 Hz: each half = 1024/22050 = ~46ms of audio.
CPU must fill a half-buffer every 46ms = well within one frame at 60fps.
```

The DMA controller fires an interrupt (or sets a status flag we poll)
when it finishes one half. The game's main loop or a lightweight ISR
checks this flag and calls the mixer to generate the next 1024 samples.

#### Mixer API

```c
/* ---- audio.h / jupiter.h ---- */

/* Audio modes */
#define AUDIO_MODE_PCM   1    /* 4-channel PCM playback */
#define AUDIO_MODE_APU   2    /* Game Boy APU emulation */
#define AUDIO_MODE_SPC   3    /* SNES SPC700 style */

/* Initialize audio subsystem.
 * Configures codec, DMA, ring buffer. Starts silent. */
void audio_init(uint32_t mode);

/* Call every frame (or from ISR). Mixes pending samples
 * into the ring buffer. Non-blocking. */
void audio_update(void);

/* ---- Mode 1: PCM ---- */
void audio_pcm_play(uint32_t channel,
                    const int16_t *samples, uint32_t length,
                    uint8_t volume, uint8_t loop);
void audio_pcm_stop(uint32_t channel);

/* ---- Mode 2: APU ---- */
void audio_apu_note_on(uint32_t channel, uint32_t frequency,
                       uint8_t volume, uint8_t duty);
void audio_apu_note_off(uint32_t channel);
void audio_apu_noise(uint8_t volume, uint8_t rate);
void audio_apu_wave(const uint8_t wave[32]);  /* custom waveform ch2 */

/* ---- Mode 3: SPC ---- */
void audio_spc_note_on(uint32_t channel, uint32_t pitch,
                       uint32_t instrument, uint8_t velocity);
void audio_spc_note_off(uint32_t channel);
void audio_spc_load_instrument(uint32_t id,
                               const int16_t *samples, uint32_t length,
                               uint32_t loop_start);
void audio_spc_play_song(const void *song_data);  /* tracker format */
void audio_spc_stop_song(void);

/* ---- Shared ---- */
void audio_set_master_volume(uint8_t volume);  /* 0-255 */
```

### Implementation Priority

**Phase 1: Bring up the DAC.**
Just get sound coming out of the speaker. Configure audio PLL, codec,
DMA. Play a 22050 Hz sine wave from a const array. When you hear the
tone, the hardware plumbing is proven. This is the equivalent of the
`colorbars` test for video.

**Phase 2: Mode 1 (PCM mixer).**
4-channel software mixer. Play a compiled-in PCM music loop on channel
0, trigger SFX on channels 1-3. Prove mixing, volume, looping.
This is immediately useful for games.

**Phase 3: Mode 2 (APU).**
Implement the 4 synthesis channels. Play a simple chiptune melody.
This could be done as a standalone module — it's pure math, no
hardware dependency beyond what Phase 1 already proved.

**Phase 4: Mode 3 (SPC).**
Sample-based playback with pitch and ADSR. Implement the tracker
sequencer. This is the most complex but also the most rewarding —
it's the sound that makes people say "this sounds like a real console."

### Hardware Bring-Up Notes

The hardest part of audio is Phase 1 — configuring the codec. Here's
what needs to happen:

1. **Enable audio PLL** (CCU PLL_AUDIO register)
2. **Enable codec bus clock** (CCU bus gating)
3. **De-assert codec reset** (CCU bus reset)
4. **Configure codec registers:**
   - DAC digital control (enable, sample rate)
   - DAC analog control (enable DAC, enable speaker amp)
   - Mixer control (route DAC to speaker output)
   - Volume/gain settings
5. **Configure DMA channel:**
   - Source: ring buffer address in DRAM
   - Destination: codec DAC FIFO register
   - Transfer: half-word (16-bit), linear source, fixed destination
   - Trigger: codec DRQ (DMA request)
   - Mode: circular with half-transfer notification
6. **Start DMA**

The Linux `sun4i-codec.c` driver is the reference for register values.
The Allwinner V3s codec is register-compatible with sun8i variants.
Extract the exact register writes from the Linux driver's `.startup`
and `.hw_params` paths for 22050 Hz mono 16-bit.

The DMA engine (`sun6i-dma.c`) is well-documented in the kernel.
The key insight: DMA runs autonomously once started. The CPU's only
job is keeping the ring buffer filled.

### CPU Budget for Audio

At 22050 Hz with 1024-sample half-buffers, the mixer runs every ~46ms
(less than 3 frames at 60fps). Mixing cost per call:

| Mode | Work per sample | Total per 1024 samples | % of frame budget |
|------|----------------|----------------------|-------------------|
| PCM (4ch) | 4 adds + clip | ~10 µs | 0.06% |
| APU (4ch) | 4 oscillators + mix | ~50 µs | 0.3% |
| SPC (8ch) | 8 pitch + ADSR + mix | ~150 µs | 0.9% |

Audio will never be the bottleneck. Even Mode 3 at maximum complexity
is less than 1% of a frame.

### ADSP Offload Path (Future VHC)

The API is designed so the backend can change without game code changing:

```
VHC0 (Lichee Pi Zero):
  audio_spc_note_on() → writes to channel struct → CPU mixes

VHC (final hardware):
  audio_spc_note_on() → sends command packet to ADSP-21565 via SPI/I2C
                        → ADSP mixes independently
                        → ADSP feeds its own DAC
```

The game calls the same function. The implementation swaps at link time
or via a function pointer table. This is why the API uses high-level
commands (note on, note off, play song) rather than exposing the ring
buffer directly.

---

## Summary: Build Order

```
 NOW ──────────────────────────────────────────────────── PLAYABLE
  │                                                          │
  ├── Input (GPIO)                                           │
  │   ├── Register defines in v3s.h                          │
  │   ├── lib/input.c (init, read, debounce, edge detect)   │
  │   ├── examples/input_test/main.c                         │
  │   └── *** First playable interaction ***                 │
  │                                                          │
  ├── Audio Phase 1 (DAC bring-up)                           │
  │   ├── Audio PLL + codec + DMA configuration              │
  │   ├── Ring buffer + sine wave test tone                  │
  │   ├── examples/audio_test/main.c                         │
  │   └── *** First sound from speaker ***                   │
  │                                                          │
  ├── Audio Phase 2 (PCM mixer)                              │
  │   ├── lib/audio.c — Mode 1: 4-channel PCM               │
  │   ├── Software mixer (sum + clip + volume)               │
  │   ├── examples/audio_pcm/main.c                          │
  │   └── *** Music + SFX playing ***                        │
  │                                                          │
  ├── Audio Phase 3 (APU mode)                               │
  │   ├── Mode 2: pulse/wave/noise synthesis                 │
  │   ├── examples/audio_apu/main.c                          │
  │   └── *** Chiptune music ***                             │
  │                                                          │
  ├── Audio Phase 4 (SPC mode)                               │
  │   ├── Mode 3: sample playback + ADSR + tracker           │
  │   ├── examples/audio_spc/main.c                          │
  │   └── *** Full SNES-quality audio ***                ────┤
  │                                                          │
  └── At this point you have:                                │
      ✓ 60fps tear-free visuals (tiles, Mode 7, sprites)     │
      ✓ 6-button input with debounce                         │
      ✓ 3 audio modes (PCM / chiptune / SNES-style)         │
      ✓ All running bare-metal on a $6 board                 │
      ✓ API designed for future ADSP offload                 │
      ✓ Someone can write game.c and ship a cartridge    ────┘
```

---

*Four chips. No OS. Every watt serves the game.*
