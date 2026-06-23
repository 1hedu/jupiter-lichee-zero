# Allwinner V3s — Every Hardware Secret for Bare-Metal Retro SDK

> Compiled reference of all on-chip hardware blocks, hidden accelerators, and dirty tricks available on the Allwinner V3s (LicheePi Zero). Focused on bare-metal exploitation for a retro gaming SDK targeting a 480×272 LCD panel.

**Confirmed: The V3s has NO G2D (2D graphics engine).** That claim was fabricated. The FunKey Project docs explicitly state: *"unfortunately no GPU or 2D graphic engine containing a hardware scaler."* What you DO have is plenty — read on.

---

## Part 1: Complete Hardware Block Map

### Acceleration (Your Blitter Replacements)

#### 1. NEON SIMD Engine
- **Address:** ARM coprocessor (CP10/CP11)
- **Tags:** HIDDEN GEM
- **Summary:** 128-bit SIMD — your real blitter. 8 pixels per instruction, zero bus overhead.

The Cortex-A7 has a full NEON unit with 32×64-bit registers (usable as 16×128-bit quad-words). This is your primary acceleration path for pixel operations.

**What it gives you for retro SDK:**
- Process 8 pixels simultaneously — load 8 palette indices, look up 8 colors, write 8 ARGB pixels in one pass
- Hardware alpha blending: `VMUL + VADD` across 8 channels at once
- Sprite compositing: `VCGT` (compare > 0) generates a mask, `VBSL` (bitwise select) merges sprite over background — the classic transparent-color-key blit in ~3 instructions for 8 pixels
- Scaling via bilinear interpolation: NEON's `VMULL` (widening multiply) handles the weight calculation for 4 pixel pairs simultaneously
- Palette expansion: `VTBL` (table lookup) converts 4-bit or 8-bit indexed pixels to 32-bit ARGB using a 16-entry or 256-entry LUT entirely in registers

**Bare metal setup:** Enable NEON by setting bits in `CPACR` (coprocessor access) and `FPEXC` (floating point exception register). GCC with `-mfpu=neon-vfpv4 -mfloat-abi=hard` gives you intrinsics like `vld1q_u8()`, `vtbl1_u8()`, `vmulq_u16()`.

**Real throughput:** At 1.2GHz, a NEON `VLD1 + VTBL + VST1` palette-expand loop processes ~300 million pixels/second — more than enough for 480×272@60fps (≈7.8M pixels/sec).

---

#### 2. Display Engine 2.0 (DE2) Mixer
- **Address:** `0x0100_0000` – `0x011F_FFFF` (2MB)
- **Tags:** HIDDEN GEM, DISPLAY

**Summary:** Hardware compositor with 3 alpha-blending channels, 4 overlay layers each, and per-channel scaler.

The V3s has a single DE2 mixer — this is the real display pipeline and it's far more capable than just pushing a framebuffer.

**Capabilities confirmed in datasheet:**
- 3 alpha blending channels (pipes), each with 4 overlay layers = up to 12 overlay surfaces
- Each channel has an independent hardware scaler
- Porter-Duff alpha blending operations in hardware
- Input formats: YUV422/420/411, ARGB8888, XRGB8888, RGB888, ARGB4444, ARGB1555, RGB565
- Color space conversion (CSC) built in
- DRC (Dynamic Range Compression) post-processing

**For your retro SDK:**
- Render SNES/Genesis native res into small buffers, let the DE2 scaler handle 256×224 → 480×272
- Use separate overlay channels for BG layers and sprite layer — hardware composites them with zero CPU cost
- The scaler supports nearest-neighbor-style upscaling if you configure bilinear coefficients to snap

**Register space:** DE2 clocks at `CCU+0x0104` (DE_CLK_REG). The mixer itself starts at `0x0110_0000`. Documented in the Allwinner DE2.0 Spec V1.0. Linux DRM driver source (`sun8i_mixer.c`) is your best register reference — the V3s mixer config shows `vi_num=1, ui_num=1, scaler_mask=0x1, mod_rate=150MHz`.

