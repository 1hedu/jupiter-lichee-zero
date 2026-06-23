# CedarVE Hardware Video Engine + Retro Graphics Pipelines

## Jupiter SDK — Allwinner V3s / Lichee Pi Zero

This document covers the complete bare-metal CedarVE H.264 encode/decode pipeline and the four retro system graphics renderers built on top of it. Everything runs bare-metal on the Allwinner V3s (Cortex-A7, 64MB DDR2, 480×272 LCD).

---

## Part 1: CedarVE — What Nobody Documented

### The V3s Video Engine

The V3s has a hardware video engine (VE) at `0x01C0E000`. VE_VERSION = `0x16810040` (version 0x1681). It supports:

- **H.264 decode** (+0x200) — confirmed working
- **H.264/MJPEG encode** (+0xB00 AVC encoder) — confirmed working
- **MPEG decode** (+0x100) — **DOES NOT EXIST on V3s** (register space is dark)

The cedrus kernel driver claims only `CEDRUS_CAPABILITY_H264_DEC | CEDRUS_CAPABILITY_UNTILED` for V3s. No MPEG, no JPEG decode. The research doc's "JPEG hardware decode" trick is impossible on V3s — the MPEG engine at +0x100 was cut from this silicon.

### Critical Init: SRAM_CTRL_REG0

The single most important discovery. Without this one register write, the VE cannot DMA to/from DRAM. **Nothing in the mainline kernel documents this.**

```c
// SRAM_CTRL_REG0 at 0x01C00000 — maps SRAM sections to bus masters
uint32_t val = REG32(0x01C00000);
val &= 0x80000000;    // keep bit 31
REG32(0x01C00000) = val;
val = REG32(0x01C00000);
val |= 0x7FFFFFFF;    // set ALL bits 0-30
REG32(0x01C00000) = val;

// SRAM_C to VE
REG32(0x01C00004) &= ~(1u << 24);
```

Source: Allwinner BSP `cedar_ve.c` (linux-3.4 kernel, `v3s_lichee` SDK). The mainline `sunxi_sram_claim()` only handles `REG1` (0x01C00004). The BSP writes to BOTH `REG0` and `REG1`.

Without `REG0 = 0x7FFFFFFF`, the VLD (Variable Length Decoder) reads all zeros from DRAM. `BASIC_BITS = 0x00000000`. The VE's DMA path through MBUS is dead.

### PLL_VE Is at CCU+0x018, NOT 0x058

We spent hours writing to CCU+0x058 (which is actually `AHB2_CFG`). The real PLL_VE:

```c
// CCU+0x018: PLL_VE — bit31=enable, bit28=lock, [15:8]=N, [3:0]=M
// 402 MHz: N=67, M=3 → 24*67/4 = 402
uint32_t pll = REG32(CCU_BASE + 0x018);
pll &= ~(0xFF << 8);  // clear N
pll &= ~(0xF << 0);   // clear M
pll |= (67 << 8) | (3 << 0) | (1u << 31);
REG32(CCU_BASE + 0x018) = pll;
while (!(REG32(CCU_BASE + 0x018) & (1u << 28)));  // wait lock
```

Confirmed by Linux `/dev/mem` register dump comparing against known CCU layout.

### Clock and Reset Ordering

The Allwinner BSP (aodzip/cedar) does:

```
1. SRAM claim
2. clk_set_rate(mod_clk, 402MHz)
3. clk_prepare_enable(ahb_clk)     ← ALL clocks ON
4. clk_prepare_enable(mod_clk)
5. clk_prepare_enable(ram_clk)
6. reset_control_reset(rstc)       ← THEN reset
```

All clocks must be running before the VE comes out of reset. Sub-engine registers at +0x100/+0x200 are only accessible when VE_CLK is running during reset de-assert.

### Complete VE Init Sequence

```c
void cedar_init(void) {
    // 1. SRAM mapping (THE critical step)
    REG32(0x01C00000) = (REG32(0x01C00000) & 0x80000000) | 0x7FFFFFFF;
    REG32(0x01C00004) &= ~(1u << 24);  // SRAM_C to VE

    // 2. PLL_VE at CCU+0x018 → 402 MHz
    // 3. Bus gate (CCU+0x060 bit 0)
    // 4. VE module clock (CCU+0x13C)
    // 5. DRAM clock gate (CCU+0x100 bit 0)
    // 6. Reset cycle (CCU+0x2C0 bit 0: assert, wait, deassert)
    // 7. VE_CTRL = 0x00130007 (park idle)
}
```

---

## Part 2: H.264 Decode

### Register Sequence (from cedrus_h264.c)

The decoder uses the H.264 engine at +0x200. Key steps:

