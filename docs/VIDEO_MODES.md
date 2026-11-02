# Jupiter Modes — hardware-native display configurations

## PINE A64+ port

The sections below record the original V3s design and Lichee Pi Zero bench
results. On PINE A64+, mixer0 has **VI0 + UI0 + UI1 + UI2**, so the port maps
the RGB `video_vi1_init()` window and Mode 5 bottom viewport to the real
**UI1** channel. It preserves the inherited HDMI timing and centers a
480×272 canvas; it does not program V3s VI1 registers on A64.

Mode 3 selects combined pixel/global alpha. Mode 4 uses A64 VI0's NV12
format, CSC at mixer0 + `0xAA050`, and the VSU to upsample the half-resolution
chroma plane with 20-bit scale steps. Mode 5 uses independent VI0/UI1
half-height windows, with UI0 as the full-canvas HUD. The mode-4 test still
needs the two displayed squares compared on physical hardware.

A64 has only HSTimer0. Modes 6 and 7 use it in separate phases with the
measured native HDMI line period. Mode 7 reads TCON1's live scanline and
back-porch timing, then delays its first band to the centered canvas start;
this includes blanking and the large top border at 720p/1080p. Its later
bands use the same timer in repeating mode. Physical IRQ/latch timing and
mid-frame LADDR behavior remain board tests, not an emulation result. The A64
DE2 probe keeps the latest 13 colorkey configurations, uses UI1 as the key
and animation plane, and tests four subwindows on **VI0**. The V3s success
reports below do not prove the corresponding A64 capabilities.