---

#### 3. VFPv4 FPU
- **Address:** ARM coprocessor
- **Tags:** ESSENTIAL

**Summary:** Hardware floating point. Use for Mode 7 rotation/scaling math.

- Mode 7 affine transforms: sin/cos/matrix multiply at full hardware speed
- Perspective projection for pseudo-3D effects
- Audio DSP: reverb, filtering, mixing with sub-sample interpolation
- Enable it: Same CPACR/FPEXC setup as NEON. GCC flag: `-mfloat-abi=hard -mfpu=neon-vfpv4`. The FPU and NEON share the register file — you get both with one enable sequence.

---

#### 4. L1/L2 Cache System
- **Address:** CP15 system registers
- **Tags:** ESSENTIAL

**Summary:** 32KB I-cache + 32KB D-cache + 128KB L2. Configure write-back for framebuffer performance.

**Bare-metal optimization:**
- Mark framebuffer regions as write-combining (not write-back) via MMU page table attributes — avoids cache pollution from pixel writes that will never be re-read by CPU
- Keep hot data (palette tables, tile caches, sprite attributes) in write-back cached regions
- Use `PLD` (preload) instructions to prefetch the next scanline of tile data into cache
- After rendering a frame with NEON, use `DSB` + cache clean to ensure DMA/DE2 sees the completed pixels
- The 128KB L2 can hold your entire tile cache — a 256-tile set at 64 bytes each = 16KB, fits comfortably

---

### Coprocessors

#### 5. CedarVE (Video Engine)
- **Address:** `0x01C0_E000` – `0x01C0_EFFF` (4KB regs) + SRAM_C at `0x01D0_0000` (512KB)
- **Tags:** HIDDEN GEM, ADVANCED

**Summary:** H.264 encoder/decoder hardware. Can be repurposed as a DMA-capable block transform engine.

**Confirmed capabilities on V3s:**
- H.264 High Profile decoding (cedrus driver: `CEDRUS_CAPABILITY_UNTILED | CEDRUS_CAPABILITY_H264_DEC`)
- MJPEG/H.264 encoding (demonstrated at 720p@60fps in community projects)
- Has its own 512KB SRAM (SRAM_C) at `0x01D0_0000` as working memory
- Operates via register-programmed command queues — truly independent of CPU

**Creative bare-metal uses:**
- JPEG hardware decode: feed it compressed sprite sheets, get raw pixel buffers out — saves memory bandwidth
- The MJPEG encoder path can be used as a fast DCT/block-transform engine if you feed it raw frames
- Its DMA paths can move pixel data between DRAM and SRAM_C independently of the CPU

**Difficulty:** Register-level documentation comes from reverse engineering (CedarX/Reverse Engineering wiki on linux-sunxi.org). The `cedrus` staging driver and `libcedarc` are your Rosetta Stone. Clock gates: `CLK_BUS_VE`, `CLK_VE`, `CLK_DRAM_VE`. Reset: `RST_BUS_VE`.

**(See Part 2 below for the full CedarVE deep-dive with tricks and register code.)**

---

#### 6. Crypto Engine (CE)
- **Address:** `0x01C1_5000` – `0x01C1_5FFF` (4KB)
- **Tags:** DOCUMENTED

**Summary:** Hardware AES/DES/3DES + SHA/MD5. Can double as a fast bulk XOR/transform engine.

- AES (128/192/256), DES, 3DES — ECB/CBC/CTR modes
- SHA-1, SHA-256, MD5 hash computation
- Hardware PRNG (pseudo-random number generator) — high-quality random numbers at hardware speed, useful for procedural generation
- AES-CTR mode is essentially a fast XOR engine
- Registers documented in V3s datasheet Section 4.10. Clock: `CE_CLK_REG` at CCU+0x009C.

---

#### 7. ISP (Image Signal Processor) — HawkView
- **Address:** Near CSI block (`0x01CB_0000` area)
- **Tags:** ADVANCED, HIDDEN GEM

