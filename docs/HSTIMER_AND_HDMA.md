# HSTimer & HDMA Work Distribution

## Jupiter SDK — Allwinner V3s / Lichee Pi Zero

Scanline-accurate mid-frame interrupts using the V3s High-Speed Timer, and HDMA-style work distribution to reduce vblank pressure.

---

## Part 1: HSTimer Hardware

### Registers

Base: `0x01C60000`. Two independent 56-bit down-counters clocked from AHB (~200MHz, 5ns/tick).

| Register | Offset | Description |
|----------|--------|-------------|
| IRQ_EN | 0x00 | Interrupt enable (bit 0 = timer0, bit 1 = timer1) |
| IRQ_STAS | 0x04 | Interrupt status (w1c) |
| TMR0_CTRL | 0x10 | Timer 0 control |
| TMR0_INTV_LO | 0x14 | Timer 0 interval (low 32 bits) |
| TMR0_INTV_HI | 0x18 | Timer 0 interval (high 24 bits) |
| TMR0_CURNT_LO | 0x1C | Timer 0 current count (low 32) |
| TMR0_CURNT_HI | 0x20 | Timer 0 current count (high 24) |
| TMR1_* | 0x30+ | Timer 1 (same layout, +0x20 offset) |

### CTRL bits

| Bit | Name | Description |
|-----|------|-------------|
| 7 | MODE | 0 = continuous, 1 = single-shot |
| 6:4 | PRESCALE | 0=/1, 1=/2, 2=/4, 3=/8, 4=/16 |
| 1 | RELOAD | Load interval into counter |
| 0 | ENABLE | Start counting down |

### Clock Setup

```c
// BUS_CLK_GATING0 bit 19 — enable HSTimer bus clock
REG32(CCU_BASE + 0x0060) |= (1u << 19);

// BUS_SOFT_RST0 bit 19 — de-assert reset
REG32(CCU_BASE + 0x02C0) |= (1u << 19);
```

### IRQ Numbers

From `irqs-sun8iw8p1.h` (V3s-specific):
```
SUNXI_IRQ_HSTMR0 = SUNXI_GIC_START + 51  →  GIC IRQ 83
SUNXI_IRQ_HSTMR1 = SUNXI_GIC_START + 52  →  GIC IRQ 84
```

**WARNING:** The sun8iw1 (A31) header uses the same SPI numbers for HSTimer, but the V3s uses a different IRQ map. SPI 51 = Timer1 on A31, SPI 51 = HSTimer0 on V3s. Always use the `irqs-sun8iw8p1.h` file.

### Scanline Timing

At our panel timing (525 pixels/line @ 9MHz pixel clock):
- 1 scanline = 58.33μs
- AHB at 200MHz = 5ns/tick
- **11,666 ticks per scanline**

---

## Part 2: The IRQ Routing Bug

### Symptom

Timer hardware fired correctly — `IRQ_STAS` showed pending=1 after countdown. But the GIC never delivered the interrupt to the CPU. ISR never ran.

Diagnostic output from the benchmark:
```
[hdma] ISR TIMEOUT! Diagnostics:
  IRQ_EN=0x00000001  IRQ_STAS=0x00000001
  TMR0_CTRL=0x00000082  TMR0_CURNT=0x0018EBD8
  Trying poll: PENDING! IRQ routing broken
```

### Root Cause

**`irq_init()` was never called.** The GIC distributor (GICD_CTLR) stayed at 0 — disabled. The `hstimer_init()` function called `irq_register()` and `irq_enable()`, which wrote the handler table entry and set the ISENABLER bit. But with the distributor disabled, interrupts never reached the CPU interface.

Every other demo that uses interrupts (audio DMA, etc.) calls `irq_init()` in main. The HSTimer demos were the only ones that forgot it.

### The Fix

One line:
```c
irq_init();       // <-- THIS WAS MISSING
hstimer_init();
irq_global_enable();
```

`irq_init()` does:
1. Zero the handler table
2. Disable all SPIs via GICD_ICENABLER
3. Set priority 0xA0 and target CPU0 for all SPIs (IDs 32-159)
4. Enable distributor: `GICD_CTLR = 1`
5. Enable CPU interface: `GICC_CTLR = 1`, `GICC_PMR = 0xFF`

### Lesson

Always verify the full init chain. The HSTimer hardware, clock gating, reset, interval, and IRQ enable were all correct. The failure was one level up in the interrupt delivery stack — the GIC distributor was never turned on.

---

## Part 3: HDMA Work Distribution — Benchmark Results

### Concept

Classic consoles (SNES, Genesis) use HDMA (Horizontal DMA) to spread work across the frame. Instead of doing everything in the vblank period, schedule mid-frame interrupts to handle non-rendering tasks (audio mixing, game logic) while the display controller scans out the current frame.

