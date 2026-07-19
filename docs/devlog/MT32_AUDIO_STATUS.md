# MT-32 Audio Status

## What Works
- MT-32 LA synthesis via Munt (real-time and pre-rendered)
- MIDI SMF playback with multi-track merge
- Pre-render to DRAM (100s, ~18MB) for zero-CPU playback
- 24-bit DAC mode (S32_LE, FIFO_MODE=00)
- 60fps rendering + MT-32 audio simultaneously (with render-skip when idle)
- Heap sized to 1MB (MT-32 needs ~915KB after lazy init)
- Reproducible munt build: `third_party/munt` is a pinned submodule
  (2.8.0), and `build/mt32/.prepared` generates `config.h` + applies the
  one `-fno-rtti` patch — no manual cmake, no local uncommitted patches.

## The multi-channel crackle — root-caused

The long-standing "faint crackling during multi-instrument passages"
had been chased through the entire real-time path (see Ruled Out
below), and the decisive clue was already in this log: **pre-rendered
audio crackles identically** — so the artifact is baked into the
rendered samples, not the delivery. There were three stacked causes:

### 1. math_neon poisoned the LA32 lookup tables (the math_neon build)
The old local munt patch (`HAVE_NEON` mmath.h) routed `EXP2F/LOG2F`
through math_neon's polynomial approximations (errors up to 22,000
absolute on expf). In the BIT16S renderer the per-sample path is pure
integer/table — but `Tables.cpp:81-86` builds `exp9[]`/`logsin9[]` **at
init** with `EXP2F`/`LOG2F`. Wrong math there means every LUT entry is
slightly wrong, i.e. constant subtle distortion on every sample, which
is exactly why `lib/mt32_lut_dump.cpp` (dump LUTs, diff against a Linux
reference) was the right instrument. The vendored stock 2.8.0 mmath.h
computes the tables with libm doubles → tables are exact. **Do not
re-wire math_neon into munt.** (It remains fine for game code; the
`g_math_path` A/B bench in mt32_rt still works for comparing.)
CPU cost is unaffected: the transcendentals only run at init and in the
FLOAT renderer, which we don't use.

### 2. Counter-phase partial mixing (authentic LA32 fuzz)
The LA32 mixes partials with `partialIndex & 4` in **counter-phase**
(`Partial.cpp:204`): pairs of near-identical partials subtract and
beat against each other. At `notes=14, partials=32` — where mt32_rt's
stats showed the fuzz — many partials sit close in frequency and this
is clearly audible. Munt ships `set_nice_partial_mixing_enabled()` for
exactly this; it is now enabled in mt32_poc / mt32_monkey / input_mt32
and is the default nice-mode in mt32_rt (cycle it off to hear the
authentic behaviour).

### 3. Per-partial int16 saturation at high polyphony
The BIT16S renderer accumulates each partial into a `Bit16s` stereo
buffer with saturation per partial (`Partial.cpp:364-367`,
`clipSampleEx`). Loud multi-partial passages can clip *intermediately*
even when the final output peak reads ~25000. The real unit distorts
here too (it's authentic), and the master-volume override in mt32_rt
exists to buy headroom. If a fully clean mix is ever wanted, the FLOAT
renderer accumulates without intermediate clipping — at real float
math cost per sample (viable to try now that the render budget is no
longer burned on SRC; see below).

## Real-time throughput with multiple channels

`mt32emu_open_synth()` defaults to **COARSE** analog mode: the synth
renders at 32 kHz and the internal resampler runs a per-sample sinc
filter to reach 48 kHz — the "10x slower" path noted in input_mt32.
**ACCURATE** analog mode renders natively at 32000·3/2 = **48000 Hz**,
so with our 48 kHz target the SRC is bypassed entirely
(`SampleRateConverter.cpp`: synth delegate when rates match).
mt32_poc and mt32_monkey previously used the default; all MT-32
examples now set `MT32EMU_AOM_ACCURATE` before `open_synth`. That
removes the dominant per-sample cost for busy multi-channel passages
(the BIT16S core itself is integer/LUT and cheap).

Also fixed on the SDK side: the audio DMA ISR's anti-underrun top-up
now stays out of the ring when no PCM channel is active — previously a
late synth frame could get a block of mixer silence spliced into the
stream (guaranteed dropout instead of a maybe-recovered late frame).

### Ruled Out (historical)
- Software ring buffer underruns (xrun=0 with TARGET_DEPTH=3800)
- Hardware codec FIFO underruns (hwxrun=0)
- Output clipping (peak never exceeds ~25000, well under 32767)
- Sample discontinuities (glitch=0 at threshold 8000, max_jump=6405)
- Dropped samples (drop=0)
- DMA timing (pre-rendered audio crackles identically)
- 16-bit vs 24-bit DAC (no change with S32_LE mode)
- Buffer depth (increased from 2400 to 3800, eliminated xruns but not crackle)
- Analog output stage (Linux sine wave is clean, OPN2 pre-render is clean)
- IRQ/NEON register corruption (vpush/vpop d0-d7 in IRQ handler)
- Heap overflow (fixed: 512KB → 1MB)

### Configuration (current)
```c
mt32emu_set_analog_output_mode(ctx, MT32EMU_AOM_ACCURATE); // native 48k, no SRC
mt32emu_set_stereo_output_samplerate(ctx, 48000.0);
mt32emu_open_synth(ctx);                                   // BIT16S renderer
mt32emu_set_nice_partial_mixing_enabled(ctx, MT32EMU_BOOL_TRUE);
mt32emu_set_output_gain(ctx, 2.0f);
audio_init_24bit();                                        // S32_LE DAC mode
```

### Verification checklist (on hardware)
- `mt32_lut_dump` output should now be bit-identical to the Linux
  reference (stock double-precision table build).
- mt32_rt at a 14-note passage: cycle nice-mode between "amp-ramp" and
  "amp+partial-mix" and listen for the fuzz appearing/disappearing.
- cpu=% in mt32_rt stats should drop noticeably vs the old COARSE+SRC
  numbers on poc/monkey-style configs.
- Remaining candidate if any hiss persists: V3s DAC noise floor
  (masked by OPN2's aggressive FM, exposed by MT-32's smooth pads) —
  push output gain toward 2.0-3.0 and use the analog HP volume.
