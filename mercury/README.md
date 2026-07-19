# Mercury — Pico Polygon Coprocessor for Jupiter

A Raspberry Pi Pico (RP2040) that acts as a 3D polygon rendering coprocessor for the Jupiter SDK, streaming frames back to the Allwinner V3s through its MIPI CSI-2 camera input (via a bridge chip — see "Pico → V3s bridge chip" below). Mercury is to Jupiter what the 32X was to the Genesis — a small fast companion that adds a capability the host platform doesn't have on its own (in our case, real polygon hardware).

## Architecture

```
V3s (Jupiter)                          Pico (Mercury)
┌──────────────┐    SPI (25MHz)    ┌──────────────┐
│  Game logic  │ ───────────────── │  SPI slave   │
│  2D VDP/PPU  │   display list    │              │
│  Audio/DMA   │                   │  Core 0:     │
│              │                   │   Rasterizer  │
│  CSI capture │ ◄─ MIPI CSI-2 ─┐  │  Core 1:     │
│  DE2 mixer   │                │  │   scanout    │
│  LCD out     │        [bridge]◄──│  (DVI or DVP)│
└──────────────┘                   └──────────────┘

DE2 Layer Stack:
  VI0  ← Pico 3D layer (captured via CSI)
  UI0  ← V3s 2D sprites (alpha-keyed ARGB8888)
  VI1  ← V3s 2D background tilemap
```

The V3s sends a display list over SPI each frame (~2KB of triangle/quad commands). The Pico rasterizes flat-shaded polygons into a 320×224 R3G3B2 framebuffer and scans it out continuously on Core 1 — as DVI (default route) or as 8-bit parallel DVP (`-DMERCURY_ROUTE_DVP=ON`). A bridge chip converts either output to MIPI CSI-2; the V3s CSI controller DMA-captures the frame into DRAM, where DE2 composites it with the 2D retro layers.

> **Why a bridge is mandatory:** the V3s CSI controller on the Lichee Pi
> Zero is wired for **MIPI CSI-2** (dedicated PHY pins on the 2.54mm
> header). The parallel CSI pins live on port E, which this SDK already
> uses for the RGB LCD — a direct Pico→PE parallel hookup is not
> possible alongside the display.

## Wiring — DVI route (default)

```
Pico GPIO    Signal            Goes to
─────────    ──────            ───────
GP0/GP1      TMDS D2± (blue)   TC358743 HDMI input
GP2/GP3      TMDS D1± (green)      "
GP4/GP5      TMDS D0± (red)        "
GP6/GP7      TMDS CLK±             "
GP8          SPI1 RX (MOSI)    V3s SPI master MOSI
GP9          SPI1 CSn          V3s SPI master CS
GP10         SPI1 SCK          V3s SPI master SCK
GP11         SPI1 TX (MISO)    optional readback
GND          GND               common ground

Bridge → V3s: TC358743 CSI-2 output → Lichee Pi Zero MIPI CSI pins
(header pins 17/19 = D0±, 21/23 = D1±, 25/27 = CLK±).
```

## Wiring — DVP route (`-DMERCURY_ROUTE_DVP=ON`)

```
Pico GPIO    Signal            Goes to
─────────    ──────            ───────
GP0-GP7      D[0:7]            TC358748 DVP input
GP8          PCLK (PIO)            "
GP9          HREF                  "
GP10         VSYNC                 "
GP12         SPI1 RX (MOSI)    V3s SPI master MOSI
GP13         SPI1 CSn          V3s SPI master CS
GP14         SPI1 SCK          V3s SPI master SCK
GP15         SPI1 TX (MISO)    optional readback
GND          GND               common ground
```

Power: Pico VSYS from V3s 5V, or separate USB power. Both boards share a common ground.

## Pin Rationale

- **GP0-GP7 for output data**: consecutive GPIO bank — required both by
  PicoDVI's serialiser and by PIO `out pins, 8` on the DVP route
