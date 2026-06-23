# Sega Jupiter — Game Developer's Trick Bible
### Every Hardware Trick Available on the Allwinner V3s for Game Rendering

This document catalogs every performance technique available to a bare-metal
game running on the V3s. The goal: 60fps hand-drawn 2D games with the feel of
a dedicated console, on a chip that has no sprite engine, no tile hardware,
and no GPU. Every pixel is placed by the CPU. That's the constraint. That's
the instrument.

---

## 1. DMA — Background Memory Operations

The V3s has a dedicated DMA controller at `0x01C02000` with 8 independent
channels. Each channel can copy memory from source to destination without
CPU intervention.

**What it gives you:**

- Framebuffer clearing: DMA fills the back buffer with a solid color while
  the CPU starts game logic for the next frame.
- Bulk sprite sheet copies: DMA blasts pre-composited tile rows or large
  background regions from asset memory to the framebuffer.
- Audio feeding: a DMA channel continuously streams PCM samples from a ring
  buffer to the audio codec. The CPU never touches the audio output registers
  directly during playback.

**How to use it:**

Set source address, destination address, byte count, transfer width (32-bit
for framebuffer ops), and trigger the channel. Poll the completion flag or
set up a DMA-done interrupt to chain operations. Each channel runs fully
parallel with the CPU — the CPU sets it up and walks away.

**Budget impact:** DMA competes with the CPU for DRAM bandwidth, but on
separate AHB ports. In practice, a framebuffer clear via DMA is 3-5x faster
than a CPU memset because the DMA controller issues optimal burst transactions
without instruction fetch overhead.

---

## 2. NEON SIMD — 4 Pixels at Once

The Cortex-A7 has a 128-bit NEON unit with 32 double-word registers (or 16
quad-word registers). In ARGB8888 mode, each register holds 4 pixels. In
RGB565 mode, each register holds 8 pixels.

**What it gives you:**

- Alpha-blended sprite blitting at 4x scalar speed. A NEON inner loop loads
  4 source pixels, 4 destination pixels, does the blend math (`src*alpha +
  dst*(255-alpha)`) in parallel, and stores 4 result pixels. One iteration
  per clock cycle on the multiply-accumulate pipeline.
- Color keying (transparency by magic pink): compare 4 pixels against the
  key color simultaneously, generate a mask, conditional-store only
  non-transparent pixels.
- Palette lookup for indexed-color sprites: use VTBL (vector table lookup)
  to convert 16 palette indices to 16 ARGB values in a few instructions.
- Matrix math for camera transforms: 4×4 matrix multiply on float32x4
  vectors. Depth-sorting sprites in a 3D camera space (the BoF4 diorama
  technique) requires this.

**Example — alpha blend inner loop (pseudocode):**
```
vld4.u8  {d0-d3}, [src]!      // load 4 ARGB pixels (deinterleaved)
vld4.u8  {d4-d7}, [dst]        // load 4 dest pixels
vmull.u8 q4, d0, d3            // R_src * A_src
vmlsl.u8 q4, d4, d3            // - R_dst * A_src (will add R_dst separately)
// ... complete blend math ...
vst4.u8  {d0-d3}, [dst]!       // store 4 blended pixels
```

**Budget impact:** A NEON sprite blitter can push ~400 million pixels/second
on the V3s. A 400×240 framebuffer is 96,000 pixels. At 60fps that's 5.76M
pixels/frame. You have budget for ~70 full-screen overdraw passes per frame.
In practice, 10-20 sprite layers with alpha is trivial.

---

## 3. DE2 Hardware Layers — Free Compositing

The V3s DE2 mixer has 1 VI (video input) channel and 1 UI (user interface)
channel. The blender composites them every scanline in hardware at zero CPU
cost.

**The smart play for games:**

- **VI channel (layer 0):** Your game world. Background tiles + sprites,
  software-composited into a single framebuffer. This is what the CPU draws.
- **UI channel (layer 1):** A transparent overlay. HUD elements, health bars,
  score display, dialog boxes. Or a full-screen effect layer.

Once configured, the blender runs forever without CPU intervention. You only
touch the UI layer's framebuffer when the HUD changes (which might be once
every few seconds in gameplay).

