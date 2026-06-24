# Jupiter SDK — Progress Log

## Current State (March 2026)

### PROVEN WORKING ✅

1. **UART output** — 16550 at 0x01C28000, U-Boot leaves it at 115200 8N1.
   `uart_putc/puts/puthex/putdec` all working.

2. **Timer** — Sunxi hardware timer 0 at 0x01C20C00, 24 MHz down-counter.
   ARM generic timer (CNTPCT coprocessor) causes undefined instruction
   trap when launched via U-Boot `go` command — use sunxi timer instead.

3. **I-cache + NEON** — I-cache enabled in start.S gave 22x speedup on
   scalar FB fill (8→179 MB/s). NEON vstm 64-byte-per-iteration fill:
   **1228 MB/s, 425µs for full FB clear**.

4. **Double Buffer** — Swapping VI_TOP_LADDR0(0) between FB0_ADDR/FB1_ADDR
   + MIX_GLB_DBUF commit. Smooth red/blue alternation confirmed.

5. **Display (VI layer)** — 480×272 at ~9MHz pixel clock (198MHz PLL / div 22).
   8 color bars confirmed on panel.

6. **DE2 UI overlay layer** — UI channel is at mixer+0x4000 (channel 2,
   NOT mixer+0x3000 which is VI1). Per-pixel ARGB alpha blending works.
   Scanlines over color bars confirmed.

7. **Bouncing sprite** — 32x32 white box on UI0 overlay, bouncing off
   all 4 edges at 60fps. Frame time: 46µs per frame. Timer-paced to 16.6ms.

8. **MMU + D-cache** — ARMv7-A short descriptor format, 1MB sections.
   Identity-mapped. Code/data/stack/framebuffers all cached WB/WA.
   Explicit `dcache_clean_fb()` flush before swap for DE2 coherency.
   Verified against NuttX armv7-a/mmu.h, ARM AN4838, Cortex-A7 TRM §5.2.1.
   TEX/C/B encodings: Device=000/0/1, Cached=001/1/1, Uncached=001/0/0.

9. **Fast tile renderer** — LUT color lookup (replaces function pointer +
   switch), AND mask (replaces non-power-of-2 UDIV), 8-pixel tile runs.
   **7.8ms full parallax frame at 60fps** (down from 66ms baseline — 8x speedup).

10. **Mode 7 affine floor** — Per-scanline rotation/scale with vortex twist.
    SNES-resolution rendering (240×68 computed, pixel-doubled to 480×136).
    Sky gradient via NEON row fill. **15.9ms total at locked 60fps.**

### KEY DISCOVERIES

- **U-Boot uses VI channel (OVL_V at mixer+0x2000), NOT UI channel.**
  VI framebuffer address register is at offset 0x18 (TOP_LADDR0), not
  0x10 like UI.

- **V3s has 2 VI channels + 1 UI channel** (from kernel: vi_num=2, ui_num=1).
  Channel map:
  - VI0 = channel 0, mixer+0x2000 (background)
  - VI1 = channel 1, mixer+0x3000 (available for PIP/minimap)
  - UI0 = channel 2, mixer+0x4000 (HUD/overlay with per-pixel alpha)

- **V3s DE2 has NO hardware scaler (VSU).** Registers at mixer+0x20000
  are mapped and read/write, but the scaler datapath is not present.
  Linux kernel sets scaler_mask=0 for V3s — confirmed correct.
  Probe test: VSU_CTRL, VSU_OUTSIZE, coefficient RAM all read back
  what we write, but enabling the scaler produces no visual output.
  The register file comes free with the address decoder; the actual
  scaler logic was cut from the V3s silicon.

- **The "480×272" panel accepts 800×480 signaling from U-Boot** but only
  physically displays ~480 pixels wide. U-Boot doesn't know or care.

- **Panel powers up in RGB mode — no SPI init required.** Confirmed by
  the fact that U-Boot's sunxi LCD driver (which does no SPI init) works.

- **VI channels don't support per-pixel alpha transparency** through the
  blender. VI1 as a transparent sprite layer over VI0 shows opaque black
  for zero-alpha pixels. The blender's alpha path works on global alpha
  per-channel, not per-pixel from VI framebuffer data. UI channel DOES
  support per-pixel alpha (ARGB8888 with alpha mode = pixel alpha).

