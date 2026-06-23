# `allwinner-v3s-dma` — DMA engine

Source: [`src/hw/dma/allwinner-v3s-dma.c`](../../src/hw/dma/allwinner-v3s-dma.c)
Header: [`src/include/hw/dma/allwinner-v3s-dma.h`](../../src/include/hw/dma/allwinner-v3s-dma.h)

Sun8i-family DMA controller at `0x01C02000`, 8 channels. Each channel
walks a linked list of descriptors in DRAM, copies bytes between the
descriptor's src/dst, and fires interrupts to GIC SPI 50 (shared).
Jupiter uses one channel: audio (DRAM → codec).

## Descriptor format (24 bytes, in DRAM)

| Offset | Field      | Description                              |
|--------|------------|------------------------------------------|
| 0x00   | `cfg`      | DRQ source/dest, src/dst width, burst, etc. |
| 0x04   | `src`      | Source byte address                      |
| 0x08   | `dst`      | Destination byte address                 |
| 0x0C   | `byte_cnt` | Bytes to transfer                        |
| 0x10   | `param`    | Misc per-channel param                   |
| 0x14   | `next`     | Pointer to next descriptor, 0 = end      |

## Shared registers

| Offset | Name        | Description                                        |
|--------|-------------|----------------------------------------------------|
| 0x00   | `IRQ_EN0`   | IRQ enable bits, channels 0–3 (4 bits/channel: HALF/PKG/QUEUE/—) |
| 0x04   | `IRQ_EN1`   | IRQ enable bits, channels 4–7                     |
| 0x10   | `IRQ_STAT0` | IRQ status, channels 0–3 (w1c)                    |
| 0x14   | `IRQ_STAT1` | Channels 4–7                                      |
| 0x20   | `GATE`      | Channel clock gate                                |
| 0x30   | `STATUS`    | Per-channel busy bits                             |

## Per-channel registers (at `0x100 + ch * 0x40`)

| Offset | Name        | Description                              |
|--------|-------------|------------------------------------------|
| +0x00  | `EN`        | Bit 0 = channel enable                   |
| +0x04  | `PAUSE`     | Bit 0 = pause                            |
| +0x08  | `DESC`      | Address of first descriptor              |
| +0x0C  | `CFG`       | Mirror of current descriptor's `cfg`     |
| +0x10  | `CUR_SRC`   | Current src address (advances during transfer) |
| +0x14  | `CUR_DST`   | Current dst address                      |
| +0x18  | `BCNT`      | Remaining bytes in current descriptor    |
| +0x1C  | `PARA`      | Mirror of current `param`                |

The guest reads `CUR_SRC` to figure out which half of a ping-pong ring
buffer just drained.

## How transfers run

When the guest writes 1 to `EN`:

1. Read the descriptor at `desc` from DRAM.
2. Set `cur_src/cur_dst/cur_bcnt/para` from the descriptor.
3. If `dst == 0x01C22C20` (DAC_TXDATA): forward payload directly to
   the codec via `aw_v3s_codec_dma_push()`. Else: copy bytes via
   `address_space_read/write`.
4. Schedule the per-channel timer to fire after
   `byte_cnt / sample_rate` virtual seconds.
5. On timer fire: assert PKG IRQ (or HALF, depending on which 4-bit
   slot is enabled). If `next != 0`, follow the chain by re-running
   step 1. If `next == 0`, deassert `EN`.

## Why the rate limit

A naive implementation transfers all bytes in 0 virtual time, fires
PKG, and immediately advances. Jupiter's audio path is a double-buffer
ping-pong driven by PKG: the ISR refills the freed half. Instant
transfer either:

- starves the refill loop (bursts of PKG faster than the guest can
  refill) →  audio stutter, or
- floods the host audio backend (every sample fed in 0 ns) →  XRUN.

Holding each descriptor for `byte_cnt / sample_rate` virtual seconds
gives the guest the same wakeup cadence it'd see on real silicon.
The rate is set by the codec via `aw_v3s_dma_set_sample_rate()` when
it reopens the voice.

## Known limitations

- **Only DAC_TXDATA fast-path.** Other dst addresses do generic
  `address_space_read/write`. Fine for current payloads.
- **`cfg` interpretation is partial.** We read width / DRQ source for
  diagnostics but the actual transfer is byte-stream. No
  burst-size or width-promotion semantics.
- **HALF/QUEUE IRQs.** The 4 IRQ slots per channel are masked the same
  way real HW does, but only PKG is actually emitted on
  descriptor-end. HALF (mid-descriptor) isn't generated. Jupiter
  doesn't enable HALF.
- **Linked-list cycle detection.** None. A descriptor with `next` ==
  itself loops forever.