Register mapping references:
[Linux v6.18 mixer configuration](https://github.com/torvalds/linux/blob/v6.18/drivers/gpu/drm/sun4i/sun8i_mixer.c),
[VI scaler](https://github.com/torvalds/linux/blob/v6.18/drivers/gpu/drm/sun4i/sun8i_vi_scaler.c),
[CSC layout](https://github.com/torvalds/linux/blob/v6.18/drivers/gpu/drm/sun4i/sun8i_csc.h).

## Original V3s mode design

An SNES homage: numbered modes, each a fixed **DE2 mixer layer stack**.
The difference from the SNES is what "mode" means here — these are
*scanout* configurations. Nothing in a Jupiter Mode touches a pixel
with the CPU; the DE2 blender composites, positions, fades, and (in
Mode 4) color-converts during scan.

The software PPU renderers (`lib/nes.c`, `lib/snes.c`, …) are the
*content* layer and run inside whatever Jupiter Mode you pick — they
draw into a VI0/UI0 buffer, the mode decides how the hardware presents
it.

Call `video_mode(n)` any time after `video_init()`. `video_init()`
itself leaves you in Mode 1.

| Mode | Name     | Layer stack (bottom → top)         | Hardware doing the work |
|------|----------|------------------------------------|-------------------------|
| 0    | FLAT     | VI0                                | single scanout, cheapest possible |
| 1    | BG+OBJ   | VI0 + UI0                          | per-pixel alpha blend |
| 2    | TRIPLANE | VI0 + UI0 + VI1 window             | 3 planes, VI1 positioned by the blender (`video_vi1_init`) |
| 3    | GHOST    | VI0 + UI0 (global alpha)           | whole-overlay hardware fade: `video_mode3_alpha(0..255)` |
| 4    | CINEMA   | VI0 **NV12** window + UI0          | YUV→RGB CSC during scan; Cedar decode → glass, zero CPU pixels |
| 5    | SPLIT    | VI0 top half + VI1 bottom half + UI0 | two independent hardware viewports (`video_mode5_split`) — 2-player split with zero CPU compositing |
| 6    | RASTER   | Mode 1 + hstimer scanline hook     | per-line register pokes (color/alpha splits) — see `hstimer.h` and the `hstimer_raster` example |
| 7    | AFFINE   | Mode 1 + hstimer lineshift         | hardware per-band scan-address shifts (`video_mode7_lineshift`) + NEON texture sampling (`mode7_scanline`) |

## Mode 7 — hardware where hardware wins

The V3s display path cannot resample pixels: the VSU is fused off
(`scaler_probe` proves it) and the DE2 has no rotation unit, so the
texture *sampling* of an affine floor is NEON (`mode7_scanline` /
`iso_scanline`) no matter what. But the other half of the SNES Mode 7
trick — changing parameters **per scanline** — is exactly what the
hardware here is good at: `video_mode7_lineshift()` arms an hstimer
scanline ISR that re-points VI0's scan address every N lines with the
mid-frame `GLB_DBUFF` latch (the register+latch technique
`hstimer_raster` verified on silicon for the backdrop color). That
gives full-screen line-shear, waves, and horizon splits for one
register write per band and **zero** CPU pixel work. Mid-frame LADDR
latching specifically is unverified on silicon — the `jupiter_modes`
tour's Mode 7 phase is its bring-up test (the rotozoom sways without
any change to the rendered frame if it works; static if not).

## Mode 4 CINEMA in one paragraph

The V3s VI channels scan YUV natively (DE2 format 8 = NV12) and each
channel has a color-space converter in front of the blender
(BT601 limited-range coefficients, register layout per Linux
`sun8i_csc.c`: 12 coefficient words at CSC base +0x10, enable bit 0,
channel-0 CSC at mixer +0xAA050). The Cedar H.264 decoder already
produces NV12 — so a video player becomes:

```c
cedar_h264_decode(nal, len, w, h, ...);
video_mode4_nv12(cedar_dec_luma_addr(), cedar_dec_chroma_addr(),
                 w, h, /*stride=*/(w + 15) & ~15,
                 (LCD_W - w) / 2, (LCD_H - h) / 2);
```

No `cedar_nv12_to_argb`, no framebuffer copy — the ~2.5 ms/frame the
480×272 ARGB conversion used to cost drops to zero. UI0 stays ARGB on
top for subtitles/HUD. `video_mode4_off()` restores normal XRGB
scanout. There is no scaler on V3s silicon (the VSU is fused off — see
`scaler_probe`), so the frame displays 1:1 in a positioned window.

## Open hardware questions — `examples/de2_probe`

Three DE2 capabilities that exist in the register map but that Linux
never drives and no public doc explains. **Round-1 silicon results are
in** (Lichee Pi Zero bench):

1. **Blender colorkey — LIVE, polarity being pinned down.** The
   CK_CTL/CK_CFG/CK_MAX/CK_MIN registers (BLD +0xB0/+0xB4/+0xC0/+0xE0)
   are functional: with CTL=0x03/0x07 the comparator matched an exact
   MIN=MAX magenta key, but inverted from the magic-pink convention —
   pixels MATCHING the key kept the top layer while everything else
   went transparent to the layer below ("key = the opaque set"). The
   current probe has 13 round-3 configs (0–12), testing regression
   baselines, RGB window exclusion and CFG channel/inversion guesses.
   Config 3 excludes blue=255 from the accepted key window. The
   moment the magenta field itself punches through: hardware magic
   pink, and Mode 5 grows a dual-full-playfield variant. Even the
   inverse mode as-is is usable (single-color hardware masks/cutouts).
2. **VI sub-windows — CONFIRMED.** All four overlay slots per VI
   channel (0x30-stride register groups) render — four staggered
   colored squares on the bench. Up to four hardware rects per VI
   channel, eight across VI0+VI1, where Linux only ever uses one.
3. **Window animation — CONFIRMED.** VI1's size and position
   re-programmed every vblank produce a smooth breathing, orbiting
   box with no tearing. Hardware window tweens, iris wipes, and
   zoom-boxes are free.

## Verification status

Modes 0–3 rearrange only registers that every shipped example already
exercises (routes, pipe enables, global alpha) — low risk, but the
mode *switch* path itself hasn't run on silicon yet. Mode 4 uses the
NV12 format + CSC path that nothing in the SDK has driven before:
**unverified on silicon** (and the QEMU display model is ARGB-only, so
the emulator will show garbage for it — that's expected). The
`mode4_cinema` example is the bring-up test: it decodes the known-good
64×64 H.264 probe frame and scans it out both ways, software ARGB
left, hardware NV12 right, so a working CSC shows two identical
squares.
