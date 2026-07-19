/*
 * Jupiter 32X — Pico Polygon Coprocessor
 *
 * Renders flat-shaded polygons and outputs via PicoDVI (HDMI) to a
 * TC358743 bridge, which converts to MIPI CSI-2 for V3s capture.
 * Future: direct PIO MIPI TX with LVDS driver for arbitrary resolution.
 *
 * Pin assignments:
 *   GPIO 0-7:   PicoDVI TMDS (4 differential pairs)
 *   GPIO 8-11:  SPI1 slave (display list from V3s)
 *
 * Scanout interface is abstracted — swap dvi_out.c for mipi_out.c
 * to switch from bridge route to analog route without touching
 * the rasterizer or display list parser.
 */

#ifndef JUPITER32X_H
#define JUPITER32X_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Display configuration — configurable per system
 * ============================================================ */

/* Default: Genesis resolution */
#ifndef J32X_WIDTH
#define J32X_WIDTH      320
#endif
#ifndef J32X_HEIGHT
#define J32X_HEIGHT     224
#endif

#define J32X_BPP        8       /* R3G3B2 format — 256 colors */
#define J32X_FB_SIZE    (J32X_WIDTH * J32X_HEIGHT)

/* DVI output timing: 640×480 @ 60Hz (25.2MHz pixel clock)
 * Active area is J32X_WIDTH × J32X_HEIGHT, centered in the
 * 640×480 frame with blanking padding. The TC358743 sees valid
 * VGA timing regardless of the active area size. */
#define J32X_DVI_H_ACTIVE   640
#define J32X_DVI_V_ACTIVE   480

/* Offset to center the active area in the DVI frame */
#define J32X_DVI_X_OFFSET   ((J32X_DVI_H_ACTIVE - J32X_WIDTH) / 2)
#define J32X_DVI_Y_OFFSET   ((J32X_DVI_V_ACTIVE - J32X_HEIGHT) / 2)

/* Pin assignments — PicoDVI route */
#define PIN_DVI_D2P     0       /* TMDS Data 2+ (Blue) */
#define PIN_DVI_D2N     1       /* TMDS Data 2- */
#define PIN_DVI_D1P     2       /* TMDS Data 1+ (Green) */
#define PIN_DVI_D1N     3       /* TMDS Data 1- */
#define PIN_DVI_D0P     4       /* TMDS Data 0+ (Red) */
#define PIN_DVI_D0N     5       /* TMDS Data 0- */
#define PIN_DVI_CLKP    6       /* TMDS Clock+ */
#define PIN_DVI_CLKN    7       /* TMDS Clock- */

/* Pin assignments — DVP/parallel route (csi_out.c, MERCURY_ROUTE_DVP).
 * Feeds a TC358748 DVP→CSI-2 bridge; data pins must start at GPIO0 for
 * PIO `out pins, 8`. */
#define PIN_CSI_D0      0       /* D[0:7] on GPIO 0-7 */
#define PIN_CSI_PCLK    8       /* pixel clock (PIO side-set) */
#define PIN_CSI_HREF    9       /* line valid (CPU GPIO) */
#define PIN_CSI_VSYNC   10      /* frame sync (CPU GPIO) */
/* PCLK = sysclk / (2 * clkdiv): 133MHz / (2*5) = 13.3MHz — inside the
 * TC358748's DVP input range. Tune once the bridge is on the bench. */
#define J32X_PIO_CLKDIV 5.0f

/* Pin assignments — SPI slave (display list from V3s).
 * RP2040 SPI1 pin groups: RX {8,12}, CSn {9,13}, SCK {10,14}, TX {11,15}.
 * The DVI route uses group 8-11; the DVP route needs 8-10 for
 * PCLK/HREF/VSYNC, so SPI moves to group 12-15 there. */
#ifdef MERCURY_ROUTE_DVP
#define PIN_SPI_RX      12      /* SPI1 RX (MOSI from V3s) */
#define PIN_SPI_CSN     13      /* SPI1 CSn */
#define PIN_SPI_SCK     14      /* SPI1 SCK */
#define PIN_SPI_TX      15      /* SPI1 TX (MISO, optional) */
#else
#define PIN_SPI_RX      8       /* SPI1 RX (MOSI from V3s) */
#define PIN_SPI_CSN     9       /* SPI1 CSn */
#define PIN_SPI_SCK     10      /* SPI1 SCK */
#define PIN_SPI_TX      11      /* SPI1 TX (MISO, optional) */
#endif