**Advanced layer tricks:**

- **Scanline overlay:** Fill the UI layer with a repeating pattern of
  alternating transparent and 30%-black horizontal lines. The blender
  composites this over the game layer every frame, producing CRT scanlines
  for free. Configure once, forget forever.
- **CRT phosphor mask:** Same technique but with a 3×1 repeating RGB subpixel
  pattern at low alpha. Simulates a shadow mask / aperture grille.
- **Vignette:** Pre-render a radial gradient from transparent (center) to
  black (edges) into the UI framebuffer. Free analog-TV-style vignetting.
- **Screen flash:** Change the UI layer's global alpha from 0 to 255 and back
  in the vblank ISR. Instant full-screen white flash on enemy hit, costs
  one register write.
- **Fade to black:** Animate the UI layer's global alpha over multiple frames.
  The UI framebuffer is solid black. Alpha 0 = normal gameplay, alpha 255 =
  blackout. Smooth fade costs one register write per frame.

---

## 4. DE2 Hardware Scaler — The Retro Resolution Trick

The VI channel has a built-in hardware scaler that upscales a framebuffer to
the panel's native resolution.

**How to use it:**

Render your game at a low "pixel art native" resolution — 400×240, 320×180,
or even 256×224 (SNES resolution). Configure the VI overlay size to match
your render resolution, but set the BLD output size and TCON to 800×480.
The scaler upscales in hardware.

**Optimal resolutions for 800×480:**

| Render Res  | Scale Factor | Pixel Clock Savings | Notes                    |
|-------------|-------------|---------------------|--------------------------|
| 400×240     | 2×2         | 75% bandwidth saved | Perfect integer scale    |
| 320×240     | 2.5×2       | 80% saved           | Slight H stretch         |
| 266×160     | 3×3         | 89% saved           | Chunky, very retro       |
| 200×120     | 4×4         | 94% saved           | Extremely low-res        |

**Why 400×240 is the sweet spot:**

- Exactly 2x integer scale in both axes. Bilinear and nearest-neighbor
  produce identical results at integer scales, so no filtering artifacts.
- Framebuffer = 400×240×4 = 384KB (vs 1.5MB at native). Fits comfortably
  in cache working sets.
- Sprite blitting is 4x fewer pixels. A 32×32 sprite blit at 400×240 costs
  25% of what it costs at 800×480.
- The "pixel art" aesthetic is a feature, not a limitation. Every pixel is
  an intentional artistic choice at this density.

**The catch:** The scaler's filter mode may introduce slight blur on
non-integer scales. Stick to integer multiples for pixel-perfect rendering.

---

## 5. Double Buffering with VSync

The TCON0 generates a vertical blanking interrupt via GINT0 (bit 15). This
fires at the top of each frame — 60-68 Hz for the AT070TN83 panel.

**Frame cycle:**

```
Frame N:
  1. Vblank IRQ fires
  2. ISR swaps DE2 UI_ADDR to the finished back buffer (FB1)
  3. ISR commits: MIX_GLB_DBUF = 1
  4. DE2 now displays FB1, FB0 is free
  5. CPU begins drawing Frame N+1 into FB0
  6. CPU finishes drawing, calls WFI (wait for interrupt)
  7. Goto step 1 (now displaying FB0, drawing into FB1)
```

The swap is two register writes — UI_ADDR(0) and MIX_GLB_DBUF. Takes ~20
nanoseconds. Completely tear-free.

**30fps lock:** If your frame consistently takes >16ms but <33ms, swap every
other vblank. The ISR increments a counter and only swaps on even frames.
Rock-solid 30fps is better than stuttery 40-60fps.

**Triple buffering:** Allocate FB0, FB1, FB2. The CPU always draws into the
next free buffer. The display always shows the most recently completed buffer.
Decouples render time from display time — smoother frame pacing at the cost
of one frame of input latency and 1.5MB more DRAM.

---

## 6. Tile-Based Background Rendering

No hardware tile engine — but the CPU is 1000x faster than the SNES PPU
that defined the golden age of tile-based games.

**Data structures:**