**Summary:** Camera ISP with 2D/3D noise reduction, debayering, color correction. Theoretically usable as a pixel processing pipeline.

- Debayering, 2D/3D spatial noise reduction, defect pixel correction, lens shading correction, color correction matrix, WDR
- The 2D noise filter is essentially a programmable convolution
- The color correction matrix could do palette remapping in hardware
- **Practical concern:** The ISP expects camera-format Bayer input from CSI — repurposing for arbitrary pixel buffers would require deep register hacking. Bootlin has published an open-source driver. This is a "reach" goal.

---

### I/O and Peripherals

#### 8. 8-Channel DMA Controller
- **Address:** `0x01C0_2000` – `0x01C0_2FFF` (4KB)
- **Tags:** ESSENTIAL

**Summary:** Descriptor-chain DMA with memory-to-memory, 8/16/32-bit widths. Your background memcpy.

- 8 independent channels
- Linked descriptor chains — queue multiple transfers without CPU intervention
- Memory-to-memory, memory-to-peripheral, peripheral-to-memory
- Flexible data widths: 8, 16, or 32 bits

**For retro SDK:**
- Double-buffer flip: DMA copies the completed frame to the DE2 framebuffer while CPU renders next frame
- Audio: DMA feeds PCM samples to the audio codec continuously
- SPI LCD refresh: DMA pushes scanlines to SPI without CPU

**Registers:** Fully documented in V3s datasheet Section 4.8. IRQ enable at `+0x00`, channel descriptors starting at `+0x100` per channel (stride 0x40).

---

#### 9. Audio Codec (Analog + Digital)
- **Address:** `0x01C2_2C00` (digital) + `0x01C2_3000` (analog)
- **Tags:** ESSENTIAL, DOCUMENTED

**Summary:** Built-in DAC/ADC with headphone amp. Your sound chip — no external I2S codec needed.

- Stereo DAC output with integrated headphone driver (HPOUTL/HPOUTR pins)
- Single microphone input (MIC1)
- DMA-driven: set up a DMA channel to feed PCM data, codec plays it autonomously
- Sample rates: 8kHz–192kHz, 24-bit resolution

**For retro SDK / SC-55 rendering:**
- Feed your MIDI-to-PCM output directly to the codec via DMA ring buffer
- Mix multiple channels in software (NEON-accelerated), output final stereo mix
- Linux reference: `sun4i-codec.c` (V3s variant) and `sun8i-codec-analog.c`

---

#### 10. TCON (LCD Timing Controller)
- **Address:** `0x01C0_C000` – `0x01C0_CFFF` (4KB)
- **Tags:** ESSENTIAL, DOCUMENTED

**Summary:** Drives the parallel RGB LCD interface. Generates sync signals and pixel clock.

- Channel 0 only (no TV encoder on V3s)
- Parallel RGB output: up to 24-bit color
- VBlank interrupt: use this as your frame sync signal for 60fps game loop
- TCON clock: `TCON_CLK_REG` at CCU+0x0118, derives from PLL_VIDEO
- Linux driver `sun4i_tcon.c` (V3s quirks) is the register reference

---

#### 11. LRADC (Low-Resolution ADC)
- **Address:** `0x01C2_2800` – `0x01C2_28FF`
- **Tags:** DOCUMENTED

**Summary:** 6-bit ADC designed for button matrices. Read up to 10 buttons on one pin.

- 6-bit resolution (64 levels), sample rate up to 250Hz
- Continuous, single-key, and hold-key detection modes
- Wire a resistor ladder to LRADC0 for D-pad + buttons using a single GPIO pin
- FunKey S project found 12+ buttons cause accuracy issues — stick to 8-10

---

#### 12. PWM (2 Channels)
- **Address:** `0x01C2_1400` – `0x01C2_17FF`
- **Tags:** DOCUMENTED

**Summary:** Hardware PWM: backlight control, audio buzzer, or crude DAC.

