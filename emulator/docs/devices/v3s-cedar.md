# `allwinner-v3s-cedar` — Cedar VE (H.264 decode + encode)

Source: [`src/hw/misc/allwinner-v3s-cedar.c`](../../src/hw/misc/allwinner-v3s-cedar.c)
Header: [`src/include/hw/misc/allwinner-v3s-cedar.h`](../../src/include/hw/misc/allwinner-v3s-cedar.h)

The V3s "Video Engine" at `0x01C0E000`. On real silicon this is a
fixed-function macroblock-by-macroblock H.264 baseline codec driven
by register pokes. We substitute libavcodec on the host, which gives
us the same "guest gets NV12 in DRAM" (decode) / "guest gets a bitstream
in DRAM" (encode) outcome with massively less code than reimplementing
the silicon.

The decode path (register block at `+0x200`) and the encode path
(ISP block at `+0xA00`, AVC encoder block at `+0xB00`) share the same
4 KiB MMIO window and the same QOM device — there is no separate encoder
device. The guest driver for encode is Jupiter's `lib/cedar_enc.c`
(ported from Bootlin's `cedrus_enc`).

## Registers

| Offset | Name                  | Behavior                                                       |
|--------|-----------------------|----------------------------------------------------------------|
| 0x000  | `VE_CTRL`             | Stored. Bit selections (clk/enable) honored as no-ops          |
| 0x0EC  | `VE_NV12_FORMAT`      | Stored                                                         |
| 0x0F0  | `VE_VERSION`          | Reads as `0x1680` (V3s VE rev)                                 |
| 0x024C | `VE_FRAME_INDEX_HI`   | Stored                                                         |
| 0x0240 | `VE_FRAME_DESC`       | Stored                                                         |
| 0x0250 | `VE_FRAME_PIC_INFO`   | Stored                                                         |
| 0x0254 | `VE_FRAME_NEIGHBOR`   | Stored                                                         |
| 0x0050 | `VE_FRAME_INDEX`      | Stored                                                         |
| 0x0200 | `VE_AVC_SHS_PARAM1`   | mb_w / mb_h (we use these)                                     |
| 0x0204 | `VE_AVC_SHS_PARAM2`   | qp / pic_init_qp                                               |
| 0x0208 | `VE_AVC_SHS_PARAM3`   | Stored                                                         |
| 0x020C | `VE_AVC_SHS_PARAM4`   | Stored                                                         |
| 0x021C | `VE_AVC_SHS_PARAM5`   | Stored                                                         |
| 0x0220 | `VE_AVC_IRQ_EN`       | Stored (we don't fire IRQs)                                    |
| 0x0224 | `VE_AVC_TRIGGER`      | **3 = skip-bits, 7 = init-swdec, 8 = avc-slice-decode**        |
| 0x0228 | `VE_AVC_STATUS`       | w1c. Bits: 0 = success, 1 = error, 8 = VLD busy                |
| 0x0230 | `VE_AVC_VLD_ADDR`     | Bitstream address (encoded — see below)                        |
| 0x0234 | `VE_AVC_VLD_OFFSET`   | Bit offset into bitstream                                      |
| 0x0238 | `VE_AVC_VLD_LEN`      | Bitstream length in bits                                       |
| 0x023C | `VE_AVC_VLD_END`      | Bitstream end address (encoded same as VLD_ADDR)               |
| 0x02E0 | `VE_SRAM_ADDR`        | Internal-SRAM auto-increment address (word index)              |
| 0x02E4 | `VE_SRAM_DATA`        | Internal-SRAM data (auto-increment on access)                  |

### `VLD_ADDR_VAL` encoding

The guest packs a 32-bit DRAM byte address into VLD_ADDR/VLD_END as:

```c
val = ((addr) & 0x0FFFFFF0) | (addr >> 28);
```

We invert it on read:

```c
addr = (val & 0x0FFFFFF0u) | ((val & 0xFu) << 28);
```

### Internal SRAM via 0x2E0/0x2E4

Real Cedar exposes 4 KiB of internal SRAM through the `VE_SRAM_ADDR /
VE_SRAM_DATA` pair: write the word offset to `0x2E0`, then read or
write `0x2E4` to access bytes there with auto-increment. The guest
stashes a "frame list" at offset `0x100`, where words 3 and 4 are the
output luma and chroma DRAM addresses we should write decoded NV12 to.

## Decode flow (TRIGGER == 8)

When the guest writes `8` to `VE_AVC_TRIGGER`:

1. Decode `VLD_ADDR` to a DRAM address. Read `VLD_LEN / 8` bytes from
   guest DRAM into a local buffer.
2. Open the codec lazily on first decode:
   - Synthesize baseline H.264 SPS+PPS from `SHS_PARAM1` (mb_w, mb_h)
     and `SHS_PARAM2` (qp). Hand-rolled bit writer with exp-Golomb
     encoding for `pic_width_in_mbs_minus1` / `pic_height_in_map_units_minus1`
     / `pic_init_qp_minus26`. Profile = baseline (66), level = 3.0,
     `frame_mbs_only_flag = 1`, no cropping, no VUI.
   - Glue with Annex-B start codes (`00 00 00 01`), assign as
     `ctx->extradata`, call `avcodec_open2`.
3. Prepend an Annex-B start code to the slice NAL bytes (libavcodec's
   parser needs it).
4. `avcodec_send_packet` + `avcodec_receive_frame`.
5. On a successful frame: convert YUV420P → NV12 (Y plane copied; UV
   plane interleaved). Read luma + chroma DRAM addresses from frame
   list at SRAM `0x100`. `address_space_write` the planes back.
6. Set `VE_AVC_STATUS = VE_STAT_SUCCESS` (always — even on libavcodec
   failure — to avoid a 50M-iteration guest poll spin).

If libavcodec/libavutil weren't found at build time, the device
compiles in stub mode: the trigger handler still sets SUCCESS but
writes nothing. Symptom: the SDL window stays on the previous frame
during cinematics.

## Encode registers (ISP `+0xA00` / AVC encoder `+0xB00`)

| Offset | Name              | Behavior                                                   |
|--------|-------------------|------------------------------------------------------------|
| 0x004  | `VE_RESET`        | Reads with "sync idle" bits 8+9 set (encoder init waits on it) |
| 0x0A00 | `ISP_PIC_INFO`    | `mb_w << 16 \| mb_h` (we use these for the frame size)      |
| 0x0A78 | `ISP_INPUT_Y`     | NV12 luma source DRAM addr                                  |
| 0x0A7C | `ISP_INPUT_C`     | NV12 chroma source DRAM addr                                |
| 0x0B08 | `AVC_ENC_PARA1`   | low 6 bits = fixed QP                                       |
| 0x0B18 | `AVC_ENC_TRIGGER` | **1 = put-bits (ignored), 8 = ENC_START**                  |
| 0x0B1C | `AVC_ENC_STATUS`  | w1c. Reads with PUT_BITS_RDY (bit 9) always set; bit 0 = FINISH |
| 0x0B20 | `AVC_ENC_PUTBITS` | Header bits the guest stages (ignored — see below)         |
| 0x0B80 | `AVC_ENC_STM_ADDR`| Output stream buffer DRAM addr                             |
| 0x0B8C | `AVC_ENC_STM_MAX` | Output capacity in **bits**                                |
| 0x0B90 | `AVC_ENC_STM_LEN` | Encoded length in **bits** (we set this; guest reads `/8`)  |

## Encode flow (TRIGGER == 8)

The guest stages an NV12 frame in DRAM (`lib/cedar_enc.c` even has an
ARGB→NV12 helper), pokes the ISP input + encoder registers, hand-writes
SPS/PPS/slice-header bit-by-bit via `put_bits`, then writes `8` to
`AVC_ENC_TRIGGER`. We:

1. Read `mb_w`/`mb_h` from `ISP_PIC_INFO`; input addrs from
   `ISP_INPUT_Y`/`ISP_INPUT_C`; QP from `AVC_ENC_PARA1`; output buffer
   from `AVC_ENC_STM_ADDR` (capacity from `AVC_ENC_STM_MAX / 8`).
2. Open an `AV_CODEC_ID_H264` encoder lazily (libx264), `pix_fmt = NV12`,
   `preset=ultrafast`, `tune=zerolatency`, constant `qp`. Reused across
   frames; rebuilt if the dimensions or QP change.
3. `address_space_read` the NV12 luma + interleaved-chroma planes straight
   into the `AVFrame`, force `pict_type = I` (the guest emits all-IDR).
4. `avcodec_send_frame` + `avcodec_receive_packet`. SPS/PPS are emitted
   **in-band** (no `GLOBAL_HEADER` flag), so the packet is a complete,
   self-contained decodable IDR — superseding the guest's hand-written
   headers.
5. `address_space_write` the bitstream to `AVC_ENC_STM_ADDR`, set
   `AVC_ENC_STM_LEN = bytes * 8`, set `AVC_ENC_STATUS` FINISH.

### Why we ignore `put_bits`

The guest writes SPS/PPS/slice-header bits through `AVC_ENC_PUTBITS`
before triggering, polling `PUT_BITS_RDY` (bit 9 of `AVC_ENC_STATUS`)
between each. We don't accumulate those bits — libavcodec regenerates a
correct header in-band — but we keep `PUT_BITS_RDY` permanently asserted
on reads so the guest's poll loops exit immediately. Bit 9 is outside the
`ENC_START` wait mask (`0xF`), so it never reads as a false FINISH.

Like decode, we always set FINISH (even on encoder failure, e.g. ffmpeg
built without an H.264 encoder → `AVC_ENC_STM_LEN = 0`) so the guest's
50M-iteration poll loop exits instead of hanging.

## Why we set SUCCESS even on failure

Jupiter's `cedar_video_av` polls `VE_AVC_STATUS` for up to 50 million
iterations. Reporting an error makes the engine retry the same
broken frame and effectively hang. Reporting success drops the bad
frame and advances; the cinematic plays through with occasional
stutter rather than locking up.

## Known limitations

- **Baseline only.** No CABAC, no B-frames, no reference-frame
  management beyond what libavcodec handles internally.
- **NV12 output only.** No tiled formats, no other YUV layouts.
- **No IRQ generation.** Guest polls `VE_AVC_STATUS`. Setting
  `VE_AVC_IRQ_EN` doesn't connect anything.
- **One stream at a time.** The codec context is lazily opened on
  first decode and reused. Switching resolution mid-session won't
  rebuild SPS/PPS — restart the payload.
- **Cedar VE has many more functions on real silicon** (JPEG, MPEG-2,
  H.265 on later revs). We model only the H.264 path.