```c
// Tile map: 2D array of tile indices (25×15 tiles for 400×240 at 16×16)
uint8_t tilemap[MAP_H][MAP_W];

// Tile atlas: all tiles packed into one bitmap in DRAM
// 256 tiles × 16×16 × 4 bytes = 256KB
uint32_t tile_atlas[256][16][16];
```

**Rendering:**

A NEON-optimized inner loop walks the visible region of the tile map and
copies 16×16 blocks from the atlas to the framebuffer. For a 400×240 screen
with 16×16 tiles, that's 375 tile blits per frame. At ~200 cycles per tile
blit (NEON), that's 75,000 cycles — 0.006% of the CPU's frame budget.

**Scrolling optimization:** When the camera scrolls, only the newly-exposed
row or column of tiles needs blitting. The rest of the framebuffer is already
correct from the previous frame. A horizontal scroll at 1 pixel/frame means
redrawing 1 column of 15 tiles instead of all 375. That's a 96% reduction.

**Multiple parallax layers:** Render 2-3 tile layers at different scroll
speeds into the VI framebuffer (back to front, with alpha or color-key
transparency). The DE2 UI layer handles the HUD on top. Three parallax
backgrounds + sprites + HUD, all at 60fps, all in software.

---

## 7. Sprite Rendering Techniques

**Opaque sprites (fastest):** Straight NEON memcpy from sprite atlas to
framebuffer. No blending, no branching. ~4 cycles per 4 pixels.

**Color-keyed sprites:** Compare each pixel against a magic color (e.g.,
0xFFFF00FF magenta). NEON VCEQ generates a mask; use BIF (bitwise insert
if false) to conditionally keep the destination pixel. ~8 cycles per 4 pixels.

**Alpha-blended sprites:** Full per-pixel alpha blend using NEON
multiply-accumulate. ~12-16 cycles per 4 pixels. Use this for
semi-transparent effects, smoke, shadows.

**Pre-multiplied alpha:** Store sprites with RGB channels pre-multiplied by
alpha. The blend equation simplifies to `result = src + dst * (1 - src_alpha)`,
saving one multiply per channel per pixel. NEON loves this.

**Sprite batching:** Sort sprites by atlas page. Blit all sprites from the
same atlas in sequence so the source data stays hot in L1 cache. Random
atlas access thrashes the cache and kills performance.

**Sprite sheets with animation:** Pack all animation frames for a character
into a contiguous strip. Advancing a frame is just changing the source
pointer offset — no data rearrangement.

---

## 8. Dirty Rectangle Tracking

Don't redraw the whole framebuffer every frame.

**How it works:**

Maintain a list of rectangular regions that changed since last frame:
- Sprite moved → old position is dirty, new position is dirty
- Tile changed → that tile's screen rect is dirty
- HUD element updated → that region is dirty

Each frame, merge overlapping dirty rects, then only clear and redraw those
regions. On a typical frame where 10-20% of the screen changes, this saves
80-90% of blitting work.

**When NOT to use it:** If the camera is scrolling every frame, the entire
screen is dirty and you should just redraw everything. Dirty rects shine
in static-camera scenes (turn-based RPGs, puzzle games, dialog sequences).

---

## 9. Cache Management

Our startup code currently disables L1 caches. For game performance, you
want them enabled.

**Enabling caches:**

```c
// Enable I-cache and D-cache in SCTLR
mrc p15, 0, r0, c1, c0, 0
orr r0, r0, #(1 << 12)    // I-cache
orr r0, r0, #(1 << 2)     // D-cache
mcr p15, 0, r0, c1, c0, 0
```

**The DE2 coherency problem:** The DE2 reads framebuffer data directly from
DRAM via its own bus master port. It does not snoop the CPU's L1 cache. If
you write pixels to the framebuffer and they're sitting in L1 cache but
haven't been flushed to DRAM, the DE2 will read stale data.

**Solutions:**

1. **Write-through region:** Configure the MMU to make the framebuffer
   address range write-through (not write-back). Every CPU store immediately
   propagates to DRAM. Slight performance cost vs write-back, but no
   coherency bugs.

2. **Explicit cache flush:** After finishing a frame, call a
   `dcache_clean_range(fb_start, fb_end)` routine that walks the cache
   and writes back dirty lines. Then signal the buffer swap.

