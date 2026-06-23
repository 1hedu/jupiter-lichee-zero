# `allwinner-v3s-codec` — DAC

Source: [`src/hw/audio/allwinner-v3s-codec.c`](../../src/hw/audio/allwinner-v3s-codec.c)
Header: [`src/include/hw/audio/allwinner-v3s-codec.h`](../../src/include/hw/audio/allwinner-v3s-codec.h)

The V3s digital audio codec at `0x01C22C00`. We model the DAC side
only — Jupiter doesn't capture audio. Samples come in via DAC_TXDATA
(or via DMA descriptors targeting that address) and go out a host
`SWVoiceOut`.

## Registers

| Offset | Name           | Behavior                                              |
|--------|----------------|-------------------------------------------------------|
| 0x00   | `DAC_DPC`      | Bit 31 = EN_DA; bits 17:12 = digital volume. Stored.   |
| 0x04   | `DAC_FIFOC`    | Bits 31:29 = FS code; bit 5 = TX_24BIT; bit 6 = MONO; bits 25:24 = FIFO_MODE; bit 4 = DRQ_EN; bit 0 = FLUSH. Triggers format reopen on write. |
| 0x08   | `DAC_FIFOS`    | Read-only. We always report "room available."          |
| 0x20   | `DAC_TXDATA`   | Sample FIFO. Each write pushes one word.               |
| 0x40   | `DAC_CNT`      | Bytes-transferred counter. Informational.              |
| 0x60   | `DAC_DAP_CTL`  | Signal-path enable. Stored.                            |

## Sample-rate decoding

The FS code field (bits 31:29 of `DAC_FIFOC`) selects an MCLK divider
against `PLL_AUDIO`'s output. Earlier code used a hardcoded `Hz` table;
that broke when payloads reprogrammed the PLL (Jupiter's 11025 Hz path
runs the PLL low + FS=2). The current code reads `PLL_AUDIO_CTRL` from
the CCU at `0x01C20008`:

```
f_pll = 24 MHz × (N+1) / ((M+1) × (P+1))
rate  = f_pll / div[FS]
```

with the divisor table:

```c
static const uint32_t codec_divs[8] = {
    512, 768, 1024, 1536, 2048, 3072, 128, 256,
};
```

Validated rates from Jupiter's `audio.c`:

| `PLL_AUDIO_CTRL` | f_pll      | Label        |
|------------------|------------|--------------|
| `0x80035514`     | 24.571 MHz | 48 kHz       |
| `0x80034E14`     | 22.571 MHz | 44.1 / 11.025 kHz |
| `0x80037A15`     | 33.545 MHz | 65.536 kHz   |
| `0x80011815`     | 13.636 MHz | 53.267 kHz   |

If PLL_AUDIO is off (bit 31 clear) we fall back to a canonical
24.576 MHz so the cold-boot 48 kHz default still works.

## Host audio backend

- `AUD_register_card("allwinner-v3s-codec", ...)`
- `AUD_open_out` with `audsettings { freq=rate, nchannels=1,
  fmt=AUDIO_FORMAT_S16 }`. Jupiter's pipeline is 16-bit mono regardless
  of what FIFOC's MONO bit says — `mix_buf` is `int16` per slot — so
  we hardcode mono.
- `AUD_set_active_out` only when `AUD_open_out` succeeds. If no audio
  backend is available the realize tolerates a NULL voice; writes to
  TXDATA still drain (counter advances) but make no sound.

When `DAC_FIFOC` is rewritten with a new rate, format, or stereo bit,
we close the existing voice and open a fresh one.

## DMA path

`aw_v3s_codec_attach_dma()` is called from the SoC realize. The DMA
device knows it's feeding the codec by the destination address —
descriptors targeting `0x01C22C20` (DAC_TXDATA) get their payload
forwarded directly to the codec instead of doing a memcpy. The codec
also calls `aw_v3s_dma_set_sample_rate()` when it reopens the voice,
so the DMA's per-descriptor pacing stays in sync.

## Known limitations

- **Capture side not modeled.** No ADC, no MIC paths.
- **Mono output forced.** Stereo writes go in but only the left channel
  reaches the host.
- **No analog codec.** `codec-analog` at `0x01C23000` is stub-mapped
  in the SoC; volume / mute writes there are silent no-ops.
- **No I2S out.** Real V3s can route digital audio out an I2S link;
  we don't model the I2S MMIO.