- **GP8-GP10 (DVP) for PCLK/HREF/VSYNC**: PCLK is PIO side-set,
  HREF/VSYNC are CPU GPIO (timing isn't cycle-critical)
- **SPI**: RP2040 hardware SPI1 only maps to pin groups
  RX {8,12} / CSn {9,13} / SCK {10,14} / TX {11,15}. The DVI route uses
  group 8-11; the DVP route needs 8-10 for sync, so SPI moves to 12-15.

## Display List Protocol

The V3s sends display lists over SPI with this framing:

```
Byte 0-1:  Payload length (uint16_t LE)
Byte 2+:   Command stream

Commands:
  0x00 NOP           — padding (1 byte)
  0x01 CLEAR         — clear framebuffer (2 bytes: cmd, color)
  0x02 TRI           — flat triangle (14 bytes: cmd, color, 3× vertex)
  0x04 QUAD          — flat quad (18 bytes: cmd, color, 4× vertex)
  0x05 LINE          — line segment (10 bytes: cmd, color, 2× vertex)
  0x11 TEXTURE       — upload texture (4+N bytes: cmd, slot, size u16 LE,
                       data). slot < 8, size ≤ 4096 or the frame aborts.
  0x20 SET_RES       — set framebuffer resolution (5 bytes: cmd, w u16, h u16)
  0xFF SCENE_END     — swap buffers and display

Reserved (defined in jupiter32x.h but not yet implemented): 0x06 SPRITE,
0x10 PALETTE. An unknown or truncated command aborts the rest of the
frame — the parser can't know the length of a command it doesn't
recognize, so resynchronizing would draw garbage.
```

Each vertex is `{int16_t x, int16_t y}` — screen coordinates, clamped by
the rasterizer to ±2048. The Pico doesn't do 3D projection. The V3s transforms world coordinates to screen space and sends projected 2D vertices. This keeps the Pico code simple and the display list compact.

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
cd mercury
git clone https://github.com/Wren6991/PicoDVI external/PicoDVI  # DVI route only
mkdir build && cd build
cmake ..                        # DVI route (default)
# cmake -DMERCURY_ROUTE_DVP=ON ..   # parallel DVP route instead
make
```

Flash `mercury.uf2` to the Pico via USB drag-and-drop.

## V3s Integration

On the V3s Jupiter SDK side, add `v3s_side/mercury_csi_capture.h` to your build:

```c
#include "mercury_csi_capture.h"

// During init:
csi_clocks_init();
csi_gpio_init();
csi_capture_init(MERCURY_CAPTURE_BUF, MERCURY_RES_GENESIS);  // phys DRAM addr, resolution

// In your main loop:
if (csi_frame_ready()) {
    // New frame from Pico is in MERCURY_CAPTURE_BUF
    // Convert R3G3B2 → ARGB8888 and map to DE2 VI0
    mercury_r3g3b2_to_argb(vi0_fb, MERCURY_CAPTURE_BUF, 320, 224);
    dcache_clean_range(vi0_fb, 320*224*4);
}

// Send display list to Pico:
mercury_spi_send(display_list_buf, display_list_len);
```

## File Map

```
mercury/
├── CMakeLists.txt              Pico SDK build config
├── README.md                   This file
├── include/
│   └── jupiter32x.h            Constants, types, display list format
├── pio/
│   └── csi_out.pio             PIO programs for DVP pixel + sync output
├── src/
│   ├── mercury_main.c             Core 0 main: SPI recv + rasterize + self-test cube
│   ├── mercury_raster.c           Triangle/line/hline rasterizer, double-buffer swap
│   ├── mercury_displaylist.c      Display list command parser and dispatch
│   ├── mercury_spi.c              SPI slave receiver (hardware SPI1, CS-framed)
│   ├── dvi_out.c               Core 1: PicoDVI scanout (default route)
│   └── csi_out.c               Core 1: PIO + DMA parallel DVP scanout (MERCURY_ROUTE_DVP)
├── test/
│   ├── Makefile                Host-side tests, no pico-sdk needed
│   └── test_mercury.c             Renders the cube via display list → PNG,
│                                  clip-equivalence fuzz, hostile-input fuzz (ASan)
└── v3s_side/
    ├── mercury_csi_capture.h      V3s CSI capture driver (add to Jupiter SDK build)
    └── tc358743_init.h            HDMI→CSI bridge I2C bring-up (TWI0, from mainline driver)
```

## Host tests

The rasterizer and display list parser are plain C with no hardware
dependencies, so they run (and get fuzzed) on a PC:

```bash
cd mercury/test
make && ./test_mercury
```

This renders the self-test cube through the display-list path to
`mercury_selftest.png`, checks the y-clipped rasterizer pixel-for-pixel
against an int64 reference over 20,000 random triangles with coordinates
up to ±32767, and feeds the parser oversized texture claims, truncated
commands, and 20,000 garbage streams under AddressSanitizer.

## Pico → V3s bridge chip

V3s's CSI controller is **MIPI CSI-2**, not parallel DVP, so a bridge chip is mandatory. Two routes have been considered and the code keeps both alive:

| Route | Bridge chip | Pico output | Code path | Status |
|---|---|---|---|---|
| **HDMI** | **TC358743** (HDMI → MIPI CSI-2) | PicoDVI library at 133 MHz | `src/dvi_out.c`, libdvi linked in CMakeLists | Currently active |
| **DVP** | **TC358748** (DVP → MIPI CSI-2) | PIO 8-bit parallel + sync lines | `src/csi_out.c` + `pio/csi_out.pio` | Kept around as the future analog-MIPI route |

Either bridge needs I2C register programming from the V3s side at boot (lane count, data rate, color format, PHY init). Pin mapping, timing, and power sequencing are TBD once a specific bridge dev board is in hand.

## Performance

At 133MHz dual Cortex-M0+:
- **Clear**: ~0.3ms (memset 70KB)
- **Triangle fill**: ~2μs per triangle (average 50-pixel triangle)
- **Frame budget**: ~16.6ms at 60fps
- **Triangle budget**: ~200-500 triangles per frame depending on size
- **SPI receive**: ~0.6ms for 2KB display list at 25MHz
- **CSI scanout**: continuous on Core 1, zero Core 0 cost

This is Star Fox / Virtua Racing class geometry. Flat-shaded, 50-200 polygons per frame, painter's algorithm sorting.

## Status / TODO

### Working (host-verified — nothing below has run on Pico hardware yet)
- Pico-side rasterizer (flat tri, line, hline, clear, tear-free
  double-buffer swap) — fuzzed against an int64 reference, see `test/`
- Display list parser (NOP, CLEAR, TRI, QUAD, LINE, TEXTURE, SET_RES,
  SCENE_END) — hostile-input fuzzed under ASan
- Self-test spinning cube (runs when no SPI master connected) — rendered
  on host, see `test/`

### Believed working (compiles against the right APIs, needs hardware)
- DVI scanout via PicoDVI (Core 1) — TMDS calls follow the upstream
  8bpp app idiom, unverified on a real display
- PIO + DMA parallel DVP scanout (Core 1, `MERCURY_ROUTE_DVP`)
- SPI slave receive (non-blocking, CS-framed, self-resynchronizing)

### Needs Work
- **MIPI CSI-2 PHY configuration on V3s** — `csi_capture_init()` sets up the CSI controller (buffer, size, interrupts) but does NOT configure the MIPI PHY (lane count, data rate, protocol layer). Need a Linux register dump with a real MIPI camera to get the exact PHY register sequence. Same approach used for audio codec and CedarVE.
- **Bridge chip I2C config** — `v3s_side/tc358743_init.h` now carries the full TC358743 sequence (transcribed from the mainline Linux driver: refclk/PHY/PLL setup, HDCP off, RGB888, 2-lane 594 Mbps CSI with the REF_02 D-PHY timing set) over the same TWI0 bus lib/si5351.c uses. Written to spec, needs a bridge board on the bench to verify — `tc358743_status()` reads SYS_STATUS for debugging (expect 0x8E with the Pico connected). The TC358748 (DVP route) equivalent is still TBD.
- **Texture rendering** — CMD_TEXTURE uploads data but no rendering command references texture slots. Need CMD_TEXTRI or similar.
- **R3G3B2→ARGB8888 NEON version** — `mercury_r3g3b2_to_argb()` is scalar C. A NEON version could process 16 pixels per pass.
