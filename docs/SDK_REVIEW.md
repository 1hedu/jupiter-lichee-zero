# SDK review — obvious bugs + proposed cleanup/additions

Full-tree review of `lib/` + `include/` + `scripts/` (startup/linker), with a
survey of all 60 examples to find code the examples have already proven out
that belongs in the SDK. Findings marked **[verified]** were re-checked
line-by-line against the source; the rest were found by careful reading and
are quoted with file:line so they're quick to confirm.

---

## 1. High-severity bugs

### Memory-map collisions (the #1 theme)

The SDK's DRAM map lives in five different places — `include/v3s.h`,
`lib/cedar.c`, `lib/cedar_enc.c`, per-example `#define`s, and
`scripts/jupiter.ld` — and they have drifted apart:

- **[verified] `lib/cedar_enc.c:20` — encoder bitstream buffer overlaps the
  live stacks.** `ENC_STREAM_ADDR 0x43700000` + `ENC_STREAM_SIZE 0x100000`
  spans up to `0x43800000`, but `scripts/jupiter.ld:84-85` puts
  `__irq_stack_top` at `0x437F0000` and `__stack_top` at `0x43800000` — the
  top 64 KB+ of the stream buffer *is* the SVC/IRQ stack, and the VE is
  licensed to DMA over it (`AVC_STM_END = ENC_STREAM_ADDR + SIZE - 1`,
  cedar_enc.c:293). Any encode producing more than ~960 KB of bitstream
  writes over the call stack. Even today, every encode
  `dcache_clean_range()`s stack lines.

- **[verified] `lib/cedar_enc.c:22-26` — all encoder work buffers
  (`0x43100000`–`0x435FFFFF`) sit inside the CODE region** — the exact
  collision class already diagnosed and fixed for the *decoder* (see the
  comment at `lib/cedar.c:16-21`). A build with >~33 MB of text+data+bss has
  live state there; `cedar_argb_to_nv12` and the VE's reconstruction DMA
  silently corrupt it.

- **[verified] `lib/video.c:164` — `video_init()` zeroes
  `0x43E00000`–`0x43FFFFFF` ("U-Boot's FB"), which is now the decoder's
  entire buffer region** (`BUF_INPUT`/`BUF_LUMA`/`BUF_CHROMA`,
  cedar.c:22-27). Order-dependence with no check: `examples/cedar_jpeg/main.c`
  decodes (line 105) *before* `video_init()` (line 120), so the decoded NV12
  is wiped before `cedar_nv12_to_argb` (line 132) — the decode probe example
  is currently broken by the buffer relocation.

- **Examples still cache-invalidate the decoder's OLD addresses.**
  `cedar_video/main.c:345`, `cedar_video_av`, `cedar_nes:181`,
  `cedar_gb:162`, `cedar_snes`, `cedar_genesis:289` all
  `dcache_invalidate_range(0x43100000/0x43200000, ...)` — which is now the
  *encoder input inside CODE*. It only works because `cedar_h264_decode`
  invalidates the real buffers internally (cedar.c:268). In a large-bss
  build, invalidating dirty lines of live program data discards state.
  These calls are redundant and should be deleted.

- **Stale defines:** `lib/cedar.c:30-32` (`BUF_ENC_LUMA /* 0x43100000 */`
  comments no longer match what the macros resolve to);
  `scripts/jupiter.ld:88` `__fb1_start = VRAM+0x200000 = 0x43A00000`
  contradicts `FB1_ADDR 0x43900000` and collides with `SPR_ADDR`.

**Fix direction:** one `include/memory_map.h` (or linker-provided symbols)
as the single source of truth for FB0/FB1/SPR/OVL/AUDIO_BUF/cedar/NAL_STAGE
/stacks, plus accessors (`cedar_enc_stream_addr()`, decoder luma/chroma
getters) so examples stop hardcoding `0x43700000`. Move `ENC_STREAM_ADDR`
and the encoder work buffers out of CODE (the `0x43A00000+` VRAM gap or
BSS_LOW has room).

### Decoder/encoder bounds

- **`lib/cedar.c:186-191` — no bounds check on `w`/`h`.** `BUF_CHROMA` is
  only 256 KB above `BUF_LUMA`; decode above ~512×480 overflows luma into
  chroma, and ≥848×480 overruns into `NAL_STAGE`. Silent garbage, no error.
- **`lib/cedar.c:172-174` — `memcpy` of the NAL into the 256 KB `BUF_INPUT`
  with no size check** — a >256 KB IDR NAL corrupts `BUF_PIC_INFO`.