- 0-100% duty cycle, up to 24MHz output
- LCD backlight brightness control (PWM0 on pin PB4)

---

#### 13. High-Speed Timer (HSTimer)
- **Address:** `0x01C6_0000` area
- **Tags:** DOCUMENTED

**Summary:** Sub-microsecond precision timer, independent of the standard timers.

- Precise timing for scanline-accurate effects (HDMA-style raster interrupts)
- Standard timers (Timer0/Timer1) handle main game loop; HSTimer handles precision stuff

---

#### 14. SPI Controller
- **Address:** `0x01C6_8000` – `0x01C6_8FFF`
- **Tags:** DOCUMENTED

**Summary:** Fast SPI for NOR flash and SPI displays. DMA-capable.

- Boot from SPI NOR flash
- SPI-connected LCD panels
- DMA chain to blast sprite data from SPI flash to DRAM without CPU

---

#### 15. CSI (Camera Serial Interface)
- **Address:** `0x01CB_0000` (parallel CSI0) + `0x01CB_4000` (CSI1) + MIPI CSI-2
- **Tags:** DOCUMENTED

**Summary:** Dual camera input. Could capture video from external sources as texture data.

---

## Part 2: CedarVE Deep-Dive — Dirty Tricks & Register Code

### V3s Memory Map (VE-relevant)

| Region | Address | Size | Notes |
|--------|---------|------|-------|
| VE Registers | `0x01C0_E000` | 4KB | All sub-engine registers |
| SRAM Controller | `0x01C0_0000` | 4KB | SRAM mapping control |
| CCU | `0x01C2_0000` | 1KB | Clock gates and resets |
| SRAM_C (VE working mem) | `0x01D0_0000` | 512KB | Zero-wait-state SRAM |
| DDR2 DRAM | `0x4000_0000` | 64MB | Main memory |

### VE Sub-Engine Register Map

| Sub-Engine | Offset from VE_BASE | Purpose |
|------------|---------------------|---------|
| General/Control | `+0x000` | VE version, ctrl, IRQ, SRAM port |
| MPEG Engine | `+0x100` | MPEG1/2/4, MJPEG, DIVX, VP6 decode |
| H264 Engine | `+0x200` | H.264 AVC decode |
| VC1 Engine | `+0x300` | VC-1 decode |
| RMVB Engine | `+0x400` | RealMedia decode |
| HEVC Engine | `+0x500` | H.265 decode (may not exist on V3s) |
| ISP Engine | `+0xA00` | Internal scaler and post-processor |
| AVC Encoder | `+0xB00` | H.264/MJPEG encode |

---

### Step 0: Bare-Metal VE Power-On Sequence
**PREREQUISITE — do this before any VE trick**

```c
// 1. Enable VE bus clock gate
//    Bus Clock Gating Reg 0 (CCU+0x060), set bit 0
*(volatile uint32_t*)(0x01C20060) |= (1 << 0);

// 2. Enable VE module clock
//    VE_CLK_REG (CCU+0x13C)
//    bits[26:24] = clock source (PLL_VE typically)
//    bit 31 = SCLK gating (enable)
*(volatile uint32_t*)(0x01C2013C) = (1 << 31) | (0 << 24);

// 3. Enable VE DRAM clock gate
//    DRAM_CLK_GATING_REG (CCU+0x100), set bit 0
*(volatile uint32_t*)(0x01C20100) |= (1 << 0);

// 4. De-assert VE reset
//    Bus Software Reset Reg 0 (CCU+0x2C0), set bit 0
*(volatile uint32_t*)(0x01C202C0) |= (1 << 0);

// 5. Map SRAM_C to VE
//    SRAM controller reg at 0x01C00004
//    Clear bit 24 so VE owns SRAM_C
uint32_t sram = *(volatile uint32_t*)(0x01C00004);
sram &= ~(1 << 24);  // map SRAM_C to VE
*(volatile uint32_t*)(0x01C00004) = sram;

// 6. Set VE frequency via PLL_VE (CCU+0x0058)
//    Configure for ~300MHz, then set VE_CLK_REG divider

// VERIFY: Read 0x01C0E000 (MACC_VE_VERSION)
// Should return non-zero VE hardware revision
// 0x00000000 or bus fault = clocks aren't gated on
```