3. **Uncached framebuffer, cached everything else:** Map the framebuffer
   region as uncached/unbuffered, but enable caches for game logic, asset
   data, and stack. The framebuffer writes go straight to DRAM (slower),
   but game logic runs at full cached speed. This is the simplest approach
   and may be fast enough at 400×240.

**Recommendation:** Start with option 3 (uncached FB). If profiling shows
framebuffer writes as the bottleneck, move to option 1 or 2.

---

## 10. Audio — DMA-Fed Ring Buffer

The V3s integrated audio codec supports mono/stereo PCM output at up to
48 KHz through an I2S/PCM interface with DMA support.

**Architecture:**

```
Game Logic → Software Mixer → Ring Buffer (DRAM) → DMA → Audio Codec → Speaker
```

The DMA channel continuously reads from the ring buffer and feeds the codec.
The CPU's only job is mixing sound effects and music into the ring buffer
ahead of the DMA read pointer.

**Software mixer:**

A simple 8-channel mixer: each channel has a source pointer, length, position,
volume, and loop flag. Every audio tick (called from a timer IRQ or during
vblank), the mixer walks all active channels, multiplies samples by volume,
sums them, clamps, and writes to the ring buffer.

At 22050 Hz mono, 8-bit samples, mixing 8 channels costs:
22050 samples × 8 channels × ~4 ops = ~700K ops/frame at 60fps.
That's <0.1% of the CPU budget. Negligible.

**Music:** For tracked music (MOD/XM/S3M), a lightweight replayer library
runs on the CPU and feeds the mixer. For streaming audio (e.g., CD-quality
background music from SD card), a DMA channel reads from SD while the CPU
decodes (ADPCM or even MP3 via a software decoder — the A7 has the horsepower).

---

## 11. Timer-Based Frame Pacing

The V3s has multiple hardware timers. Use one to measure frame time precisely.

**Frame budget at 60fps:** 16.67ms = 20,000,000 CPU cycles at 1.2 GHz.

**Technique:**

```
frame_start = timer_read();
// ... game logic + rendering ...
frame_end = timer_read();
frame_time = frame_end - frame_start;
// frame_time tells you exactly how much headroom you have
```

If you finish in 8ms, you have 8ms of budget left for more enemies, more
particles, more visual polish. If you're over budget, you know exactly
which subsystem to optimize.

**WFI for power:** After rendering, execute `wfi` (wait for interrupt).
The CPU halts until the next vblank IRQ, consuming near-zero power. This
matters for a handheld/portable console — every milliwatt counts.

---

## 12. Asset Pipeline — SD Card to DRAM

**Boot-time loading:** At startup, read sprite atlases, tile sets, music,
and sound effects from the SD card into DRAM. The V3s SD controller supports
4-bit mode at 50 MHz — theoretical 25 MB/s, practical ~10-15 MB/s.

A complete game's assets (sprite sheets, tilemaps, audio) for a 2D RPG
might be 4-8MB. Load time: under 1 second.

**Runtime streaming:** For games with more data than fits in DRAM (unlikely
with 64MB, but possible for voice-heavy games), stream assets from SD
during gameplay. Use a background DMA channel to read the next level's
data while the current level plays.

**Asset compression:** Store assets DEFLATE-compressed on SD. Decompress
into DRAM at load time. The CPU can inflate at ~50-100 MB/s, so a 2MB
compressed asset file decompresses in ~20ms. This lets you fit more
content on small SD cards and reduces load times (less data to read).

---

## 13. What the V3s Does NOT Have

Know your boundaries:

- **No hardware sprite engine.** Every sprite is a CPU blit.
- **No hardware tile map.** Tile rendering is a software inner loop.
- **No polygon rasterizer / GPU.** No 3D acceleration whatsoever.
- **No hardware texture rotation.** Rotating a sprite requires a software
  rotozoomer or pre-rendered rotation frames in the sprite sheet.
- **No hardware texture filtering** (beyond the DE2 VI scaler).
- **No second CPU core.** Single Cortex-A7. No SMP tricks.
- **Only 2 DE2 layers** (1 VI + 1 UI on V3s). Not 4, not 8. Two.
- **64MB total DRAM** shared between code, data, framebuffers, and assets.
  Budget carefully.