- **`lib/cedar.c:66` — `cedar_init()` waits for PLL_VE lock with no
  timeout** (decode/encode polls do have one) → silent full-system hang.
- **`lib/cedar_enc.c:251-262` — rec/subpix/mb_info sizes never checked
  against their fixed slots** (≥ ~1088×736 collides with the bitstream
  buffer).
- **`lib/cedar_enc.c:80-104` — `cedar_argb_to_nv12` pads columns but not
  rows**: heights not a multiple of 16 encode stale DRAM in the bottom
  macroblock row.

### Audio (lib/audio.c)

- **[verified] `audio.c:392-395` — the ISR anti-underrun top-up never fires
  until the ring is completely empty, then mixes double.** Guard is
  `depth + AUDIO_BUF_HALF*2 <= MIX_BUF_SIZE`, i.e. `depth + 4096 <= 4096` —
  true only at `depth == 0`, at which point the dropout has already
  happened. And `audio_mix(AUDIO_BUF_HALF)` writes `2*AUDIO_BUF_HALF = 4096`
  slots (the whole ring), double the 2048 consumed per half. The comment
  ("refill exactly what was just consumed") matches neither. Likely intent:
  `depth + AUDIO_BUF_HALF <= MIX_BUF_SIZE` with `audio_mix(AUDIO_BUF_HALF/2)`.
- **[verified] `audio_mix()` is reentrancy-unsafe but is called from both
  the main loop and the DMA ISR** (audio.c:394 + 430-451). `mix_buf[mix_wr++
  & MASK] = L; ... = R;` — an ISR firing between the L and R stores
  permanently swaps the stereo interleave, and per-channel `pos_int/pos_frac`
  updates get double-applied. Needs IRQ masking around the main-loop mix (or
  a lock-free single-producer design where the ISR is the only mixer).
- **`audio.c:452-458` — `audio_pcm_play()` lacks the deactivate/barrier
  sequence `audio_pcm_play_rate()` was given** for exactly this ISR race —
  retriggering a channel as the DMA IRQ lands can read a new (shorter)
  sample pointer with the old, larger position → OOB read.
- **`audio.c:340-376` — strict-alternation half refill has no resync path**:
  if both halves complete under a long IRQ-masked section,
  `audio_next_refill_b` inverts permanently (the code even records the
  disagreement in `audio_dbg_cursrc_disagree` but never uses it to resync).
- **`audio.c:563/963` — the ISR top-up hardwires the PCM mixer**: apps using
  `audio_apu_mix`/`audio_genesis_mix` (mono producers) get blocks of
  PCM-mixer silence injected whenever the ring drains.

### PPU renderers render out of bounds in full-screen mode

All three software PPUs size buffers for *native* resolution but index them
by *framebuffer* coordinates when `authentic == 0` (rw/rh = fb_w/fb_h =
480×272 on the reference panel):

- **[verified] `lib/nes.c:137/157` — `uint8_t scanline_count[NES_FULL_H]`
  (240) indexed by `screen_y < rh` (272)** → stack smash for sprites on
  lines 240–271.
- **[verified] `lib/nes.c:214/230` — `bg_opaque_buf[256*240]` written as
  `bg_opaque_buf[sy * rw + sx]` with rw=480, rh=272** → writes to index
  ~130 559 in a 61 440-byte static buffer (reads at nes.c:172 overrun the
  same way).
- **`lib/snes.c:443/457`** and **`lib/gb.c:61-115`** — same two patterns
  (`line_count[224]`/`line_count[144]`, `bg_opaque[160*144]`).

Fix: size the buffers for the max render rect (or clamp sprite Y to native
height), and compute BG opacity during the BG pass instead of a second
full decode loop.

- **`lib/snes.c:17/28` — 10-bit tile index truncated to `uint8_t tidx`**
  (`SNES_GET_TILE` masks 0x3FF). Tiles ≥256 alias into 0–255; multiples of
  256 become "transparent". Genesis got this right (`uint16_t`,
  genesis.c:23).
- **`lib/nes.c:94/220` — negative vertical scroll wraps wrong**: `(uint32_t)
  (sy + scroll_y) % 240` — 2³² isn't divisible by 240, so −1 maps to row 15,
  not 239. (X is safe only because 256 divides 2³².)
- **`lib/nes.c:175` — NULL `bg` crashes `render_sprites`**
  (`bg->palette_ram` dereferenced; only `render_bg` guards NULL).
- **`nes.c:141-157` / `snes.c:447` / `gb.c:88` — the 8-per-scanline sprite
  limit keeps the *lowest*-priority sprites**: rendering runs in reverse OAM
  order, so the saturating counter drops OAM-0 (the player) on crowded
  lines instead of the décor.