- **VI1 is useful for minimap/PIP** — a small opaque rectangle positioned
  at a specific coordinate, composited geometrically (not alpha-blended).

- **D-cache: the bottleneck was compute, not memory.** Enabling D-cache
  with framebuffers uncached (Option A) gave only 68ms→61ms on tile
  rendering. Caching framebuffers too (Option B) gave identical 61ms.
  **NOTE: This analysis was performed before the ACTLR.SMP fix. The
  D-cache was not actually functioning. The 7ms savings came from
  I-cache or branch predictor effects, not D-cache. All "D-cache"
  measurements from before the SMP fix are invalid. See BENCHMARK
  RESULTS section below for corrected numbers.**

- **Per-pixel renderer bottleneck is ~1100 cycles/pixel uncached.**
  Mode 7 at 480×272 native = 72ms (65K pixels × 1100 cycles). This is
  dominated by strided tile map reads thrashing L1 (32KB cache can't
  hold 4KB map + 260KB framebuffer simultaneously).

- **SNES-resolution pixel doubling is the correct optimization** for
  per-pixel affine renderers. 240×68 → 480×136 doubled gives 4x fewer
  lookups and looks authentic. 72ms → 15.6ms.

### U-BOOT REGISTER DUMP (Ground Truth)

These are the exact register values from the working U-Boot 2017.01-rc2
LCD driver. Our code matches these for all clock/polarity settings.

```
PLL_VIDEO  (0x01c20010) = 0x91004107  → ~198MHz (24/8*66)
BUS_GATE1  (0x01c20064) = 0x00001011  → DE(bit12) + TCON(bit4) enabled
DE_CLK     (0x01c20104) = 0x82000003  → src=2, div=4
TCON_CLK   (0x01c20118) = 0x80000000  → src=PLL_VIDEO, div=1
BUS_RST1   (0x01c202c4) = 0x00001011  → DE + TCON de-asserted
TCON_GCTL  (0x01c0c000) = 0x80000000  → enabled
TCON0_CTL  (0x01c0c040) = 0x800001E0  → enabled, clk_delay=30
TCON0_DCLK (0x01c0c044) = 0xF0000006  → gate+all outputs, div=6
TCON0_BASIC0(0x01c0c048)= 0x031F01DF  → 800×480
IO_POL     (0x01c0c088) = 0x10000000  → DCLK phase=1
DE2 CCU    (0x01000000) = 1,1,1,0     → gate/bus/reset/div
MIXER_GLB  (0x01100000) = 1,0x10,0,0x01DF031F
VI0_ATTR   (0x01102000) = 0x00008401  → enabled, XRGB8888
VI0_PITCH  (0x0110200C) = 0x00000C80  → 3200 bytes
VI0_ADDR   (0x01102018) = 0x03E89000  → U-Boot's FB address
BLD_ROUTE  (0x01101080) = 0x00000000  → channel 0 (VI) → pipe 0
BLD_PIPECTL(0x01101000) = 0x00000101  → pipe 0 enabled + fill color
```

### PERFORMANCE NUMBERS

**⚠ Pre-SMP-fix measurements (D-cache was NOT functioning):**

| Operation                          | Time       | Notes                      |
|------------------------------------|------------|----------------------------|
| FB fill (scalar)                   | 2,902 µs   | 179 MB/s                   |
| FB fill (NEON)                     | 425 µs     | 1,228 MB/s                 |
| DE2 init (48KB zero)               | 1,238 µs   | —                          |
| Sprite frame (bounce)              | 46 µs      | —                          |
| D-cache flush (522KB FB)           | 57 µs      | DCCMVAC per cache line     |
| Sky gradient (NEON row fill)       | 222 µs     | 136 rows                   |
| Parallax tiles (old, fn ptr)       | 61,000 µs  | ~560 cycles/pixel          |
| Parallax tiles (fast, LUT+AND)     | 7,800 µs   | ~72 cycles/pixel, 8× faster|
| Mode 7 floor (native 480×272)      | 72,200 µs  | ~1100 cycles/pixel         |
| Mode 7 floor (240×68 doubled)      | 15,570 µs  | 4× fewer lookups           |
| Mode 7 total (sky+floor+sprite)    | 15,860 µs  | Locked 60fps               |
| Sprite blit (25 sprites)           | 940 µs     | ~38µs per sprite avg       |
| Tiles + 25 sprites                 | 8,830 µs   | Locked 60fps               |
| Mode 7 + 25 sprites (tear-free)   | 12,960 µs  | Both layers double-buffered|
| Jupiter logo (full-res sphere)    | 55,000 µs  | ~18fps, flicker-free       |
| Frame budget @60fps                | 16,667 µs  | —                          |
| Frame budget @30fps                | 33,333 µs  | —                          |

