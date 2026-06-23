# Jupiter Mars — Pico Polygon Coprocessor

A Raspberry Pi Pico (RP2040) that acts as a 3D polygon rendering coprocessor for the Jupiter SDK, outputting frames over parallel CSI to the Allwinner V3s. The Pico is to Jupiter what the 32X (codenamed Mars) was to the Genesis.

## Architecture

```
V3s (Jupiter)                          Pico (Mars)
┌──────────────┐    SPI (25MHz)    ┌──────────────┐
│  Game logic  │ ───────────────── │  SPI slave   │
│  2D VDP/PPU  │   display list    │              │
│  Audio/DMA   │                   │  Core 0:     │
│              │                   │   Rasterizer  │
│  CSI capture │ ◄──── parallel ── │  Core 1:     │
│  DE2 mixer   │   CSI (8-bit)     │   PIO scanout │
│  LCD out     │                   │              │
└──────────────┘                   └──────────────┘

DE2 Layer Stack:
  VI0  ← Pico 3D layer (captured via CSI)
  UI0  ← V3s 2D sprites (alpha-keyed ARGB8888)
  VI1  ← V3s 2D background tilemap
```

The V3s sends a display list over SPI each frame (~2KB of triangle/quad commands). The Pico rasterizes flat-shaded polygons into a 320×224 R3G3B2 framebuffer, then PIO streams it as parallel CSI. The V3s CSI controller DMA-captures the frame into DRAM, where DE2 composites it with the 2D retro layers.

## Wiring

```
Pico GPIO    Signal      V3s Pin (CSI0)
─────────    ──────      ──────────────
GP0-GP7      D[0:7]      PE4-PE11 (CSI_D0-D7)
GP8          PCLK        PE0 (CSI_PCLK)
GP9          HREF        PE2 (CSI_HSYNC)
GP10         VSYNC       PE3 (CSI_VSYNC)
GP11         SPI MOSI    PB0 (SPI0_MOSI) or any SPI master pin
GP12         SPI SCK     PB1 (SPI0_CLK)
GP13         SPI CSn     PB2 (SPI0_CS)
GND          GND         GND

Power: Pico VSYS from V3s 5V, or separate USB power.
Both boards share a common ground.
```

## Pin Rationale

- **GP0-GP7 for data**: Consecutive GPIO bank, optimal for PIO `out pins, 8`
- **GP8 for PCLK**: Adjacent to data, used as PIO side-set or set pin
- **GP9-GP10 for sync**: CPU-driven GPIO, timing isn't cycle-critical
- **GP11-GP14 for SPI**: Hardware SPI1 slave on these pins

## Display List Protocol

The V3s sends display lists over SPI with this framing:

```
Byte 0-1:  Payload length (uint16_t LE)
Byte 2+:   Command stream

Commands:
  0x01 COLOR         — clear framebuffer (2 bytes: cmd, color)
  0x02 TRI           — flat triangle (14 bytes: cmd, color, 3× vertex)
  0x04 QUAD          — flat quad (18 bytes: cmd, color, 4× vertex)
  0x05 LINE          — line segment (10 bytes: cmd, color, 2× vertex)
  0x11 TEXTURE       — upload texture (4+N bytes: cmd, slot, size, data)
  0xFF SCENE_END     — swap buffers and display
```

Each vertex is `{int16_t x, int16_t y}` — screen coordinates. The Pico doesn't do 3D projection. The V3s transforms world coordinates to screen space and sends projected 2D vertices. This keeps the Pico code simple and the display list compact.

For 3D scenes, the V3s does:
```c
// V3s side — build display list
for each polygon:
    project vertices to screen coords (VFPv4 FPU)
    backface cull
    append CMD_TRI with 2D screen vertices
send display list over SPI
```

## Pixel Format: R3G3B2

8-bit color, 256 possible values. One byte per pixel.

```
Bit 7  6  5  4  3  2  1  0
    R  R  R  G  G  G  B  B
```

On the V3s side, the CSI captures raw 8-bit data. To display through DE2 (which expects ARGB8888 or RGB565), you either:
- Expand R3G3B2 → ARGB8888 in software (NEON-fast, 16 pixels per pass)
- Configure DE2 VI channel for 8-bit indexed with a 256-entry CLUT if available

## Self-Test Mode