### Input

- **`lib/input.c:186-206` — Genesis 6-button detection tests one TH pulse
  too early.** The all-zero low-nibble signature appears on the *third*
  TH-low phase; the code tests after the second, so `is_6btn` is always
  false and Z/Y/X/Mode never register. (The cycle-5 extra-button read is
  off-phase for the same reason.)
- **`lib/input.c:252-255/341` — N64 timeout commits `buttons = 0`**, so a
  one-frame glitch fires spurious `input_released()` edges for every held
  button.

### Core platform

- **[verified] `lib/mem.c:18-33` — `memset32_neon` is a do-while**: writes
  64 bytes even for `bytes == 0`, and overruns by up to 63 bytes for any
  size not a multiple of 64. All in-tree callers pass multiples of 64, but
  it's a public API with a `bytes` parameter. Same family:
  `memcpy_neon` (mem.c:59-64) silently drops the last `bytes & 3` bytes.
  Either handle remainders or rename/document the contract
  (`_64aligned`).
- **`lib/video.c:226-244` — `video_wait_vblank` treats TCON0_GINT0 as
  write-1-to-clear, but Allwinner TCON flags are write-**0**-to-clear**
  (Linux `sun4i_tcon` writes `status & ~bit`). The function works by
  accident (the first RMW clears the bit unintentionally) and the "clear"
  write at line 237 zeroes every *other* pending flag (line-trigger etc.).