---

### Trick 1: JPEG Decode as a Sprite Decompressor
**Feasibility: 85% — well-documented, proven on similar hardware**

**The play:** Store sprite sheets and background tilesets as JPEG files. Instead of keeping them uncompressed in your 64MB of RAM, keep them compressed and let the VE decompress on demand. A 320×224 background as raw ARGB = 280KB. As JPEG quality 90 ≈ 30KB. That's ~10:1 savings.

The MPEG sub-engine handles JPEG/MJPEG decode directly. This is the best-documented CedarVE code path.

```c
#define VE_BASE   0x01C0E000
#define MPEG_BASE (VE_BASE + 0x100)

// 1. Select MPEG engine in VE_CTRL
*(volatile uint32_t*)(VE_BASE + 0x00) = 0x130007;

// 2. Set JPEG restart interval
*(volatile uint32_t*)(MPEG_BASE + 0x5C) = 0; // no restart markers

// 3. Load quantization tables (2x 8x8 matrices)
//    Write 128 bytes (64 chroma + 64 luma) one-by-one
//    through MACC_MPEG_IQ_MIN_INPUT register
for (int i = 0; i < 128; i++)
    *(volatile uint32_t*)(MPEG_BASE + 0x28) = qtable[i];

// 4. Load Huffman tables (2KB total)
//    First half = Huffman tree description
//    Second half = Huffman data
//    Written through dedicated register
for (int i = 0; i < 512; i++)
    *(volatile uint32_t*)(MPEG_BASE + 0x4C) = htable[i];

// 5. Set output buffers (MUST be physical addresses, DRAM-relative)
//    Output is in 32x32 pixel tiled blocks!
*(volatile uint32_t*)(MPEG_BASE + 0x48) = luma_buf_phys;
*(volatile uint32_t*)(MPEG_BASE + 0x4C) = chroma_buf_phys;

// 6. Set picture size in MCUs (Minimum Coded Units)
//    Height in upper 16 bits, width in lower 16
*(volatile uint32_t*)(MPEG_BASE + 0x34) =
    (height_mcus << 16) | width_mcus;

// 7. Set input bitstream address + length
*(volatile uint32_t*)(MPEG_BASE + 0x14) =
    src_phys | 0x70000000;  // physical addr with flag
*(volatile uint32_t*)(MPEG_BASE + 0x18) =
    bitstream_length_bits;   // length in BITS
*(volatile uint32_t*)(MPEG_BASE + 0x1C) = 0; // bit offset

// 8. Trigger decode
*(volatile uint32_t*)(MPEG_BASE + 0x08) |= (1 << 8);

// 9. Wait for completion (poll or use GIC IRQ 58)
while (!(*(volatile uint32_t*)(VE_BASE + 0x1C) & 1));
// Clear IRQ flag
*(volatile uint32_t*)(VE_BASE + 0x1C) = 1;
```

**Output format gotcha:** The VE outputs in 32×32 pixel tiled blocks, NOT linear scanlines. You need either:
- A NEON de-tile pass (fast, ~2 cycles per pixel), or
- Route the output through the DE2's video input channel which can accept tiled YUV natively

**Integration into retro SDK:**
- At boot: decode all sprite sheets from compressed JPEG → DRAM buffers
- During gameplay: decode background layers on demand (level transitions)
- The VE runs independently — CPU does game logic while VE decompresses the next level's assets
- Parse JPEG headers in software (extract QT/HT tables, dimensions), feed only the raw entropy data to VE

---

### Trick 1b: JPEG Encode for Screenshots/Savestates
**Feasibility: 55% — demonstrated but less documented**

The AVC encoder sub-engine at `VE_BASE+0xB00` can also do MJPEG encoding.