When no SPI master is connected, the Pico renders a spinning flat-shaded cube at 60fps. This lets you verify the CSI output works without the V3s connected — just hook the Pico's output to any parallel camera receiver, or probe the GPIO signals with a logic analyzer.

The cube uses painter's algorithm (back-to-front face sorting) and integer-only rotation math with a 256-entry sine table.

## Building

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cd jupiter-pico
mkdir build && cd build
cmake ..
make
```

Flash `jupiter_mars.uf2` to the Pico via USB drag-and-drop.

## V3s Integration

On the V3s Jupiter SDK side, add `v3s_side/mars_csi_capture.h` to your build:

```c
#include "mars_csi_capture.h"

// During init:
csi_clocks_init();
csi_gpio_init();
csi_capture_init(MARS_CAPTURE_BUF);  // physical DRAM address

// In your main loop:
if (csi_frame_ready()) {
    // New frame from Pico is in MARS_CAPTURE_BUF
    // Convert R3G3B2 → ARGB8888 and map to DE2 VI0
    mars_r3g3b2_to_argb(vi0_fb, MARS_CAPTURE_BUF, 320, 224);
    dcache_clean_range(vi0_fb, 320*224*4);
}

// Send display list to Pico:
mars_spi_send(display_list_buf, display_list_len);
```

## File Map

```
jupiter-pico/
├── CMakeLists.txt              Pico SDK build config
├── README.md                   This file
├── include/
│   └── jupiter32x.h            Constants, types, display list format
├── pio/
│   └── csi_out.pio             PIO programs for pixel + sync output
├── src/
│   ├── mars_main.c             Core 0 main: SPI recv + rasterize + self-test cube
│   ├── mars_raster.c           Triangle/line/hline rasterizer, double-buffer swap
│   ├── mars_displaylist.c      Display list command parser and dispatch
│   ├── mars_spi.c              SPI slave receiver (hardware SPI1 + DMA)
│   └── csi_out.c               Core 1: PIO + DMA scanout loop
└── v3s_side/
    └── mars_csi_capture.h      V3s CSI capture driver (add to Jupiter SDK build)
```

## Performance

At 133MHz dual Cortex-M0+:
- **Clear**: ~0.3ms (memset 70KB)
- **Triangle fill**: ~2μs per triangle (average 50-pixel triangle)
- **Frame budget**: ~16.6ms at 60fps
- **Triangle budget**: ~200-500 triangles per frame depending on size
- **SPI receive**: ~0.6ms for 2KB display list at 25MHz
- **CSI scanout**: continuous on Core 1, zero Core 0 cost

This is Star Fox / Virtua Racing class geometry. Flat-shaded, 50-200 polygons per frame, painter's algorithm sorting.

## Why "Mars"

The Sega 32X was codenamed **Mars**. It was a coprocessor that bolted onto the Genesis, adding a polygon-capable layer that the Genesis VDP couldn't render. The two SH-2 processors in the 32X did 3D rendering, the Genesis VDP continued doing 2D tiles and sprites, and the outputs were composited together.

Jupiter Mars does the same thing: a $4 coprocessor bolted onto the $6 main system, adding 3D polygon capability to a 2D retro SDK. Two processors, asymmetric architecture, layered output through a hardware compositor.

Mars → Jupiter. Same orbital path, more mass.

## Status / TODO

### Working
- Pico-side rasterizer (flat tri, line, hline, clear, double-buffer)
- PIO pixel scanout with DMA (Core 1)
- Display list parser (CLEAR, TRI, QUAD, LINE, SCENE_END)
- Self-test spinning cube (runs when no SPI master connected)
- SPI slave receive (non-blocking)

### Needs Work
- **MIPI CSI-2 PHY configuration on V3s** — `csi_capture_init()` sets up the CSI controller (buffer, size, interrupts) but does NOT configure the MIPI PHY (lane count, data rate, protocol layer). Need a Linux register dump with a real MIPI camera to get the exact PHY register sequence. Same approach used for audio codec and CedarVE.
- **DVP-to-MIPI bridge chip selection and I2C config** — the bridge (e.g. TC358748) needs I2C register programming at init. Pin mapping, timing, and power sequencing TBD once hardware is in hand.
- **Texture rendering** — CMD_TEXTURE uploads data but no rendering command references texture slots. Need CMD_TEXTRI or similar.
- **R3G3B2→ARGB8888 NEON version** — `mars_r3g3b2_to_argb()` is scalar C. A NEON version could process 16 pixels per pass.
