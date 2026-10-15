# Mercury — Pico Polygon Coprocessor for Jupiter

A Raspberry Pi Pico 2 W (RP2350; the original RP2040 Pico is still supported) that acts as a 3D polygon rendering coprocessor for the Jupiter SDK, streaming frames back to the Allwinner V3s through its MIPI CSI-2 camera input (via a bridge chip — see "Pico → V3s bridge chip" below). Mercury is to Jupiter what the 32X was to the Genesis — a small fast companion that adds a capability the host platform doesn't have on its own (in our case, real polygon hardware).

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

## Wiring — Pico 2 W + SpotPear Pico HDMI board (default)

The SpotPear board plugs onto the Pico (HDMI connector + 270 Ω TMDS
resistors on board). Its schematic routes TMDS to **GP8–GP15** and HDMI
DDC/CEC to GP18–GP20, so the V3s SPI link uses SPI0 on GP4–7. V3s-side
pins follow docs/PIN_OVERLAYS.md **Overlay 4**.

```
Pico GPIO    Signal              Goes to
─────────    ──────              ───────
GP8-GP15     TMDS (SpotPear)     HDMI → TC358743 (or a monitor)
GP4          SPI0 RX (MOSI)      V3s PC3  (header pin 24)
GP5          SPI0 CSn            V3s PC2  (header pin 28)
GP6          SPI0 SCK            V3s PC1  (header pin 22)
GP7          SPI0 TX (MISO)      unused
GND          GND                 common ground

TC358743 I2C:  V3s PE21 = SCL, PE22 = SDA (TWI1)
```

This board's pairs sit partly outside GP12–19, so it uses the PicoDVI
(PIO) route on the RP2350 — `clk_sys` is raised to 252 MHz at 1.20 V
(one TMDS bit per cycle), exactly as SpotPear's own demos do, and the
program runs from SRAM so the overclock never touches flash.

**PicoDVI output is 2× scaled:** its 8bpp encoder pixel-doubles
horizontally and each line is repeated twice, so the 320×224
framebuffer fills the screen as 640×448 with thin black bars top and
bottom. (The HSTX route below is 1:1 centered; the V3s capture side
must match whichever route is in use.)

## Other DVI boards — HSTX hardware DVI on Pico 2 / 2 W

The RP2350's **HSTX** peripheral TMDS-encodes in hardware (zero CPU
pixel work), but exists **only on GP12–GP19**. Boards wired entirely
there use it: `-DMERCURY_DVI_BOARD=DVI_SOCK` (Pico DVI Sock) or `AMY`.

```
Pico 2 GPIO  Signal            (DVI Sock layout)
───────────  ──────
GP12/GP13    TMDS D0± (blue)
GP14/GP15    TMDS CLK±
GP16/GP17    TMDS D2± (red)
GP18/GP19    TMDS D1± (green)
GP8-GP11     SPI1 RX / CSn / SCK / TX  (V3s link)
```

**HSTX clock:** HSTX shifts 2 bits per `clk_hstx` cycle and the SDK
ties `clk_hstx` to `clk_sys`, so these builds pin `clk_sys` to
**126 MHz** (252 Mbps/lane → exactly 60.0 Hz). At the RP2350's
150 MHz default the same config would emit a non-standard ~71 Hz mode.

## Wiring — Pico (RP2040), original hand wiring (`-DPICO_BOARD=pico -DMERCURY_DVI_BOARD=LEGACY_GP0`)

```
Pico GPIO    Signal            Goes to
─────────    ──────            ───────
GP0/GP1      TMDS D2± (red)    TC358743 HDMI input
GP2/GP3      TMDS D1± (green)      "
GP4/GP5      TMDS D0± (blue)       "
GP6/GP7      TMDS CLK±             "
GP8          SPI1 RX (MOSI)    V3s SPI master MOSI
GP9          SPI1 CSn          V3s SPI master CS
GP10         SPI1 SCK          V3s SPI master SCK
GP11         SPI1 TX (MISO)    optional readback
GND          GND               common ground
```

Bridge → V3s (both DVI routes): TC358743 CSI-2 output → Lichee Pi Zero
MIPI CSI pins (header pins 17/19 = D0±, 21/23 = D1±, 25/27 = CLK±).

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

- **GP8-GP15 (SpotPear)**: fixed by the board; GP4-7 SPI0 avoids it
  and its DDC lines on GP18-20
- **GP12-GP19 for HSTX DVI on RP2350**: the only pins HSTX can drive
- **GP0-GP7 for output data on RP2040 / DVP**: consecutive GPIO bank —
  required both by PicoDVI's serialiser and by PIO `out pins, 8`