1. `VE_MODE = 0x00130001` (select H264, DDR 128-bit, 2MB write mode)
2. Set NV12 output format (`VE_PRIMARY_OUT_FMT = 0x04 << 4`)
3. Set stride (`VE_PRIMARY_FB_LINE_STRIDE`)
4. Write frame buffer list to SRAM (output luma/chroma addresses)
5. Set extra buffers (pic_info, neighbor_info)
6. VLD bitstream setup (ADDR with VAL encoding, LEN, END, OFFSET)
7. INIT_SWDEC trigger → skip header bits via FLUSH_BITS
8. Set SPS/PPS/SHS/QP registers
9. Trigger `AVC_SLICE_DECODE`
10. Poll status for `SLICE_OK`

### VLD Address Encoding

```c
#define VLD_ADDR_VAL(x) (((x) & 0x0FFFFFF0) | ((x) >> 28))
// Physical 0x43000000 → VAL = 0x03000004
// With flags: 0x03000004 | 0x70000000 = 0x73000004
```

The top nibble of the physical address is swizzled into bits [3:0]. Flags in bits [30:28]: FIRST, LAST, VALID.

---

## Part 3: H.264 Encode

### The Encoder Breakthrough

The AVC encoder at +0xB00 required a complete rewrite based on Bootlin's `cedrus_enc_h264.c` (1588 lines). Key discoveries:

**ISP Enable (VE_MODE bit 6):** Without this, the encoder can't read input frames. The ISP (Image Signal Processor) is a separate enable from the encoder (bit 7).

```c
VE(0x000) |= (1u << 7) | (1u << 6) | 0x07;  // enc + ISP + dec_disabled
```

**Encoder Reset (VE_RESET_REG, offset 0x04, bit 24):** Separate from the global VE reset.

```c
VE(0x004) |= (1u << 24);   // assert encoder reset
VE(0x004) &= ~(1u << 24);  // deassert
```

**Missing Buffers:**
- `MB_INFO` (0xBC0) — `DIV_ROUND_UP(width_mbs, 32) * 4KB`
- `SUBPIX` (0xBB8, 0xBBC) — subpixel interpolation buffers
- `REC` (0xBB0, 0xBB4) — reconstruction buffers with specific alignment

**put_bits with Polling:** Must wait for `STATUS bit 9 (PUT_BITS_READY)` before each write:

```c
while (!(VE(0xB1C) & (1u << 9)));  // poll PUT_BITS_READY
VE(0xB20) = value;                  // PUTBITSDATA
VE(0xB18) = (nbits << 8) | 1;      // STARTTRIG: PUT_BITS
```

**EPTB (Emulation Prevention Three-Byte):** Must be disabled during NAL header writes, enabled for macroblock data. Controlled via `PARA0` bit 31.

**Sync Idle:** After writing headers, poll `VE_RESET_REG` bits 8-9 for sync idle before launching encode.

### Exp-Golomb Bitstream Writing

SPS, PPS, and slice headers are written via put_bits using proper H.264 exp-golomb coding:

```c
// ue(v): unsigned exp-golomb
int bits = 2 * fls(v + 1) + 1;
put_bits(v + 1, bits);

// se(v): signed exp-golomb
uint32_t ue_v = (v > 0) ? (2*v - 1) : (-2*v);
put_ue(ue_v);
```

### Buffer Overlap Bug

The encoder output buffer and decoder input buffer must NOT be the same address. The decoder's `cedar_h264_decode` does `memset → memcpy` on `BUF_INPUT`, which destroys the encoder output if they overlap.

### Encode Performance

| Image | Raw ARGB | Encoded | Ratio |
|-------|----------|---------|-------|
| Pulseman 512×480 | 960KB | ~180KB | 5.3:1 |
| FF6 Soldier 448×928 | 1624KB | 202KB | 8:1 |

---

## Part 4: Retro System Renderers

### Architecture

All four renderers follow the same DE2 layer mapping:

| DE2 Layer | Purpose | Format |
|-----------|---------|--------|
| VI0 | Background (opaque) | XRGB8888 |
| UI0 | Sprites (alpha-keyed) | ARGB8888 |
| VI1 | Sprite sheet visualizer | XRGB8888 |

All renderers support "authentic mode" — centered native resolution with black pillarbox. Vblank-synced via `video_wait_vblank()` (TCON0_GINT0 bit 15 polling).

### Genesis VDP (`lib/genesis.c`, `include/genesis.h`)

