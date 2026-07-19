# Jupiter Modes — hardware-native display configurations

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
| 5    | —        | *(reserved)*                       | second full playfield / split-screen, once colorkey is proven |
| 6    | RASTER   | Mode 1 + hstimer scanline hook     | per-line register pokes (line-scroll, splits) — see `hstimer.h` and the `hstimer_raster` example |
| 7    | —        | Mode 1                             | affine stays **software** on the V3s (`mode7_scanline`, NEON). The number is reserved out of respect. |

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
