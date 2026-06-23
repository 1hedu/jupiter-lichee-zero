# MT-32 Audio Status

## What Works
- MT-32 LA synthesis via Munt (real-time and pre-rendered)
- MIDI SMF playback with multi-track merge
- Pre-render to DRAM (100s, ~18MB) for zero-CPU playback
- 24-bit DAC mode (S32_LE, FIFO_MODE=00)
- NEON-accelerated math (HAVE_NEON + math_neon library)
- 60fps rendering + MT-32 audio simultaneously (with render-skip when idle)
- Heap sized to 1MB (MT-32 needs ~915KB after lazy init)

## Known Issue: Subtle Crackling
Faint crackling audible during multi-instrument MT-32 passages. Not present with OPN2, NES APU, or Linux sine wave through the same DAC.

### Ruled Out
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

### Suspected Causes
1. **math_neon approximation errors** — HAVE_NEON replaces exp()/log()/pow() with polynomial approximations. Reported errors up to 22,000 absolute on expf. LA32 wave generator calls EXP2F dozens of times per sample. mt32-pi does NOT use math_neon and sounds clean. Needs A/B test with HAVE_NEON disabled (attempted but inconclusive due to separate bug during test).
2. **V3s DAC noise floor** — integrated codec headphone amp has inherent noise. MT-32's smooth tones expose it; OPN2's aggressive FM masks it.

### Configuration (current)
```c
// mt32-pi defaults
mt32emu_set_stereo_output_samplerate(ctx, 48000.0);  // 32kHz→48kHz SRC
mt32emu_open_synth(ctx);                              // COARSE analog, BIT16S renderer
mt32emu_set_output_gain(ctx, 1.0f);                   // NICE DAC mode (default)
audio_init_24bit();                                    // S32_LE DAC mode
```

### Next Steps
- Clean A/B test: build two binaries (HAVE_NEON on/off), same pre-rendered data, listen back-to-back
- Try higher output gain (2.0-3.0) to push signal above DAC noise floor
- Consider replacing math_neon with ARM's optimized-routines math library (accurate + fast)