- **`lib/video.c:160-169` — cold-boot `video_init` never clears
  `OVL_ADDR`/`OVL1`, but `de2_init` enables UI0 scanning it** → random
  DRAM composited over the game layer until the app clears the overlay
  (every example does it by hand — see proposal #2 below).
- **`scripts/start.S:38-48` — `_undef_handler` saves `{r0-r4, lr}` but then
  calls C code, which may clobber r12/ip**; since this handler *resumes*
  the interrupted code, a live ip is corrupted.
- **`scripts/start.S:221-247` — IRQ frame is 292 bytes at `bl irq_handler`**
  → C handlers run with SP ≡ 4 (mod 8), violating AAPCS 8-byte alignment.
  Works today by luck; save one more register.
- **`lib/hstimer.c:88-128` — pending bit never cleared on
  `hstimer_set_ticks`/`hstimer_stop`** → a stale latched expiry fires the
  *new* callback immediately on re-arm (e.g. raster split at line 0).
- **`lib/mmu.c:125` — TTBR0 IRGN encoding is wrong for WB/WA**: bit 0 alone
  is IRGN=0b10 (write-through ≈ uncached walks on A7); WB/WA needs bit 6.
  Performance-only, but it's the exact penalty the comment claims to avoid.
- **`include/jupiter.h:202-216` — `draw_rect`/`clear_rect` do no clipping
  or sign checks** while every path in sprite.c clips carefully; the
  "easy" helper is the trap the template steers users into.

### Storage

- **`lib/sdmmc.c:513-517` — clock raised to 50 MHz without CMD6
  high-speed switch**; default-speed cards are only specified to 25 MHz
  (which is what the file header and `sdmmc.h:6` still claim). Marginal
  cards → CRC errors on every transfer.
- **`lib/sdmmc.c:528-536` — read paths lack the DATA_FSM_BUSY entry guard
  the write paths have** (write_multi's own comment explains why the guard
  exists), and clear RISR before the FIFO reset — the ordering the comment
  at sdmmc.c:759-762 calls out as wrong.
- **`lib/sdmmc.c:425` — bus soft-reset only de-asserts** (`|=`), never
  pulses; warm re-init doesn't actually reset the controller.
- **`lib/cpakfs.c:457-467` — `cpakfs_create_note` reads past the end of
  short caller strings** (`name[i]` evaluated beyond the NUL for i up to 15).
- **`lib/sram.c:96-110` — probe writes up to 512 KB past the true SRAM_C
  end** (onto the VE bus) before detecting the mismatch.

### MIDI (unverified-on-wire subsystem, but these are software bugs)

- **`lib/midi.c:203-210` — Program Change / Channel Pressure deliver their
  data byte in `d2` with `d1 = 0`**; every conventional handler reads
  `d1` → always program 0.
- **`lib/midi.c:33-42` — ring-overflow drops the *oldest* byte by advancing
  `s_rx_tail` from the ISR**, racing the consumer's own tail update.
  SPSC rings must drop the new byte.
- **`lib/midi.c:191-198` — System Common (0xF1–0xF6) installed as running
  status** (spec: they clear it).

### libc shim

- **`lib/libc_shim.c:326-385` — `%*d` / `%.*s` unhandled AND the width int
  is never consumed from varargs** → every later specifier in that format
  reads the wrong slot (wild pointer for a subsequent `%s`).
- **`lib/cpp_runtime.cpp:21-22` — `operator new` returns nullptr on OOM
  under `-fno-exceptions`**; C++ callers don't null-check by contract.
  Should halt loudly instead of silent null-deref.
- `emit_double` ignores precision and doesn't round (`0.1` → `0.099999`);
  Lua `%.14g` round-tripping is lossy (libc_shim.c:305-325).

### YM3438 hardware driver

- **`lib/ym3438_hw.c:72/92` — `PG_DAT = ctrl` writes the whole port**
  (PB path does proper RMW); anything else on PG5+ gets driven low
  thousands of times/sec.
- **`lib/ym3438_hw.c:246-255` — F-Number scaling skips CH3 special-mode
  registers ($A8–$AA/$AC–$AE)** → those operators play ~73 cents sharp on
  the 8 MHz clock paths. Common in Genesis VGMs.

---

## 2. Documentation rot (quick wins)

- `README.md:154` + `LIMITATIONS.md:53` reference `examples/cedar_decode_test`
  — that rename was reverted back to `cedar_jpeg` (commit a0e4435). **[verified]**
- `lib/mmu.c:5-13` header still describes "framebuffers uncached, no flush
  required"; the code maps them cached and requires `dcache_clean_fb`.
- `include/cpak.h:8-12` self-contradicts ("128 blocks… 1024 blocks");
  `include/cpakfs.h:6-13` layout comment contradicts the implementation
  (pages vs blocks, ~31.5 KB not ~3.94 KB).
- `include/input.h:75` pin-conflict note is stale twice over (PG vs actual
  PF/PE; NES/SNES *do* conflict with Genesis per input.h:27).
- `lib/sdmmc.c:384-388` CSD "shifted left 8 bits" comment describes the
  opposite of what the (correct) code does; `sdmmc.c:456` names the wrong
  voltage-window constant. `include/sdmmc.h:6` says 25 MHz, code sets 50.
- `include/jupiter.h:226-227` documents `cedar_h264_encode`'s
  `nal_header` params that cedar_enc.c:241 ignores (`unused_hdr`).
- `uart_puthex` prints `0x` itself; `video_diag` call sites also append
  `=0x` → `PLL_VIDEO=0x0x91004107` (lib/video.c:103-115).
- `include/hstimer.h:41` labels the repeating helper "One-shot".

---

## 3. Cleanup (no behavior change)

1. **Delete the 235-line `#if 0` legacy encoder block** in `lib/cedar.c:291+`
   and the dead `BUF_ENC_*`/ISP defines — the live file is ~300 lines.
2. **Centralize the DRAM map** (see §1) — this one change would have
   prevented the three worst bugs in this review.
3. **`lib/audio.c`**: fold the seven `audio_dbg_*` counters behind
   `#ifdef AUDIO_DEBUG`; delete or gate the legacy `audio_update()` polling
   path (running both paths double-refills); centralize the thrice-repeated
   `f * (0xFFFFFFFF / 48000)` phase-step (hardcoded rate ignores
   `audio_set_rate`).
4. **`lib/cpak.c`**: `cpak_data_crc` and `cpak_data_crc_33` are
   byte-for-byte identical — the fallback compare at cpak.c:308 is dead;
   the double `cpak_inter_cmd_gap()` per block halves cpakfs throughput.
5. **`lib/sdmmc.c`**: four transfer functions share ~80 % of their bodies —
   one parameterized `sdmmc_xfer()` would land the FSM-guard fix everywhere.
6. **`lib/snes.c`**: `render_ovl1_ofs_rect` vs `snes_mode6_render` body are
   near-identical; the `#define TRY/#undef TRY` per-pixel macros should be
   a static inline.
7. **Unit consistency**: `memset32(…, count_words)` vs
   `memset32_neon(…, bytes)` — same-looking third parameter, 4× different
   meaning (jupiter.h:25-26).
8. **`lib/si5351.c:186-192`**: dead `cfg1` read/mask of the wrong register;
   drive-strength comment says 8 mA, code sets 4 mA.

---

## 4. Proposed SDK additions (from what the examples achieved)

Ranked by duplication × lines saved. Total across these: **~4,000+ lines of
example code collapses into ~10 small lib files.**

1. **`lib/text.c` — 5×7 font + text renderer.** Copied into **13 examples**
   (~90–130 lines each, ~1,400 total); the copies literally cite each other
   ("Same glyph set as the other editors"). Use `cpak_browser`'s table as
   canonical — it's the only one with lowercase glyphs.
   `text_draw()`, `text_draw_bg()`, `text_width()`, plus a *clipped*
   `fill_rect()` (every example re-wrote clipping that jupiter.h's
   `draw_rect` lacks).

2. **Frame-loop helpers in `lib/video.c`.** 29 examples clear all five
   buffers by hand; 22 hand-roll the flush+swap+exchange double-buffer
   dance; 40 busy-wait `16667 µs`; 21 print the same UART fps heartbeat.
   `video_clear_layers(argb)`, `video_flip(video_fbpair_t*)`,
   `frame_pace_us()`. This also fixes the cold-boot garbage-overlay bug
   (§1) and institutionalizes the overlay-tearing fix documented in
   `parallax/main.c:72` and `menu/main.c:388`.

3. **`lib/smf.c` — Standard MIDI File player.** Complete multi-track SMF
   parser (VLQ, running status, tempo meta, SysEx reassembly, scheduler)
   duplicated in **4 examples** (~900 lines); copies differ only in the
   event sink. `smf_init/advance/rewind/done` with a sink callback serves
   mt32emu, SC-55, and `midi_send` alike.

4. **Audio ring API.** **19 examples** declare
   `extern int16_t mix_buf[]; extern volatile uint32_t mix_wr, mix_rd;`
   and poke lib-private state; `sdmmc_music` copies `MIX_BUF_SIZE` with a
   "matches lib/audio.c" comment (a desync waiting to happen).
   `audio_ring_free/depth/push/push_silence`. Also: **`audio_quickstart()`
   exists precisely to capture this boilerplate but zero examples call
   it** — migrating war1/sdmmc_music/cedar_video_av is free.

5. **`lib/vgm_hw.c` — VGM → hardware-YM3438 engine.** The VGM command
   interpreter driving `ym3438_hw_vgm_write` (waits, loop, mute mask, PSG
   hook) is copied in **5 opn2_hw examples** (~650 lines).

6. **Fixed-point sin/cos in a neutral header.** 13 examples build private
   parabola LUTs (the `4*j*(128-j)` idiom appears 16×) while
   `snes_sin_lut()/snes_cos_lut()` already exist — hidden in `snes.h` where
   only 2 examples found them. Promote as `fx_sin/fx_cos`.

7. **`uart_printf()`.** Every example prints via 5–10-call
   `uart_puts/putdec/puthex` chains; three editors carry `int_to_str`;
   `uart_putdec` can't print negatives. A minimal `%s %c %d %u %x`
   formatter collapses hundreds of lines. Move fs_test's 23-line
   `FRESULT`→string table into the FatFs glue as `fr_str()`.

8. **`fs_mount()` one-liner.** FatFs is integrated but has no SDK entry
   point; fs_test hand-sequences `sdmmc_init` + `f_mount` + partition
   config. Only 1 of 60 examples uses the filesystem — sdmmc_music and
   wc1_save fell back to raw-LBA pack formats partly because mounting is
   friction.

9. **`audio_autopump_start(frames_per_tick)`.** The hstimer-driven audio
   pump that "solved music-streaming jitter" (cedar_video_av's words) is
   copied in 3 examples; it's the pattern the ISR top-up in §1 tries and
   fails to be.

10. **Small utilities:** `sprite_blit_opaque()` + `sprite_composite()`
    (5 examples carry identical local copies); `crc32()` (2 copies);
    `cpu_clock_hz()` (raw PLL_CPU decode repeated in 4 places);
    `video_vi1_label()` (VI1 text banner duplicated in 2);
    a `lib/vbin.c` for the .vbin container parsed identically in both
    A/V examples.

Also worth reconciling: **`cpak_browser` reimplements cpakfs locally**
(index/note-table walk, delete, format — examples/cpak_browser/main.c:153-236)
instead of calling `lib/cpakfs.c`, and the two disagree on delete semantics;
one of them is wrong.

---

## 5. Suggested priority

1. Memory-map unification + encoder-buffer relocation (prevents the
   stack-DMA and CODE-collision corruptions).
2. Audio ISR guard + reentrancy fix (this is the "underrun on long click
   handlers" bug LIMITATIONS.md thinks is fixed — the guard makes the fix
   inert).
3. PPU bounds fixes (nes/snes/gb) + SNES `tidx` widening — cheap, mechanical.
4. Genesis 6-button phase fix; SD 25 MHz (or implement CMD6).
5. Doc-rot pass (§2) — an hour of edits, big trust payoff.
6. Additions §4 in rank order; #1/#2/#4 unlock deleting ~2,500 example
   lines immediately.