On the V3s, we use HSTimer to fire an ISR at scanline 140 (mid-frame). The rendering happens in the vblank burst; audio mix + game logic happen in the ISR.

### Test Setup

- NES PPU rendering: checkerboard background with 32 sprites
- Simulated audio mix: 30K iterations (~375μs)
- Simulated game logic: 50K iterations (~625μs)
- 120 frames (2 seconds) per mode

### Mode A — Burst (baseline)

All work done sequentially after vblank:

```
avg frame:    4775μs
max frame:    5367μs
avg render:   3617μs
avg audio:     375μs
avg logic:     625μs
budget:      16667μs (60fps)
headroom:    11892μs
```

Peak vblank pressure: **5367μs** — everything crammed into one burst.

### Mode B — HDMA (HSTimer ISR)

Rendering at vblank, audio+logic deferred to scanline 140 ISR:

```
avg frame:    9167μs
max frame:    9169μs
max vblank burst: 4366μs
budget:      16667μs (60fps)
headroom:     7500μs
```

Peak vblank pressure: **4366μs** — audio+logic moved out of the burst.

### Analysis

| Metric | Burst | HDMA | Delta |
|--------|-------|------|-------|
| Max vblank load | 5367μs | 4366μs | **-1001μs** |
| Avg total frame | 4775μs | 9167μs | +4392μs |

The total frame time is higher in HDMA mode because we intentionally *wait* for scanline 140 before running audio+logic (140 scanlines × 58.33μs = 8166μs into the frame). The point isn't total time — it's **peak vblank pressure**.

With a heavier rendering load (closer to the 16.7ms budget), the difference matters more. If rendering takes 14ms in burst mode, you're over budget. With HDMA, rendering takes 14ms but audio+logic run during scanout — you're under budget.

### When HDMA Wins

- **Heavy rendering** that nearly fills the vblank period
- **Consistent audio** that can't tolerate jitter from competing with rendering
- **Complex game logic** (AI, physics) that doesn't need results until next frame

### When Burst Is Fine

- **Light rendering** with plenty of headroom (our test case: 5ms of 16.7ms)
- **Tightly coupled** logic that depends on the current frame's render output

---

## Part 4: DE2 Raster Effects — What Doesn't Work

### Attempted: Mid-frame BLD_BKCOLOR changes

Tried using HSTimer to change the DE2 blender background color at different scanlines to create sky gradients (like NES/SNES/Genesis raster effects).

**Attempt 1: Bare register write.** Wrote BLD_BKCOLOR (MIXER0+0x1088) in the HSTimer ISR without committing. No effect — sky stays solid blue.

**Attempt 2: Write + GLB_DBUFF commit.** Wrote BLD_BKCOLOR then poked GLB_DBUFF=1 (MIXER0+0x08) to force a double-buffer latch mid-frame. Still no effect — sky stays solid blue.

**Conclusion:** The DE2 mixer is strictly vblank-committed. GLB_DBUFF queues a commit for the next vertical blanking period, not immediately. There is no way to change DE2 parameters mid-scanline. This is by design — it's a modern compositor, not a retro-style scanline register file.

### What DOES Work for Raster-like Effects

1. **Software rendering** — compute per-scanline colors in the CPU and write to the framebuffer directly. NES/SNES/GB renderers already do this.
2. **Pre-baked gradients** — render the sky gradient into the framebuffer at init time.
3. **NEON row fill** — fill rows with different colors using NEON memset, fast enough for real-time.

The DE2 is not a retro-style display controller with per-scanline registers. It's a compositor that reads framebuffers and blends them. All "raster effects" must happen in software before the framebuffer is written.

---

## Part 5: API Reference

### hstimer.h

```c
#define HSTIMER_TICKS_PER_SCANLINE  11666

void hstimer_init(void);
void hstimer_set_scanline(int timer, uint32_t scanlines, void (*cb)(void));
void hstimer_set_repeating(int timer, uint32_t scanlines, void (*cb)(void));
void hstimer_set_ticks(int timer, uint32_t ticks, int oneshot, void (*cb)(void));
void hstimer_stop(int timer);
```

### Required Init Sequence

```c
timer_init();        // System timer
mmu_init();          // MMU + D-cache
irq_init();          // GIC distributor + CPU interface  ← DON'T FORGET
hstimer_init();      // Clock gate + reset + ISR registration
irq_global_enable(); // CPSIE i
```

### Usage Pattern

```c
// One-shot: fire ISR at scanline 140
hstimer_set_scanline(0, 140, my_isr);

// Repeating: fire every 28 scanlines
hstimer_set_repeating(0, 28, raster_isr);

// Raw ticks for sub-scanline precision
hstimer_set_ticks(0, 5000, 1, precise_isr);  // 25μs one-shot

// Stop
hstimer_stop(0);
```