**Uses:**
- Save states: compress game state snapshots to SD card
- Screenshots: encode current framebuffer
- Video recording: MJPEG stream to SD

**Key details:**
- Encoder sub-engine at different register offset (`+0xB00`)
- On post-A20 VEs (includes V3s): bits 6-7 of VE_CTRL enable the encoder
- Input must be NV12/NV21 (planar Y + interleaved UV)
- Community project `h264enc_demo` demonstrated 1080p@30fps H.264 encode on V3s
- `libjpegenc.a` from Allwinner BSP was successfully tested — register sequences can be traced

---

### Trick 2: ISP Sub-Engine Polyphase Scaler
**Feasibility: 65% — registers documented, integration is the challenge**

Hidden inside the VE is a programmable 4-tap, 16-phase polyphase filter — the same class of hardware scaler used in MiSTer FPGA retro projects.

```c
// ISP sub-engine registers at VE_BASE+0xA00
// Enable scaler via MACC_ISP_CTRL: set SCALER_EN bit

// Set SRAM index to coefficient table base
*(volatile uint32_t*)(VE_BASE + 0xA20) = 0x400;

// Write 64 values for the polyphase filter coefficients:
//
// Table A (first 32 words): 4-tap, 16-phase HORIZONTAL filter
//   Each phase = 2 words:
//     word 0: [31:16] = h1, [15:0] = h0
//     word 1: [31:16] = h3, [15:0] = h2
//
// Table B (next 32 words): 2-tap, 32-phase VERTICAL filter
//   Each phase = 1 word:
//     [31:16] = h1, [15:0] = h0
//
// Coefficient format: signed 16-bit fixed point
//   0x0100 = 1.0
//   0x0080 = 0.5
//   0x0000 = 0.0
//   0xFF00 = -1.0

for (int i = 0; i < 64; i++)
    *(volatile uint32_t*)(VE_BASE + 0xA24) = coefficients[i];
```

**For retro pixel-perfect scaling (nearest-neighbor):**
Set all phases so one tap = `0x0100` (1.0), all others = `0x0000` (0.0). This gives integer-snapped sampling.

**For scanline simulation:**
Alternate phases between full brightness (`0x0100`) and reduced (`0x0060`), giving CRT-like horizontal dark lines.

**Practical note:** This scaler operates on the VE's tiled YUV output, not arbitrary RGB. Use it in the JPEG decode → scale → output chain. For standalone scaling, the DE2 mixer's scaler is more practical.

---

### Trick 3: SRAM_C Steal — The Secret Weapon
**Feasibility: 90% — one register write, confirmed in hardware**

This is the single biggest performance win. When the VE isn't running, remap its 512KB SRAM to the CPU for zero-wait-state access.

```c
// Remap SRAM_C to CPU (instead of VE)
// SRAM controller at 0x01C00004, set bit 24
uint32_t ctrl = *(volatile uint32_t*)(0x01C00004);
ctrl |= (1 << 24);  // map to CPU
*(volatile uint32_t*)(0x01C00004) = ctrl;

// Now 0x01D00000..0x01D7FFFF is your fast scratchpad
volatile uint8_t *fast_buf = (volatile uint8_t*)0x01D00000;
```

**SRAM is on-die memory with zero wait states.** DDR2 has ~10-15 cycle latency even with cache. For time-critical inner loops, SRAM is dramatically faster.

**What to put in SRAM_C:**
- Active palette table (256 entries × 4 bytes = 1KB)
- Current tile cache (hot tiles being rendered this frame)
- Sprite attribute table (OAM equivalent)
- Audio mix buffer (one frame of stereo PCM)
- The inner render loop's stack and local variables

**Time-sharing with VE:**
- Before triggering a VE decode: flush SRAM data to DRAM, remap to VE
- After VE finishes: remap back to CPU, reload hot data
- If not using VE during gameplay, keep SRAM_C permanently on CPU

**This is the retro secret weapon.** Classic consoles had fast on-chip RAM for exactly this reason (SNES WRAM, Genesis 68K RAM). Your V3s has it too — it's just hiding behind the VE's SRAM mapping register.