**✅ Post-SMP-fix measurements (D-cache working, from benchmark v2):**

| Operation                          | Time       | Notes                      |
|------------------------------------|------------|----------------------------|
| NEON flat fill 480×272             | 451 µs     | 1,157 MB/s                 |
| D-cache flush (522KB FB)           | 57 µs      | unchanged                  |
| Scatter r+w 480×272 (full ground)  | 2,593 µs   | 23 cyc/px (was 573)        |
| iso_scanline 240×136 doubled       | 1,092 µs   | 15× faster than pre-fix    |
| iso_scanline ×136 calls            | 523 µs     | 29× faster                 |
| 25× sprite_blit 32×32             | 230 µs     | 26× faster                 |
| L1 hit latency                     | 9 cyc      | was 272 cyc (DRAM)         |
| Sequential read 32KB               | 1,340 MB/s | was 18 MB/s (74× faster)   |

### ARCHITECTURE FOR GAMES

```
VI0 (ch0): Background — tiles, scrolling world, Mode 7 floor
           NO hardware scaler (V3s silicon cut)
           XRGB8888, double-buffered (FB0/FB1 swap)

VI1 (ch1): Minimap/PIP — small opaque window, positioned via COOR
           Same register layout as VI0
           No per-pixel alpha, but geometric transparency

UI0 (ch2): HUD/Overlay — sprites, health bars, scanlines, fade effects
           ARGB8888 with per-pixel alpha
           At mixer+0x4000 (NOT mixer+0x3000)
```

### OPTIMIZATION LESSONS LEARNED

1. **Function pointer per pixel is lethal on in-order A7.** ~50-100 cycle
   pipeline flush per indirect call. Replace with LUT indexed load.

2. **Non-power-of-2 modulo is expensive.** UDIV + MLS = ~20-40 cycles.
   Use power-of-2 map widths and AND mask instead.

3. **Tile-run amortization is the biggest win for scrolling renderers.**
   One tile lookup for 8 identical pixels = 8x fewer loads. Goes from
   66ms to 7.8ms on parallax.

4. **Per-pixel affine (Mode 7) can't amortize — reduce pixel count instead.**
   SNES-resolution doubling (240×68 → 480×136) gives 4x speedup with
   authentic aesthetic. This is not a compromise; it's the correct look.

5. **D-cache helps reads more than writes.** Caching the framebuffer
   (write target) gave zero measurable improvement on tile rendering
   because the bottleneck was CPU-side compute, not store latency.
   But the flush cost is only 57µs, so cache everything and flush.

6. **NEON fills crush scalar loops.** Sky gradient went from ~35ms
   (per-pixel store) to 222µs (memset32_neon per row) — 157x.

7. **volatile is the enemy of cached rendering.** Remove volatile from
   framebuffer pointers when FB is cached and you flush before swap.
   Lets GCC keep values in registers and batch stores.

### NEXT STEPS (Priority Order)

1. **Input (GPIO buttons)** — Poll GPIO pins for game input. D-pad + 2
   buttons. Debounce in software. This unblocks everything — every demo
   becomes a playable prototype the moment input exists.

2. **Sprite blitting** — NEON-accelerated sprite blit with transparency
   on UI0. Read ARGB source, skip zero-alpha, write to overlay. Combined
   with input, this is enough for a walking character on a scrolling world.