- **GP8-GP10 (DVP) for PCLK/HREF/VSYNC**: PCLK is PIO side-set,
  HREF/VSYNC are CPU GPIO (timing isn't cycle-critical)
- **SPI**: hardware SPI1 (same groups on RP2040 and RP2350) only maps to
  RX {8,12} / CSn {9,13} / SCK {10,14} / TX {11,15}. Both DVI routes use
  group 8-11; the DVP route needs 8-10 for sync, so SPI moves to 12-15.
  CSn has an internal pull-up so it idles deasserted with no V3s
  attached (the self-test needs that).

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
                       w: multiple of 4, 32..320; h: 1..224 (else ignored)
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
export PICO_SDK_PATH=/path/to/pico-sdk          # SDK 2.1.1 or newer
cd mercury && mkdir build && cd build
git clone https://github.com/Wren6991/PicoDVI external/PicoDVI  # PicoDVI boards
cmake ..                                  # Pico 2 W + SpotPear (default)
# cmake -DMERCURY_DVI_BOARD=DVI_SOCK ..   #   DVI Sock: HSTX, no PicoDVI needed
# cmake -DPICO_BOARD=pico ..              #   RP2040 Pico (+ any board preset)
# cmake -DMERCURY_ROUTE_DVP=ON ..         #   parallel DVP route instead
make
```

Board presets (`MERCURY_DVI_BOARD`): `SPOTPEAR`, `DVI_SOCK`, `AMY`,
`LEGACY_GP0` (Mercury's original GP0–7 hand wiring) — see
include/jupiter32x.h.
The SDK fetches and builds `picotool` on first configure for RP2350
targets (set `PICOTOOL_FETCH_FROM_GIT_PATH` to share it across builds).

Flash `mercury.uf2`: hold BOOTSEL while plugging in USB, drag the file
onto the drive that appears.

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
│   ├── jupiter32x.h            Constants, types, pins + HSTX board presets
│   └── hstx_dvi_stream.h       RP2350 DVI timing + HSTX command opcodes
├── pio/
│   └── csi_out.pio             PIO programs for DVP pixel + sync output
├── src/
│   ├── mercury_main.c             Core 0 main: SPI recv + rasterize + self-test cube
│   ├── mercury_raster.c           Triangle/line/hline rasterizer, double-buffer swap
│   ├── mercury_displaylist.c      Display list command parser and dispatch
│   ├── mercury_spi.c              SPI slave receiver (hardware SPI1, CS-framed)
│   ├── hstx_dvi_out.c          Core 1: RP2350 HSTX DVI scanout (default, Pico 2 / 2 W)
│   ├── hstx_dvi_stream.c       HSTX DMA segment generator (pure C, host-tested)
│   ├── dvi_out.c               Core 1: PicoDVI scanout (RP2040)
│   └── csi_out.c               Core 1: PIO + DMA parallel DVP scanout (MERCURY_ROUTE_DVP)
├── test/
│   ├── Makefile                Host-side tests, no pico-sdk needed
│   ├── test_mercury.c             Renders the cube via display list → PNG,
│   │                              clip-equivalence fuzz, hostile-input fuzz (ASan)
│   └── test_hstx_stream.c         Decodes the HSTX DVI stream symbol-by-symbol
└── v3s_side/
    ├── mercury_csi_capture.h      V3s CSI capture driver (add to Jupiter SDK build)
    ├── mercury_spi_master.h       V3s SPI0 master — CS-framed display-list sender
    └── tc358743_init.h            HDMI→CSI bridge I2C bring-up (TWI0, from mainline driver)
```

## V3s-side example: `examples/mercury_gpu`

`make GAME=examples/mercury_gpu/main.c` (also in the menu under
Benchmarks) drives all three legs of the link with a live HUD:
bridge I2C init + SYS_STATUS decode (TMDS/PLL/SCDT/SYNC), a
spinning-cube display list streamed over SPI0 at 60 Hz, and the CSI
capture counter. Best first bring-up: no bridge at all — plug the
Pico's DVI into a monitor, run the example, and the Pico's self-test
cube (primary colors) should be replaced by the V3s-commanded cube
(dusk palette). That proves V3s→SPI→Pico→raster end to end. The
display-list builder is verified byte-compatible with the Pico parser
by a host round-trip test (built on the V3s side, executed by
mercury_displaylist.c + mercury_raster.c, rendered to PNG).

## Host tests

The rasterizer, display list parser, and HSTX stream generator are
plain C with no hardware dependencies, so they run (and get fuzzed) on
a PC:

```bash
cd mercury/test
make && ./test_mercury && ./test_hstx_stream
```

This renders the self-test cube through the display-list path to
`mercury_selftest.png`, checks the y-clipped rasterizer pixel-for-pixel
against an int64 reference over 20,000 random triangles with coordinates
up to ±32767, and feeds the parser oversized texture claims, truncated
commands, and 20,000 garbage streams under AddressSanitizer.

`test_hstx_stream` feeds the RP2350 scanout's DMA segments, in the order
the ping-pong channels deliver them, through a model of the HSTX command
expander (RAW / RAW_REPEAT / TMDS / NOP) and checks every symbol period
of 4 frames: 800 periods per line, front porch/sync/back porch
placement and polarity, the two vsync lines, black letterbox and
pillarbox, and every framebuffer pixel in position — across a buffer
swap and a mid-stream resolution change. It caught a one-word
command-list misalignment (no-signal on a real monitor) during
development.

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
