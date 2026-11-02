# A64 audio and Cedar implementation notes

These paths are compiled for the original PINE A64+ with 2 GB RAM. Their
peripheral behavior still needs a physical board test. The previously checked
SD2 bring-up image established the board's CPU, MMU, SD and display path; it did
not exercise this audio or video-decoder implementation.

## Audio

`audio_hw.c` implements the A64's onboard analog headphone output. The original
V3s audio register layout is excluded from the A64 build. The A64 has separate
codec-I2S transport (`0x01c22c00`), digital AIF/DAC (`0x01c22e00`) and indirect
analog (`0x01f015c0`) blocks.

- The output is the onboard **3.5 mm jack**. HDMI audio is not implemented.
- The driver initializes the headphone charge-pump supply through AXP803 RSB:
  it verifies the PMIC identity, selects ELDO1 at 1.8 V, enables that rail, and
  checks the readback. It preserves unrelated regulator settings. This matters
  because the supplied U-Boot configuration does not initialize the PMIC.
- Hardware runs at exactly 48 kHz, stereo, with 24 significant bits in 32-bit
  DMA words. The public source ring is signed 16-bit interleaved stereo; selecting
  `audio_init_24bit()` does not add precision to those source samples.
- A linear resampler accepts logical source rates from 8 kHz through 192 kHz,
  including the demos' nonstandard synthesizer rates. `audio_set_rate()` stops
  DMA and resets the source ring; callers fill the ring and call `audio_start()`
  again. `current_audio_rate` describes that logical source rate.
- DMA channel 0 uses DRQ 15, 32-bit words, four-word bursts, and GIC interrupt
  ID 82. Each descriptor holds 512 stereo hardware frames. At the maximum
  192 kHz source rate, one descriptor consumes at most the ring's 2048 source
  frames. The descriptor completion interval is about 10.67 ms.
- A64 DMA autogating is at `DMA+0x28`; `DMA+0x20` is the security register and
  is not written. Reinitialization and cleanup stop channel 0 without resetting
  the whole DMA controller.
- The DMA interrupt replenishes only the completed half. PCM auto-mixing runs
  only while PCM channels are active, so it cannot fill the ring with silence
  ahead of the OPN2, Munt or SC-55 producers. A64 PCM/APU/FM mixer calls respect
  the ring's capacity and publish stereo frames atomically against the ISR.
- `audio_update()` services keyboard/menu requests in normal context. It also
  polls pending DMA completions for callers that temporarily mask interrupts.
  Peripheral waits have both a timer deadline and a finite poll limit.

The A64 compatibility entry point `audio_genesis_mix_asm()` delegates to the C
mixer. The original A7 assembly writes mono ring slots and is excluded from the
A64 build.

Expected serial markers include `AXP803 1.8V; AIF1 -> DAC -> 3.5mm headphones`
and `48000 Hz stereo, 24-bit I2S, DMA0; ready`. A PMIC, PLL or FIFO timeout leaves
audio stopped and prints a specific initialization failure. Physical tests
should confirm both channels, pitch, repeated demo launches and underrun counts.

## Cedar sprite round trips

The A64 path uses a **CPU Baseline I_PCM H.264 encoder followed by the actual
Cedar H.264 decoder**. A64 silicon has H.264 encoding capability, but the SDK's
Bootlin-derived encoder register sequence is verified for V3/V3s/S3. No V3s
encoder register accesses are compiled into the A64 build.

I_PCM is a standard H.264 macroblock format. This encoder emits Annex B SPS,
PPS and IDR NAL units with emulation-prevention bytes, and its output decodes
in FFmpeg. It preserves the converted NV12 samples exactly; it does not
compress them. ARGB-to-NV12 4:2:0 conversion remains lossy.

All four sprite demos display `CPU I_PCM encode / Cedar decode` and either
`H.264 ROUND TRIP COMPLETE` or `PIPELINE FAILED - RAW SPRITES`. A decoder error
does not masquerade as a successful hardware round trip. The original sprite
atlas remains available for the demos' raw fallback.

The hardware decoder uses the A64 VE bus gate/reset bank, a 402 MHz VE clock
and SRAM C1. It preserves the separate DE2 SRAM selection at `SYS_CTRL+4` bit
24. The supported driver dimensions are even widths up to 512 pixels and even
heights up to 1024 pixels, covering every included sprite atlas. This is a
driver buffer limit, not the A64 silicon's video limit. Decoder waits are
bounded, and a failed decode resets only VE.

### Reserved physical memory

The menu linker must keep **all of `0x43000000..0x43800000` reserved for Cedar**.

