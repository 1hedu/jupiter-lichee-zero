# Development phases

A narrative walk through how this emulator got built, in roughly the
order it happened. Each phase corresponds to "what guest payload broke
next when we ran it on the previous phase's emulator." This is not a
spec; it's a postmortem of the design decisions that turned out to
matter.

If you want to know *what* a device does, read its source header
comment or its [`devices/`](devices/) page. This doc is about *why* we
did it the way we did.

---

## Phase 1 + 2 — Boot + display

**Goal**: get a Jupiter `.bin` to print "hello" on UART0 and draw
something on the LCD.

**Decisions that mattered:**

- **Bare-metal entry, not Linux.** Jupiter targets U-Boot's `go
  0x41000000` handoff. We don't simulate SPL or U-Boot — we drop the
  `-kernel` image at `0x41000000` and force the CPU PC there on reset.
  No DTB, no PSCI, no firmware. This means our machine init is ~30
  lines instead of the ~300 a typical Linux-targeted ARM machine
  needs.
- **64 MiB RAM is hardcoded.** Real V3s boards ship with 64 MiB DDR2
  in-package; the Jupiter linker script lays out static allocations
  assuming exactly that. Rather than parameterize, we abort if
  `-m` isn't 64.
- **Reuse H3 device models.** The H3 CCU, sysctrl, cpucfg, and SID
  models in stock QEMU work close enough that bringup-poll-PLL_LOCK
  succeeds. We didn't write V3s-specific models for these because the
  guest only checks bits the H3 model already provides.
- **Display is a stitched-together compromise.** DE2 + Mixer0 + TCON0
  live behind a single `allwinner-v3s-display` device that records
  every register write but only acts on the handful Jupiter actually
  flips during scan-out: VI0 enable, VI0 framebuffer address, UI0
  enable + alpha, TCON_GCTL/TCON0_CTL global enables. This isn't
  register-accurate emulation — it's "make Jupiter's `video.c` happy."

**Result:** Jupiter logo demo renders correctly. `colorbars.bin`
shows the right pattern.

---

## Phase 3, part 1 — PIO + HSTimer

**Goal**: bring up GPIO so the Jupiter input layer (`lib/input.c`)
can poll buttons, and add the high-speed timer Jupiter uses for sub-µs
delays.

**Decisions that mattered:**

- **PIO is a register store, not a behavioral model.** CFG/DRV/PULL
  are pure read/write storage. DAT is special: reads composite the
  guest-written output level with the `ext_in[port]` bits siblings
  asserted via gpio-in.
- **HSTimer assumes 200 MHz AHB.** The real clock depends on the CCU
  AHB divider, but Jupiter only uses the timer for relative timing
  (compare two readouts), so an exact rate doesn't matter.

---

## Phase 3, part 2 — Virtual N64 pad + edge fan-out

**Goal**: make `examples/menu/main.c` (which polls an N64 controller
over a single GPIO) actually receive input.

**The problem we hit and the design it forced.**

The first cut was straightforward: the pad device drives `ext_in` for
PE20 to "pull the line low," and `lib/input.c`'s polling loop sees
the right level. That worked for static reads but failed the moment
the protocol kicked in.

The N64 Joybus protocol has both sides talk on the same wire,
half-duplex:

- The console (i.e. the Jupiter guest) issues a command by clocking
  bits onto the line.
- The pad responds with bits clocked the same way.

The console **doesn't drive the line low by writing a 0 to DAT**. It
keeps DAT=0 the whole time and toggles the **CFG** between
output-enable (drives low through the DAT=0) and input (Hi-Z, external
pullup wins, line idles high). So the pad device needs to know when
the console-side electrical level changed — and that's a function of
both DAT *and* CFG, not just DAT.

Solution: introduce `port_effective_level()` in the PIO. After every
CFG or DAT write, it recomputes the effective output level for the
whole port and fires `qemu_set_irq(pin_out[…])` only on bits that
actually transitioned. The pad's gpio-in handler then sees the
correct edge sequence.

A second nasty problem: timing. Joybus uses 4 µs bit cells. Without
`-icount shift=auto`, virtual time advances in ~20 µs chunks; every
bit's "low pulse width" rounds to the same value and decoding
fails. Adding `-icount` to `scripts/run.sh` fixed it.

**Result:** `menu.bin` boots and the D-pad scrolls.

---

## Phase 4 — Audio codec + DMA

**Goal**: get `examples/jupiter_moon/` (a wavetable demo) to make sound.

**Decisions that mattered:**

- **DMA is rate-limited.** Naively, the engine could transfer all bytes
  in a descriptor instantly in virtual time, then fire the PKG IRQ.
  Jupiter's audio path is a double-buffer ping-pong driven by that IRQ
  — instant transfer either starves the refill loop (too few wakeups)
  or floods the host audio backend (XRUN). We hold each descriptor for
  `byte_count / sample_rate` virtual seconds before firing PKG.
- **Codec FIFO is "infinitely roomy."** `DAC_FIFOS` always reports
  room available; the host audio backend is the actual sink with its
  own buffer/draining behavior. This decouples the guest's TXFL polling
  from real-time pacing.

A later fix replaced a hardcoded `Hz` table for FIFOC's FS field with
a live decode of `PLL_AUDIO_CTRL`. The earlier table assumed default
PLL settings; payloads that reprogrammed the audio PLL (e.g. for 11025
Hz playback) came out at the wrong pitch.

---

## Phase 5 — Storage path (SDMMC0)

**Goal**: let `examples/sdmmc/main.c` find a card, mount FAT, list files.

The QEMU `allwinner-sdhost-sun5i` device is register-compatible with
the V3s SDMMC0 — we wire it at `0x01C0F000`, IRQ to GIC SPI 23, and
hand it the `sd-bus` from `lichee-zero` machine init. `-drive
if=sd,file=...` plugs into that bus.

The first SD image we built (one big FAT16 partition starting at LBA
2048) **didn't mount**. The error from FatFs was `rc=13`
(`FR_NO_FILESYSTEM`). Took a while to track down: `third_party/fatfs/
diskio.c`'s `VolToPart` table maps drive `0:/` to physical partition
#2 (1-indexed), so a one-partition image looks empty as far as drive
0 is concerned. See Phase 7.

The music-pack splice convention came from real-HW practice: WC1's
music pack lives outside any FAT partition, raw at LBA 524288. Jupiter
reads it via raw block ops. `make-music-sd.sh` mirrors that layout.

---

## Phase 6 — Joybus protocol + Controller Pak

**Goal**: make `examples/cpak_browser/main.c` work — list, read, and
write Controller Pak pages.

This is where the pad device went from "decode one command" to "full
protocol implementation." Added:

- TX path: schedule per-bit timer expiries to clock the response back
  on the same line, with the proper 1-µs / 3-µs cell shape. Stop bit
  signals end-of-message.
- Command dispatch: `0x00 INFO` (says "Cpak inserted"), `0x01
  BTN_STATE` (controller buttons), `0x02 READ32` (32 bytes from pak),
  `0x03 WRITE32` (32 bytes into pak + CRC verification).
- 32 KB in-memory pak buffer. Lost on reset; persistence is a future
  item.

CRC-8 with polynomial 0x85 is what the N64 OS expects. Mismatched CRC
returns `0xFF` and the OS retries.

---

## Phase 7 — SD partition layout fix

**Goal**: stop debugging and just make `0:/` mount.

`scripts/make-sd-image.sh` rewritten to produce a 2-partition layout:

| LBA           | Type    | Purpose                                     |
|---------------|---------|---------------------------------------------|
| 2048..4095    | 0x83    | 1 MiB Linux dummy — placeholder for SPL     |
| 4096..524287  | 0x06    | ~254 MiB FAT16 — what `0:/` mounts to       |
| 524288+       | (free)  | WC1 music pack splice                       |

The MBR is hand-written via Python (struct-pack) because `parted` /
`fdisk` get cute about default partition boundaries.

`scripts/populate-fat.sh` was added to mount partition #2 via loop
device and `cp -v` content from the Jupiter build tree. Loop mount
needs `sudo`, which is annoying but `mtools` is even more annoying.

After this, the war1 binary boots all the way through Stratagus init,
loads its Lua scripts off FAT, and reaches the cinematic phase.

---

## Phase 8 — Cedar VE / H.264 decode

**Goal**: make war1's intro cinematic actually render frames.

Real Cedar is a fixed-function macroblock-by-macroblock H.264 baseline
decoder driven by register pokes. Replicating that would be a
multi-month project; instead we substitute libavcodec on the host:

- The guest writes bitstream pointers (VLD_ADDR / VLD_END), output
  frame buffer pointers (in a frame list at internal SRAM `0x100`,
  words 3 and 4), and slice header parameters (mb_w, mb_h, qp).
- It writes `8` to VE(0x224) to trigger a slice decode.
- Our handler recovers the bitstream from guest DRAM, prepends an
  Annex-B start code (`00 00 00 01`), feeds it to `avcodec_send_packet`.
- On `avcodec_receive_frame` we get YUV420P; convert to NV12 (Y plane
  followed by interleaved UV plane), write the planes back to the
  guest-pointed luma + chroma DRAM addresses.
- We always set the success status bit, even on libavcodec failure —
  the guest polls in a tight 50M-iteration loop on failure, which
  spins forever. Better to drop the frame and continue.

The first big surprise: vbin streams contain **slice NALs only**.
Real Cedar gets SPS/PPS from VE registers, not from the bitstream.
libavcodec doesn't have that out-of-band channel — it expects either
inline parameter sets or an `extradata` blob. The follow-up Phase 8
fix synthesizes baseline H.264 SPS+PPS from the SHS_PARAM register
contents (mb_w/mb_h/qp), assembles them with start codes, and feeds
them as `ctx->extradata` before opening the codec.

---

## Unreleased — virt-ym3438

**Goal**: let `examples/ym3438/main.c` make sound on the emulator.

The Jupiter `lib/ym3438_hw.c` driver is a GPIO bit-banger: it writes
the 8-bit data byte to `PB_DAT`, sets `A0/A1/CS` via `PG_DAT`, pulses
`/WR` low for ~200 ns, and waits ~10 µs between byte writes.

We snoop those transitions instead of building a virtual MMIO chip
the guest doesn't expect. The `virt-ym3438` device has 13 GPIO inputs
wired from the PIO's `pin_out[]`:

- Lines 0–7: PB0..PB7 (data bus)
- Lines 8–9: PG0..PG1 (A0, A1)
- Lines 10–12: PG2..PG4 (/WR, /CS, /IC)

A `/WR` rising edge with `/CS` asserted (low) latches one bus cycle:
port = `(A1 << 1) | A0`, data = current PB shadow. We feed that into
Nuked-OPN2 via `nukedopn2_write`.

Audio output: a `SWVoiceOut` opened at 44.1 kHz stereo. The host
audio backend's pull callback asks Nuked-OPN2 for samples and clamps
to S16. If `-audio` isn't configured, the device tolerates a NULL
voice and silently does nothing — the rest of the SoC realize must
succeed regardless, otherwise the menu (which doesn't use the chip)
won't boot.

---

## Things we deliberately *don't* model

| What                               | Why                                          |
|------------------------------------|----------------------------------------------|
| DRAM controller bringup            | We hand 64 MiB at 0x40000000 and skip it     |
| CCU PLL ramp timing                | Guest only polls PLL_LOCK; H3 model handles  |
| TCON VBLANK/HSYNC                  | Guest uses HSTimer for frame timing          |
| GICv2 priorities                   | No payload abuses priority                   |
| USB OTG, CSI, crypto engine, SPI, I2C | No payload uses these                     |
| TLB / cache coherency at MMU level | TCG handles correctness at instruction level |