Every one of these limitations is a design constraint that forces creative
solutions. The SNES had the same story — no framebuffer at all, just
hardware tile planes and a tiny OAM — and it produced Chrono Trigger.

---

## 14. The Full Trick Stack (Reference Architecture)

A shipping game on the Jupiter would use all of these together:

```
┌─────────────────────────────────────────────────────────┐
│                    VBLANK ISR                           │
│  1. Swap DE2 framebuffer pointer (FB0 ↔ FB1)           │
│  2. Commit: MIX_GLB_DBUF = 1                           │
│  3. Kick DMA: clear back buffer                        │
│  4. Read input                                         │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    GAME LOGIC                           │
│  • Update positions, physics, AI state                 │
│  • ~1-2ms on a typical frame                           │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    RENDERING                            │
│  • NEON tile blitter: parallax background layers       │
│  • NEON sprite blitter: characters, enemies, FX        │
│  • Dirty rect or full redraw depending on camera state │
│  • Render at 400×240 into back buffer                  │
│  • ~4-8ms on a typical frame                           │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    AUDIO MIX                            │
│  • 8-channel software mixer into DMA ring buffer       │
│  • Music replayer (MOD/XM)                             │
│  • ~0.1ms                                              │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    WFI                                  │
│  • CPU sleeps until next vblank                        │
│  • ~5-10ms of idle time = headroom                     │
└─────────────────────────────────────────────────────────┘
                          │
                 (DE2 hardware, zero CPU cost)
                          │
┌─────────────────────────────────────────────────────────┐
│                    DE2 PIPELINE                         │
│  • VI layer: game world at 400×240, scaled to 800×480  │
│  • UI layer: HUD overlay (scanlines, health bar, score)│
│  • Blender: alpha composite VI + UI → TCON             │
│  • TCON: LCD timing → AT070TN83 panel                  │
└─────────────────────────────────────────────────────────┘
```

Total frame time budget: 16.67ms at 60fps.
Typical usage: 6-10ms, leaving 40-60% headroom for complex scenes.

---

## 15. Virtual Master Clock — Raster Interrupts on Async Hardware

On the Saturn, the Genesis, and the SNES, every chip in the system derived
its timing from a single master oscillator. The CPU and the video hardware
were phase-locked. A programmer could count CPU cycles and know *exactly*
which pixel the beam was painting. This is how racing-the-beam tricks
worked: per-scanline palette swaps, scroll register splits, mode changes
mid-frame. The hardware was deterministic to the nanosecond.

The V3s doesn't work like that. The CPU runs on PLL_CPU (1.2 GHz), the
DE2/TCON runs on PLL_VIDEO (264 MHz), and audio runs on PLL_AUDIO. They're
asynchronous. The CPU has no inherent knowledge of which scanline the TCON
is currently outputting.

But the TCON gives us a way back in: **line interrupts.**

### 15.1 The TCON Line Interrupt

TCON0 GINT0 register (offset 0x04) can trigger an IRQ at a specific
scanline number. GINT1 (offset 0x08) reports the current line. By setting
a target line in GINT0 and enabling the line match interrupt, you get an
IRQ that fires when the TCON's scanout reaches that line.

In that ISR, you can change DE2 parameters — layer scroll offset, alpha
value, even the framebuffer pointer — and commit with DBUFF. The DE2
picks up the change on the next scanline. This is a software raster
interrupt.

### 15.2 Jitter Budget

An IRQ on the Cortex-A7 takes ~20-40 cycles to enter the handler (pipeline
flush, mode switch, vector fetch). At 1.2 GHz that's ~25-33 nanoseconds.
One pixel at 33 MHz is ~30 nanoseconds.

This means your raster ISR lands with ±1 pixel of horizontal jitter. For
vertical effects (scroll splits, color changes, alpha ramps) this is
invisible — the change happens between scanlines. For horizontal effects
that need mid-scanline precision, the jitter is visible as a 1-pixel wobble
on the split line. The old consoles had zero jitter. We have ±1 pixel.
That's the tax for async clocks.

### 15.3 The Virtual Clock Architecture