| Area | Address / extent |
| --- | --- |
| Decoder input NAL | `0x43000000`, 1 MiB capacity |
| NV12 luma | `0x43100000`, 1 MiB reserved |
| NV12 interleaved chroma | `0x43200000`, 1 MiB reserved |
| Picture metadata | `0x43300000`, 256 KiB |
| Neighbor metadata | `0x43340000`, 32 KiB |
| Colocated motion vectors | `0x43348000`, 256 KiB |
| Demo ARGB atlas | `0x43400000`; largest included atlas ends at `0x43596000` |
| Encoded H.264 stream | `0x43700000`, 1 MiB capacity |

The former V3s decoder output arena was too small for the 448×928 SNES atlas.
The V3s hardware encoder scratch also overlapped that atlas. The A64 buffer
layout and CPU encoder avoid both collisions. CPU caches are cleaned before
VE reads and invalidated after VE writes using the board's cache functions.

## SC-55 integration fixes

The driver is checked against the pinned Nuked-SC55 revision used by this menu.
Its startup now resets the H8, sub-MCU, PCM and timers. The missing sub-MCU reset
was significant: `SM_Reset()` loads the sub-MCU's actual reset vector.

`sc55_warmup(1000)` advances one second of the upstream model's 24 MHz MCU
clock, discards startup samples, and services keyboard requests. Counting
samples at a nominal rate was unsuitable during firmware initialization,
because the firmware changes PCM slot count and oversampling during startup.

`sc55_render()` delivers exactly the requested callback count, carrying an
extra oversampled frame into the next call. A stalled render has a finite
instruction budget, returns short and stops further rendering; it does not
manufacture successful silent output. The driver rejects missing or incorrectly
sized ROM buffers before copying or unscrambling them. Actual synthesizer
operation still requires the user's matching SC-55 mkII ROM set.

## Verification

- Cortex-A53 AArch32 hard-float compilation of the audio/Cedar sources, all four
  Cedar sprite demos, and the SC-55 driver against the pinned upstream headers.
- `python3 boards/pine64/tests/test_cedar_ipcm.py`: FFmpeg decodes the actual
  encoder implementation with byte-for-byte NV12 equality at 384×80, 480×64,
  512×480, 448×928 and 512×1024, plus a cropped 18×30 frame and a zero-byte
  escaping case. Invalid arguments and output-capacity guards also pass.
- `test_sc55_driver.cpp` compiles the actual driver against an emulator fixture
  to check startup reset calls, exact callback counts across oversampled pairs,
  warmup clock advancement, discarded startup output and bounded stalled render.
  Its compile/run command is in the file. This does not substitute for testing
  the emulator with a matching ROM set or listening to board output.

## Primary references

- [Linux v6.12 A64 codec-I2S variant and sample packing](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/sound/soc/sunxi/sun4i-i2s.c?h=v6.12)
- [Linux v6.12 digital AIF/DAC codec](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/sound/soc/sunxi/sun8i-codec.c?h=v6.12)
- [Linux v6.12 A64 analog codec and headphone supplies](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/sound/soc/sunxi/sun50i-codec-analog.c?h=v6.12)
- [Linux v6.12 PINE64 board regulator routing](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/arch/arm64/boot/dts/allwinner/sun50i-a64-pine64.dts?h=v6.12)
- [Linux v6.12 A64 DMA layout](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/dma/sun6i-dma.c?h=v6.12)
- [Linux v6.12 A64 clock settings](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/clk/sunxi-ng/ccu-sun50i-a64.c?h=v6.12)
- [Linux v6.12 separate VE/DE2 SRAM ownership](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/soc/sunxi/sunxi_sram.c?h=v6.12)
- [Linux v6.12 Cedrus A64 capability and clock variant](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/staging/media/sunxi/cedrus/cedrus.c?h=v6.12)
- [Linux v6.12 H.264 decoder register and buffer setup](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/staging/media/sunxi/cedrus/cedrus_h264.c?h=v6.12)
- [Allwinner A64 user manual, hosted by PINE64](https://files.pine64.org/doc/datasheet/pine64/Allwinner_A64_User_Manual_V1.0.pdf)
- [Bootlin's stated V3/V3s/S3 hardware encoder scope](https://bootlin.com/blog/open-source-linux-kernel-support-for-the-allwinner-v3-v3s-s3-h-264-video-encoder/)
- U-Boot v2025.10 `drivers/power/axp_spl.c`, `drivers/power/regulator/axp_regulator.c`,
  `drivers/power/pmic/axp.c` and `drivers/i2c/sun8i_rsb.c` from the firmware
  source checkout provide the AXP803 identity, regulator and RSB definitions.
