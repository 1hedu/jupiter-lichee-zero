# V3s Hardware Status — Confirmed on Silicon

## Working Right Now

| Feature | Status | Notes |
|---------|--------|-------|
| H.264 encode | ~5ms, 5-8:1 compression | AVC engine +0xB00, Bootlin port |
| H.264 decode | ~5ms | H264 engine +0x200, cedrus register sequence |
| Encode→decode round-trip | Working | Asset compression pipeline |
| Genesis VDP renderer | Working | 4bpp, 2 planes + sprites, 320×224 |
| NES PPU renderer | Working | 2bpp, OAM, attribute table, 256×224 |
| GB/GBC PPU renderer | Working | 2bpp, DMG + GBC palette modes, 160×144 |
| SNES PPU renderer | Working | 2/4/8bpp, Modes 0-6, sprite system, 256×224 |
| NEON SIMD | Working | ~300M pixels/sec, used for all compositing |
| DE2 mixer | Working | VI0 (opaque + scaler), UI0 (alpha), VI1 (PIP) |
| DMA audio codec | Working | 48kHz stereo, ring buffer, ISR refill |
| VFPv4 FPU | Working | Mode 7 affine transforms, sin/cos |
| Vblank sync | Working | TCON0_GINT0 bit 15 polling |
| HSTimer mid-frame IRQ | Working | Scanline-accurate ISR via GIC SPI 51 (IRQ 83) |
| HDMA work distribution | Working | Audio+logic in mid-frame ISR, reduces vblank pressure by ~1ms |
| CPU at 1.2 GHz | Working | PLL_CPUX bump from 1.008 GHz default |

## Tricks Still Untapped

### DE2 Scaler Abuse

VI0 has a hardware scaler — the only layer with one on V3s. Currently rendering at native res and centering manually. Configure the VI0 scaler to go from 256×224 → 480×272 (or any native res → panel) and get free hardware upscaling. Set it once, forget it. Could also switch scaling coefficients per-frame to toggle between sharp pixels and a softer filtered look without touching the CPU.

### DMA Descriptor Chains

Instead of single DMA transfers for audio, set up linked descriptor rings that continuously cycle buffers. The CPU just writes to the "next" buffer and flips a pointer — DMA handles the rest autonomously. Reduces ISR overhead.

### H.264 Encode for Save States

Snapshot entire game state (VRAM + RAM + registers) as a raw image, hardware encode it, write the compressed blob to SD. Restore by decoding. At 8:1 ratio, a full 480×272 framebuffer save state is ~60KB instead of ~500KB.

### H.264 Encode for Video Capture

Encode every frame (or every Nth frame) as an I-frame, stream to SD as raw H.264 NAL units. Wrap with a simple container later on PC. The encode path runs at ~5ms — fits in a 60fps budget if you alternate encode/render across frames.

### CRT Simulation

Three approaches, from simple to full:

**1. DE2 scaler trick (free).** Render at half vertical res (256×112 for NES), let VI0 hardware scaler upscale to 480×272 with nearest-neighbor. Every row doubles naturally — instant scanlines, zero CPU cost.

**2. NEON runtime filter.** Stripped-down [ffmpeg-crt-transform](https://github.com/viler-int10h/ffmpeg-crt-transform) style pipeline in NEON:
- Scanline darkening: multiply every other row by 0.6 (8 pixels/instruction)
- Phosphor shadow mask: tile a 3-pixel RGB pattern (lookup)
- Bloom: horizontal 3-tap blur on bright pixels
- Curvature: barrel distortion via per-scanline LUT (same math as Mode 7)
- Skip the heavy stuff (halation, beam profile, analog noise)
- NEON at 300M pixels/sec can handle this at native res

**3. Offline bake (zero runtime cost).** Run the full ffmpeg-crt-transform on the PC, pre-process backgrounds before H.264 encoding. The CRT look gets baked into the compressed asset. VE decodes a CRT-filtered background directly — the retro aesthetic is in the data, not the renderer.

### VE Polyphase Scaler

Also available inside the VE decode output path at +0xA00. 4-tap, 16-phase filter — same class as MiSTer FPGA scalers. Could do hardware CRT scanlines on decoded frames by programming alternating phases to full/reduced brightness. But requires routing frames through encode→decode (10ms overhead per frame), so the DE2 or NEON approaches above are more practical.

### HSTimer for Raster Effects — PARTIALLY TAPPED

HSTimer is now working for mid-frame ISRs (HDMA work distribution confirmed). However, DE2 raster effects (changing BLD_BKCOLOR per-scanline) do NOT work — DE2 is double-buffered, register changes only take effect at vblank. Raster-style effects must be done in software (NEON row fills, per-scanline framebuffer writes).

### Crypto Engine as Fast XOR/PRNG

Hardware AES/SHA at 0x01C15000. AES-CTR mode is essentially a fast XOR engine. The hardware PRNG generates high-quality random numbers at hardware speed — useful for procedural generation without burning CPU cycles.

## What Doesn't Work / Doesn't Exist

| Feature | Status | Notes |
|---------|--------|-------|
| MPEG decode (+0x100) | Does not exist | Entire register space dark on V3s |
| JPEG hardware decode | Impossible | Requires MPEG engine |
| G2D (2D graphics) | Does not exist | Confirmed absent by FunKey Project |
| SRAM_C as CPU scratchpad | Not viable | 90 cycles/access through VE bus bridge |
| VC1 decode (+0x300) | Not present | |
| HEVC decode (+0x500) | Not present | |
| AVC encoder (+0xB00) scan from Linux | Blocked | Kernel CONFIG_IO_STRICT_DEVMEM |