To get predictable raster timing, build a virtual master clock that
synchronizes to the TCON's actual scanout:

**Calibration (once at boot):**
```c
// At vblank ISR:
uint32_t timer_at_vblank = timer_read();
// At next vblank ISR:
uint32_t timer_at_next_vblank = timer_read();
uint32_t ticks_per_frame = timer_at_next_vblank - timer_at_vblank;
uint32_t ticks_per_line = ticks_per_frame / LCD_VT; // 525 lines
```

Now you know exactly how many CPU timer ticks correspond to one scanline.
On the V3s: 1.2 GHz / 68 Hz / 525 lines ≈ 33,613 ticks per scanline.

**Scheduling raster effects:**
```c
// "At scanline 200, change the scroll register"
uint32_t target_tick = vblank_tick + (200 * ticks_per_line);
timer_set_compare(target_tick);
// Timer ISR fires → write scroll register → commit DBUFF
```

**Re-sync every frame:** Drift between PLL_CPU and PLL_VIDEO means the
virtual clock drifts ~1 scanline per few seconds. Re-calibrate
`ticks_per_line` at every vblank by comparing the predicted vblank time
to the actual vblank IRQ time. This keeps the virtual clock locked to
within ±1 line indefinitely.

### 15.4 What This Enables

Classic raster effects, adapted for the Jupiter's architecture:

**Scroll splits (parallax distortion):**
Schedule 4-8 line interrupts at different Y positions. Each ISR writes a
different X-scroll offset to the DE2 layer's BLD_OFFSET register. The
background appears to have per-band horizontal scrolling — the technique
behind every 16-bit-era water reflection and heat shimmer.

**Per-scanline palette ramps:**
The DE2 doesn't have a palette (it's true-color), but you can simulate
palette effects by changing the UI overlay's global alpha or fill color
at specific scanlines. A gradient from 0% to 30% black overlay from top
to bottom simulates atmospheric depth — dark sky at top, bright ground
at bottom — for free.

**Mode switches:**
At a specific scanline, swap the VI framebuffer pointer from the "sky"
buffer (rendered at a different resolution or with different content) to
the "ground" buffer. The Saturn's VDP2 did exactly this for games that
had a 3D sky and a 2D ground plane.

**Water reflections:**
Below a water line (say, scanline 300), flip the VI layer's Y coordinate
and increase the UI overlay alpha to 40% blue-tinted. The bottom third
of the screen becomes a darkened, vertically-mirrored reflection of the
top. One line interrupt, two register writes.

### 15.5 Limitations vs True Master Clock

| Capability                      | Retro (master clock) | Jupiter (virtual clock) |
|---------------------------------|---------------------|------------------------|
| Scanline-accurate V effects     | Perfect             | ±1 line (re-synced)    |
| Pixel-accurate H effects        | Perfect             | ±1 pixel jitter        |
| Mid-scanline register changes   | Yes (cycle-counted) | No (ISR too slow)      |
| Racing the beam                 | Yes                 | No                     |
| Beam position readback          | Instant, exact      | Stale by ~30ns         |
| Multiple splits per frame       | Limited by CPU speed| Dozens (CPU is fast)   |
| Raster effect complexity        | Simple (slow CPU)   | Complex (fast CPU)     |

The Jupiter trades horizontal precision for vertical abundance. You can't
race the beam, but you can schedule 50 raster events per frame where the
SNES could manage 8. The effects are slightly less precise but far more
numerous and complex.

### 15.6 The Philosophical Argument

The Manifesto says: *"What if the fifth-generation philosophy had continued
to evolve? Not backward. Not retro. Forward."*

The virtual master clock is exactly this. The old consoles had raster
interrupts because the hardware was simple enough to be deterministic. We
lost that when consoles became async multi-clock-domain PCs. The Jupiter
gets it back — not by reverting to a single oscillator, but by building a
software synchronization layer that gives the developer the *same creative
vocabulary* (scroll splits, mode changes, palette ramps) on modern silicon.

It's not emulation. It's evolution.

---

*The developer is the operating system. The CPU is the renderer. The DE2
is the mixing console. NEON is the brush. DMA is the stagehand. Every
watt serves the game.*