- **Resolution:** 320×224
- **Tiles:** 4bpp, 32 bytes/tile
- **Layers:** Plane B (VI0) + Plane A (UI0) + Window + Sprites
- **Palette:** 4 palettes × 16 colors = 64 CRAM entries (ARGB8888)
- **Sprites:** Column-major tile layout (`tile + tx * sprite.h + ty`)
- **Metasprite:** Grid of 4×4-tile hardware sprites, stored per-chunk with chunk-local stride

### NES PPU (`lib/nes.c`, `include/nes.h`)

- **Resolution:** 256×224 (safe area)
- **Tiles:** 2bpp, 16 bytes/tile (two bitplanes)
- **Background:** 32×30 nametable + 64-byte attribute table (2 bits per 16×16 region)
- **Palette:** 4 BG + 4 sprite palettes, NES master palette (64 RGB colors)
- **Sprites:** 64 OAM entries, 8 per scanline limit, behind-BG priority
- **Attribute table:** `shift = ((ty & 2) << 1) | (tx & 2)`

### Game Boy / GBC (`lib/gb.c`, `include/gb.h`)

- **Resolution:** 160×144
- **Tiles:** 2bpp, 16 bytes/tile (same layout as NES)
- **Background:** 32×32 tilemap, GBC per-tile attributes (flip, palette, bank)
- **Sprites:** 40 OAM entries, 10 per scanline limit
- **Palette:** GBC: 8 palettes × 4 ARGB colors. DMG: single 4-shade palette.

### SNES PPU (`lib/snes.c`, `include/snes.h`)

- **Resolution:** 256×224
- **Tiles:** 2/4/8 bpp
- **Modes:** 0-6 background compositors + Mode 7 (NEON ASM)
- **Sprites:** 128 OAM entries, 32 per scanline, 4bpp, 8 palettes × 16 colors
- **Tilemap entry:** `[9:0]=tile, [12:10]=palette, [13]=priority, [14]=flipX, [15]=flipY`

### SNES 4bpp Tile Format

```
Bytes [0..15]:  bitplanes 0-1 interleaved (row0_bp0, row0_bp1, row1_bp0, ...)
Bytes [16..31]: bitplanes 2-3 interleaved

Pixel decode:
  bit0 = (tile[row*2]     >> (7-col)) & 1
  bit1 = (tile[row*2+1]   >> (7-col)) & 1
  bit2 = (tile[row*2+16]  >> (7-col)) & 1
  bit3 = (tile[row*2+17]  >> (7-col)) & 1
  color = (bit3<<3) | (bit2<<2) | (bit1<<1) | bit0
```

---

## Part 5: The Sprite Sheet Pipeline

### End-to-End Flow

```
Spriters Resource PNG
        ↓
ffmpeg -pix_fmt bgra -f rawvideo → raw ARGB (embedded via objcopy)
        ↓
[On V3s at boot]
        ↓
ARGB → NV12 (software BT.601 conversion)
        ↓
CedarVE H.264 ENCODE (AVC engine, hardware, ~5ms)
        ↓
CedarVE H.264 DECODE (H264 engine, hardware, ~5ms)
        ↓
NV12 → ARGB (software conversion)
        ↓
Palette extraction (find unique non-background colors)
        ↓
3× nearest-neighbor scale
        ↓
Convert to native tile format (2bpp NES/GB or 4bpp Genesis/SNES)
        ↓
Build metasprite (grid of hardware sprites, column-major tiles)
        ↓
Render through system PPU → DE2 layers → LCD
```

### Metasprite Tile Storage

Genesis/SNES sprites index tiles as `base + tx * sprite.h + ty` (column-major). When splitting a large tile grid into 4×4-tile sprite chunks, tiles must be stored **per chunk** with chunk-local stride, NOT in one big grid with global stride:

```
WRONG: tiles stored with grid stride (th=21)
       renderer reads with chunk stride (ch=4) → garbled

RIGHT: each 4×4 chunk gets its own contiguous tile block
       stride within chunk matches sprite.h
```

### Pixel Format: BGRA for Little-Endian ARM

ffmpeg's `-pix_fmt rgba` outputs R,G,B,A byte order. On little-endian ARM, `uint32_t` reads this as `0xAABBGGRR` — R and B are swapped from expected `0xAARRGGBB`. Use `-pix_fmt bgra` instead:

```bash
ffmpeg -i sprite.png -vf "pad=W:H:0:0:color=0x00FF00" \
  -pix_fmt bgra -f rawvideo sprite.argb
```

### Pre-Conversion for Small Sheets

For small sprite sheets (<50KB), H.264 adds overhead. Pre-convert to native tile format on the PC instead:

```python
# NES 2bpp: 16 bytes/tile
# SNES 4bpp: 32 bytes/tile
# Embed as C header array
```

---

## Part 6: V3s Hardware Findings

### SRAM_C at 0x01D00000