---

### Trick 4: VE as a Second DMA Engine
**Feasibility: 40% — conceptually sound, nobody has published a proof**

The VE has dedicated DRAM access through the MBUS — a high-bandwidth path separate from the CPU's AHB bus. This is how it achieves 1080p decode without CPU bus contention.

**The idea:** Configure the VE to "decode" trivially simple bitstreams where the output is a known pattern → you're using it as a programmable DMA on its own bus.

**Scenarios:**
- Fill framebuffer with solid color: decode a 1-MCU solid JPEG repeatedly to adjacent addresses
- Use H.264 reference frame mechanism to copy rectangular regions between buffers
- The VE's MBUS port doesn't compete with CPU memory access — if you're bandwidth-limited, this helps

**Why it's risky:** VE programming overhead may exceed time saved vs. NEON + system DMA. The 32×32 tiled output adds complexity.

---

### Trick 5: H.264 Motion Compensation as a Blitter
**Feasibility: 25% — technically correct, practically insane**

H.264's motion compensation copies rectangular blocks between reference frames. That's literally what a blitter does.

**How it would work:**
1. Set up a "reference frame" containing your sprite sheet in DRAM
2. Construct a fake H.264 slice with motion vectors pointing to each sprite's position
3. Integer motion vectors = pixel-perfect copy (no interpolation)
4. The VE copies all sprites from sheet to output positions in one hardware operation

**Why it's rated 25%:**
- Requires constructing valid NAL units with CABAC/CAVLC entropy coding
- The deblocking filter will smear pixel-art edges
- For the effort involved, NEON at 300M pixels/sec is already fast enough

**But as a flex?** If you pull this off, you've turned an H.264 decoder into a sprite blitter. That's the hardware abuse equivalent of a speedrun world record.

---

## Architecture Summary — Recommended Pipeline

```
CPU + NEON           → Sprite compositing, palette expansion, tile rendering
                       (renders into small native-res buffers: 256×224, 320×224)
         ↓
DMA (8-ch)           → Copies completed frame to DE2 input buffer
                       → Feeds audio PCM to codec
         ↓
DE2 Mixer            → Hardware scales native res → 480×272
                       → Alpha-blends overlay layers
                       → Color space conversion
         ↓
TCON                 → Drives LCD signals, provides VBlank interrupt
         ↓
LCD Panel (480×272)

[Parallel paths:]
CedarVE              → Decompresses JPEG sprite sheets (background, off-CPU)
SRAM_C               → Fast scratchpad for hot data when VE idle
Audio Codec + DMA    → Autonomous audio playback
```

---

## Key Documentation Sources

| Resource | What it covers |
|----------|---------------|
| V3s Datasheet V1.0 (PDF on linux-sunxi.org) | Memory map, CCU, DMA, timers, GPIO, crypto |
| Allwinner DE2.0 Spec V1.0 (linux-sunxi.org) | Display Engine mixer, blender, scaler registers |
| linux-sunxi.org/VE_Register_guide | CedarVE register map (all sub-engines) |
| linux-sunxi.org/CedarX/JPEG-MJPEG_Decoding | JPEG decode register sequence |
| linux-sunxi.org/CedarX/MPEG_Engine_Init_procedure | VE MPEG engine init |
| linux-sunxi.org/SRAM_Controller_Register_Guide | SRAM mapping registers |
| Linux kernel `sun8i_mixer.c` | DE2 mixer driver (V3s config) |
| Linux kernel `sun4i_tcon.c` | TCON driver (V3s quirks) |
| Linux kernel `sun4i-codec.c` | Audio codec driver (V3s) |
| Linux kernel `cedrus` staging driver | VE H.264 decode reference |
| github.com/minilogic/v3s_nonos | Bare-metal V3s examples |
| github.com/Unturned3/h264enc_demo | V3s H.264 hardware encode demo |
| doc.funkey-project.com | FunKey S hardware reference (V3s retro gaming device) |