3. **Audio** — DMA-fed ring buffer to V3s integrated codec. Even a square
   wave on button press transforms the experience from "display test" to
   "console."

4. **VSync interrupt** — TCON0 vblank IRQ for tear-free swap timing.
   Current timer pacing works but isn't synced to actual beam position.

5. **VI1 minimap test** — Small opaque window in corner, separate FB.

6. **Double-buffered UI0** — Eliminate sprite tearing.

### SDK TRAIL

Each example proves one capability and leaves SDK code behind it:

| Example           | Proves                        | SDK contribution         |
|-------------------|-------------------------------|--------------------------|
| `colorbars`       | Display init, VI0             | `lib/video.c`           |
| `bouncing_sprite` | UI0 alpha overlay, 60fps      | `draw_rect`, `clear_rect`|
| `parallax`        | Tiles, scrolling, double buf  | `lib/tiles.c` (original)|
| `mmu_dcache`      | MMU + D-cache, FB coherency   | `lib/mmu.c`, `dcache_clean_fb` |
| `fast_tiles`      | LUT renderer, 8x speedup     | `tiles_render_fast()`   |
| `scaler_probe`    | VSU registers live, no scaler | Confirmed V3s has no VSU |
| `mode7_vortex`    | Affine floor, vortex, 60fps   | Sin/cos LUT, Mode 7 pattern |
| `sprites`         | Alpha-key blit, clip, flip    | `lib/sprite.c`          |
| `mode7_sprites`   | Full pipeline, tear-free      | Double-buffered UI0 overlay |
| `jupiter_logo`    | Procedural sphere, lighting   | No-clear trick for flicker-free |

### FILES IN THIS SDK

```
jupiter-sdk/
├── Makefile            — make / make GAME=examples/xxx/main.c
├── README.md
├── docs/
│   ├── PROGRESS.md                        — this file
│   ├── JUPITER_HARDWARE_REFERENCE.md
│   ├── GAME_DEV_TRICK_BIBLE.md
│   └── UBOOT_SPL_GUIDE.md
├── include/
│   ├── jupiter.h       — single include, full API
│   └── v3s.h           — raw hardware registers
├── lib/
│   ├── uart.c          — serial debug
│   ├── timer.c         — 24 MHz hardware timer
│   ├── mem.c           — NEON and scalar fills
│   ├── video.c         — display init, buffer swap
│   ├── tiles.c         — tile renderers (original + fast)
│   ├── mmu.c           — MMU, D-cache, dcache_clean_range
│   └── sprite.c        — alpha-key sprite blit, flip, clear
├── scripts/
│   ├── start.S         — ARM startup (I-cache, NEON, BSS)
│   ├── jupiter.ld      — linker script
│   └── boot.cmd        — U-Boot boot script
├── template/
│   └── game.c          — your starting point
├── examples/
│   ├── colorbars/
│   ├── bouncing_sprite/
│   ├── parallax/
│   ├── mmu_dcache/
│   ├── fast_tiles/
│   ├── scaler_probe/
│   ├── mode7_vortex/
│   ├── sprites/
│   ├── mode7_sprites/
│   └── jupiter_logo/
└── tools/              — asset pipeline (future)
```

### CRITICAL LESSONS LEARNED

1. Always match U-Boot's exact register values first, then change ONE
   variable at a time.
2. V3s DE2 uses VI channel (not UI) as primary — U-Boot confirmed this.
3. Mixer registers MUST be zeroed on cold boot (48KB at mixer base).
4. I-cache alone gives 22× speedup even without D-cache/MMU.
   (Note: the D-cache was NOT working at the time of this test due
   to the ACTLR.SMP bug. The 22× was purely from I-cache.)
5. ARM generic timer coprocessor traps when launched via U-Boot `go` —
   use sunxi hardware timer instead.