- **NOT viable as CPU scratchpad.** 90 cycles/access through VE bus bridge vs 3-5 for cached DDR.
- Only ~52KB usable (pages 3-15 of 4KB each), pages 1-2 corrupted.
- The "zero-wait-state SRAM" claim applies from VE's internal bus, not CPU access.

### DE2 Display Engine

- VI0: opaque background, **only layer with hardware scaler** on V3s
- UI0: alpha-blended overlay (ARGB8888)
- VI1: positioned opaque rectangle (PIP)
- `video_wait_vblank()`: poll TCON0_GINT0 (offset 0x04) bit 15 (w1c)

### Key Register Addresses

| Register | Address | Purpose |
|----------|---------|---------|
| SRAM_CTRL_REG0 | 0x01C00000 | Bus master SRAM access (write 0x7FFFFFFF) |
| SRAM_CTRL_REG1 | 0x01C00004 | SRAM_C mapping (bit 24: 0=VE, 1=CPU) |
| PLL_VE | CCU+0x018 | VE clock PLL (NOT 0x058!) |
| VE_CLK | CCU+0x13C | VE module clock gate |
| DRAM_GATE | CCU+0x100 | VE DRAM access gate (bit 0) |
| BUS_GATE | CCU+0x060 | VE bus clock gate (bit 0) |
| BUS_RST | CCU+0x2C0 | VE reset control (bit 0) |
| VE_BASE | 0x01C0E000 | Video Engine registers |
| VE_RESET_REG | VE+0x004 | Encoder reset (bit 24) |
| VE_MODE | VE+0x000 | Engine select + encoder/ISP enable |

### What Doesn't Exist on V3s

- MPEG decode engine (+0x100) — entire register space dark
- JPEG hardware decode — requires MPEG engine
- VC1 decode (+0x300) — not present
- HEVC decode (+0x500) — not present
- G2D (2D graphics engine) — confirmed absent by FunKey Project

---

## Part 7: Reference Code

| File | Purpose |
|------|---------|
| `lib/cedar.c` | VE init + H.264 decode |
| `lib/cedar_enc.c` | H.264 encode (Bootlin port) |
| `lib/genesis.c` | Genesis VDP renderer |
| `lib/nes.c` | NES PPU renderer |
| `lib/gb.c` | Game Boy / GBC renderer |
| `lib/snes.c` | SNES PPU renderer + sprite system |
| `examples/cedar_genesis/` | Pulseman metasprite + encode→decode |
| `examples/cedar_nes/` | Mendel Palace Vinci + NES PPU |
| `examples/cedar_gb/` | Pokemon Crystal Celebi + GBC PPU |
| `examples/cedar_snes/` | FF6 Magitek Soldier + encode→decode |
| `libcedarjpeg/` | Reference: cedrus driver, Bootlin encoder |
| `third_party/cedar_aodzip/` | Reference: aodzip cedar driver (V3s) |
| `tools/ve_probe.c` | Linux register dump utility |
| `tools/ve_full_test.c` | Linux VE diagnostic tool |

---

## Lessons Learned

1. **Read the BSP source first.** Both critical breakthroughs (SRAM_CTRL_REG0, encoder ISP enable) came from reading actual Allwinner/Bootlin source code, not from guessing at registers.

2. **The hardware doesn't care about documentation quality.** The V3s VE works exactly as the silicon implements it. Wiki pages, forum posts, and AI summaries are approximations. The kernel driver source is the ground truth.

3. **One missing register write can make an entire subsystem appear dead.** SRAM_CTRL_REG0 for decode, VE_MODE bit 6 for encode, VE_RESET_REG bit 24 for encoder reset — each was a single bit that gated an entire hardware path.

4. **Buffer addresses must not overlap between encode and decode.** The decoder's memset+memcpy destroys the encoder output if they share the same buffer address.

5. **ffmpeg pixel format on little-endian ARM:** Use `-pix_fmt bgra` (not `rgba`). RGBA byte order reads as `0xAABBGGRR` on LE, swapping red and blue. BGRA gives the correct `0xAARRGGBB`. Also use `-color_range pc` for full-range YUV (default limited range makes colors appear faded). Both are one-time settings, not ongoing concerns.

6. **For tiny sprite sheets (<50KB), skip H.264.** The encode/decode overhead and H.264 header size make compression counterproductive. Pre-convert to native tile format on the PC and embed directly (768 bytes for an 8-frame NES animation vs 89KB H.264).

7. **Metasprite tile storage must match the renderer's indexing.** Genesis/SNES sprites use `tile + tx * sprite.h + ty` (column-major). Split large tile grids into chunks with per-chunk stride, not global stride.