/* Vertex coordinate clamp. Keeps the rasterizer's 16.16 edge stepping
 * far from int32 overflow ((x1-x0)<<16 must fit) and bounds the
 * offscreen scanline walk for hostile/garbled display lists. */
#define J32X_COORD_MIN  (-2048)
#define J32X_COORD_MAX  2047

/* ============================================================
 * R3G3B2 color format
 * ============================================================ */
#define RGB332(r, g, b) \
    ((uint8_t)(((r) & 0xE0) | (((g) >> 3) & 0x1C) | (((b) >> 6) & 0x03)))

#define COL_BLACK       RGB332(0x00, 0x00, 0x00)
#define COL_WHITE       RGB332(0xFF, 0xFF, 0xFF)
#define COL_RED         RGB332(0xFF, 0x00, 0x00)
#define COL_GREEN       RGB332(0x00, 0xFF, 0x00)
#define COL_BLUE        RGB332(0x00, 0x00, 0xFF)
#define COL_TRANSPARENT 0x00

/* ============================================================
 * Display list commands (received from V3s over SPI)
 * ============================================================ */
#define CMD_NOP         0x00
#define CMD_CLEAR       0x01
#define CMD_TRI         0x02
#define CMD_QUAD        0x04
#define CMD_LINE        0x05
#define CMD_SPRITE      0x06
#define CMD_PALETTE     0x10
#define CMD_TEXTURE     0x11
#define CMD_SET_RES     0x20    /* set framebuffer resolution */
#define CMD_SCENE_END   0xFF

typedef struct {
    int16_t x, y;
} j32x_vertex_t;

typedef struct __attribute__((packed)) {
    uint8_t         cmd;
    uint8_t         color;
    j32x_vertex_t   v[3];
} j32x_cmd_tri_t;

typedef struct __attribute__((packed)) {
    uint8_t         cmd;
    uint8_t         color;
    j32x_vertex_t   v[4];
} j32x_cmd_quad_t;

typedef struct __attribute__((packed)) {
    uint8_t         cmd;
    uint8_t         color;
} j32x_cmd_clear_t;

typedef struct __attribute__((packed)) {
    uint8_t         cmd;
    uint8_t         color;
    j32x_vertex_t   v[2];
} j32x_cmd_line_t;

typedef struct __attribute__((packed)) {
    uint8_t         cmd;
    uint16_t        w;
    uint16_t        h;
} j32x_cmd_set_res_t;

/* ============================================================
 * Texture cache
 * ============================================================ */
#define J32X_TEX_SLOTS  8
#define J32X_TEX_MAX    4096

typedef struct {
    uint8_t     data[J32X_TEX_MAX];
    uint16_t    w, h;
    bool        valid;
} j32x_tex_slot_t;

/* ============================================================
 * State
 * ============================================================ */
typedef struct {
    uint8_t         fb[2][J32X_FB_SIZE];
    volatile uint8_t front;
    volatile uint8_t back;
    /* Set by j32x_swap() (core 0), consumed at frame boundary by the
     * scanout loop (core 1) so a swap can never tear mid-scanout. */
    volatile uint8_t swap_pending;
    uint16_t        width;      /* current render width */
    uint16_t        height;     /* current render height */
    j32x_tex_slot_t tex[J32X_TEX_SLOTS];
    volatile uint32_t frame_count;
    uint32_t        tri_count;
} j32x_state_t;

extern j32x_state_t g_state;

/* ============================================================
 * Scanout interface (implemented by dvi_out.c or mipi_out.c)
 * ============================================================ */
void j32x_scanout_start(void);  /* init PIO/DVI/MIPI hardware */
void j32x_scanout_loop(void);   /* run on Core 1, never returns */

/* ============================================================
 * Rasterizer
 * ============================================================ */
void j32x_raster_clear(uint8_t color);
void j32x_raster_tri(j32x_vertex_t v0, j32x_vertex_t v1, j32x_vertex_t v2,
                      uint8_t color);
void j32x_raster_line(j32x_vertex_t v0, j32x_vertex_t v1, uint8_t color);
void j32x_raster_hline(int x0, int x1, int y, uint8_t color);
void j32x_swap(void);        /* core 0: raise + wait          */
void j32x_swap_apply(void);  /* core 1: flip at frame boundary */

/* ============================================================
 * SPI + Display List
 * ============================================================ */
void j32x_spi_init(void);
int  j32x_spi_recv_displaylist(uint8_t *buf, uint32_t max_len);
void j32x_exec_displaylist(const uint8_t *buf, uint32_t len);

#endif