6. The UI channel is at mixer+0x4000 (channel 2), NOT mixer+0x3000
   (that's VI1). This was the root cause of all UI layer failures.
7. VI channels are opaque to the blender — no per-pixel alpha. Use UI
   for anything that needs transparency.
8. V3s has no VSU hardware scaler. Registers are mapped but datapath
   is absent. Linux scaler_mask=0 is correct. Software scaling only.
9. Profile before optimizing. (Note: the original claim "D-cache didn't
   help tile rendering" was wrong — D-cache wasn't functioning. With
   D-cache working via SMP fix, memory-bound ops are 15-50× faster.)
10. The SNES got Mode 7 right: low-res computation + pixel doubling is
    both faster AND more aesthetically correct than native-res affine.
11. Never clear what you're about to fully overwrite. Clearing a region
    then redrawing it creates a window where the clear is visible on
    screen during a mid-scan swap — this causes flicker/tearing even
    with double buffering. If every pixel in a region gets overwritten
    every frame (e.g. a fixed-position lit sphere), skip the clear.
12. Clear SCTLR.A in startup. U-Boot may enable alignment checking.
    GCC at -O2 generates unaligned word stores for byte array fills.
13. Cortex-A7 with MMU disabled treats all memory as Device/Strongly-
    Ordered. Device memory always enforces alignment. Call mmu_init()
    before any array initialization.
14. **CRITICAL: Set ACTLR.SMP (bit 6) before enabling D-cache.** On
    Cortex-A7, even single-core, SMP must be set for the D-cache to
    participate in coherency. Without it, SCTLR.C=1 has no effect —
    every load goes to DRAM at ~272 cycles. With SMP=1, L1 hits at
    9 cycles, read bandwidth jumps 74×, and all rendering operations
    speed up 15-50×. This one bit was the root cause of every
    performance issue in the SDK's history.

### BENCHMARK RESULTS (April 2026)

Full hardware characterization via `examples/benchmark/main.c`.
Diagnostic v2 found and fixed the root cause of all performance issues.

#### THE BUG: ACTLR.SMP (bit 6) was clear

On Cortex-A7, the SMP bit in the Auxiliary Control Register controls
whether the core participates in the cache coherency domain. Even on
a single-core SoC like the V3s, SMP must be set to 1 for the L1 D-cache
to function. With SMP=0, SCTLR.C=1 has no effect — every load and store
goes directly to DRAM at ~272 cycles regardless of MMU cache attributes.

**The fix:** One write to ACTLR before enabling D-cache in `mmu_init()`:

```c
uint32_t actlr;
__asm__ volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(actlr));
actlr |= (1 << 6);  /* SMP — cache coherency participation */
__asm__ volatile("mcr p15, 0, %0, c1, c0, 1" :: "r"(actlr));
```

#### CP15 REGISTER STATE

| Register | Value | Key bits |
|----------|-------|----------|
| SCTLR | 0x00C5187D | M=1 A=0 C=1 I=1 Z=1 TRE=0 |
| ACTLR (before fix) | 0x00006000 | **SMP=0** |
| ACTLR (after fix) | 0x00006040 | **SMP=1** |
| CTR | 0x84448003 | IminLine=32B, DminLine=64B |
| CLIDR | 0x0A200023 | L1: separate I+D, L2: unified |
| CCSIDR(L1D) | 0x700FE01A | 64B lines, 4-way, 128 sets = **32KB** |
| CCSIDR(L2) | 0x701FE03A | unified L2 present |

#### BEFORE vs AFTER (SMP=0 → SMP=1)

| Metric | SMP=0 (broken) | SMP=1 (fixed) | Speedup |
|--------|----------------|---------------|---------|
| L1 latency (1KB chase) | 272 cyc | 9 cyc | **30×** |
| Sequential read (32KB) | 18 MB/s | 1,340 MB/s | **74×** |
| Sequential write (32KB) | 541 MB/s | 1,365 MB/s | **2.5×** |
| NEON memset (4KB) | 1,157 MB/s | 3,056 MB/s | **2.6×** |
| NEON memset (512KB) | 1,218 MB/s | 1,218 MB/s | 1× (DRAM-limited) |
| Scatter r+w 480×272 | 62,387 µs | 2,593 µs | **24×** |
| iso_scanline 240×136 dbl | 16,972 µs | 1,092 µs | **15×** |
| 25× sprite_blit 32×32 | 6,107 µs | 230 µs | **26×** |
| iso_scanline ×136 calls | 15,215 µs | 523 µs | **29×** |
| Cache pollution 512KB 2-lookup | 560 cyc/px | 11 cyc/px | **50×** |
| dcache_clean_fb | 57 µs | 57 µs | (same) |

#### MEMORY HIERARCHY (with SMP=1)

Pointer-chase latency by working set size:

| Size | Latency | Interpretation |
|------|---------|----------------|
| 1KB–32KB | 9 cyc | L1 D-cache hit |
| 64KB–128KB | 10 cyc | L2 hit (L1 miss) |
| 256KB–4MB | 10–11 cyc | L2 still catching most accesses |

The L2 is large enough that even 4MB random chases only cost 11 cyc.
True DRAM misses (bypassing both L1 and L2) cost ~272 cyc, but the
L2 prevents most of these from occurring in practice.

Read/write bandwidth (with SMP=1):

| Size | Read | Write | NEON fill |
|------|------|-------|-----------|
| 4KB | 1,338 MB/s | 1,338 MB/s | 3,056 MB/s |
| 32KB | 1,340 MB/s | 1,234 MB/s | 1,432 MB/s |
| 64KB | 1,215 MB/s | 1,200 MB/s | 1,285 MB/s |
| 512KB | 864 MB/s | 1,175 MB/s | 1,218 MB/s |

Small buffers (≤32KB) run at L1 speed. Larger buffers degrade toward
DRAM bandwidth (~1.2 GB/s for writes, ~860 MB/s for reads). NEON
fills at 4KB hit 3 GB/s — the L1 write bandwidth ceiling.

#### CACHE POLLUTION (with SMP=1)

The test that directly models the isometric ground renderer:
2 lookups (1KB map + 40B LUT) per pixel, writing to buffers of
increasing size:

| FB write size | cyc/px |
|---------------|--------|
| 4KB | 11 |
| 32KB | 11 |
| 64KB | 11 |
| 128KB | 11 |
| 256KB | 11 |
| 512KB | 11 |

**11 cycles/pixel at every size.** No cache pollution effect at all.
The L1 (32KB, 4-way) + L2 keep the small lookup tables cached even
while sweeping through a 512KB framebuffer. Write-allocate thrashing
is not a real concern on this hardware with proper cache enabled.

#### ARM↔NEON TRANSFER + FUNCTION CALL

| Operation | Cost |
|-----------|------|
| VDUP.32 (ARM→NEON broadcast) | 2 cyc |
| VMOV.32 (ARM→NEON lane insert) | 1 cyc |
| VMOV.32 (NEON→ARM lane read) | 1 cyc |
| Empty function call | 1 cyc |
| 8-arg function call | 5 cyc |

VMOV lane insert is NOT slow on Cortex-A7. The 59ms regression we
attributed to "ARM→NEON pipeline stalls" was actually DRAM latency
from the absent D-cache. NEON lane operations are single-cycle.

#### INCORRECT ASSUMPTIONS (CORRECTED)

1. **"D-cache gives 22× speedup" (from early mmu_dcache test).**
   That 22× was from I-cache, not D-cache. D-cache was never working.

2. **"Write-allocate thrashing caused 90ms."**
   No cache existed to thrash. 90ms = 130K pixels × 2 DRAM reads/px
   × 272 cyc/read. Pure DRAM latency.

3. **"WT mapping would stop L1 pollution."**
   WT, WB/WA, and Uncached all behaved identically because no D-cache.

4. **"VMOV.32 costs ~20 cycles on A7."**
   Costs 1 cycle. The slowdown was DRAM-bound, not pipeline-bound.

5. **"Function call overhead matters for per-scanline dispatch."**
   5 cycles. 136 calls = 680 cycles total. Irrelevant.

6. **"ASM beats GCC for scatter-load loops."**
   At 573 cyc/px DRAM-bound, instruction scheduling is irrelevant.
   With cache working (11 cyc/px), ASM interleaving helps marginally
   but the gains are small vs properly optimized C.

7. **"The Cortex-A7 L1 is 32KB, enough to avoid pollution."**
   True, but irrelevant when the L1 is not participating. The L2
   also exists and catches nearly all L1 misses (10-11 cyc at 4MB
   working set), making cache pollution a non-issue for our workloads.


