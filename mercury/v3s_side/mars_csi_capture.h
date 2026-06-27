/*
 * Mercury — V3s MIPI CSI-2 Capture Driver
 *
 * Captures frames from the Pico coprocessor via MIPI CSI-2.
 *
 * Two supported signal paths:
 *   1. Bridge route: Pico (PicoDVI) → TC358743 (HDMI→CSI) → V3s MIPI CSI-2
 *   2. Analog route: Pico (PIO MIPI TX) → LVDS driver + resistor network → V3s MIPI CSI-2
 *
 * Both arrive at the same V3s MIPI CSI-2 receiver. This driver handles
 * the V3s side only — it doesn't care which route the pixels took.
 *
 * MIPI CSI-2 pins on Lichee Pi Zero (2.54mm header):
 *   Pin 17: MCSI-D0P    Pin 19: MCSI-D0N    (Data Lane 0)
 *   Pin 21: MCSI-D1P    Pin 23: MCSI-D1N    (Data Lane 1)
 *   Pin 25: MCSI-CKP    Pin 27: MCSI-CKN    (Clock)
 *
 * No PE pin conflict with LCD — MIPI uses dedicated PHY pins.
 *
 * CSI0 register base: 0x01CB0000
 * MIPI CSI-2 PHY base: 0x01CB1000
 */

#ifndef JUPITER_CSI_CAPTURE_H
#define JUPITER_CSI_CAPTURE_H

#include <stdint.h>

/* CSI0 register base */
#define CSI_BASE        0x01CB0000
#define MIPI_CSI_BASE   0x01CB1000

/* CSI registers */
#define CSI_EN          0x000
#define CSI_CFG         0x004
#define CSI_CAP         0x008
#define CSI_HSIZE       0x040
#define CSI_VSIZE       0x044
#define CSI_BUF_ADDR0   0x048
#define CSI_BUF_LEN     0x050
#define CSI_INT_EN      0x060
#define CSI_INT_STA     0x064

#define CSI_EN_CSI_EN           (1u << 0)
#define CSI_CFG_INPUT_FMT(x)    ((x) << 20)
#define CSI_CFG_OUTPUT_FMT(x)   ((x) << 16)
#define CSI_CAP_VCAP_ON         (1u << 1)
#define CSI_INT_FRAME_DONE      (1u << 1)

/* Supported capture resolutions (match Pico output) */
#define MARS_RES_GB       0   /* 160×144 */
#define MARS_RES_NES      1   /* 256×224 */
#define MARS_RES_SNES     2   /* 256×224 */
#define MARS_RES_GENESIS  3   /* 320×224 */
#define MARS_RES_LCD      4   /* 480×272 */

static const struct { uint16_t w, h; } mars_resolutions[] = {
    {160, 144},   /* GB */
    {256, 224},   /* NES */
    {256, 224},   /* SNES */
    {320, 224},   /* Genesis */
    {480, 272},   /* V3s LCD */
};

/* R3G3B2 → ARGB8888 expansion */
static inline void mars_r3g3b2_to_argb(uint32_t *dst, const uint8_t *src,
                                        uint32_t w, uint32_t h)
{
    uint32_t count = w * h;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t p = src[i];
        uint8_t r = (p & 0xE0);
        uint8_t g = (p & 0x1C) << 3;
        uint8_t b = (p & 0x03) << 6;
        r |= (r >> 3) | (r >> 6);
        g |= (g >> 3) | (g >> 6);
        b |= (b >> 2) | (b >> 4) | (b >> 6);
        dst[i] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}

static inline void csi_clocks_init(void)
{
    volatile uint32_t *ccu = (volatile uint32_t *)0x01C20000;
    ccu[0x064 / 4] |= (1u << 8) | (1u << 2);    /* bus gate: CSI + MIPI */
    ccu[0x2C4 / 4] |= (1u << 8) | (1u << 2);    /* reset: CSI + MIPI */
    ccu[0x134 / 4] = (1u << 31) | (0 << 24);     /* CSI module clock */
    ccu[0x13C / 4] = (1u << 31);                  /* MIPI CSI clock */
    ccu[0x100 / 4] |= (1u << 1);                  /* DRAM gate for CSI */
}

/* MIPI CSI uses dedicated PHY pins — no GPIO mux needed */
static inline void csi_gpio_init(void) { }

/*
 * Initialize CSI0 for MIPI CSI-2 capture at the specified resolution.
 *
 * buf_addr: physical DRAM address, 4-byte aligned.
 * res: one of MARS_RES_* constants.
 *
 * TODO: MIPI PHY configuration (lane count, data rate, protocol setup).
 * Needs Linux register dump with TC358743 connected to get exact values.
 */
static inline void csi_capture_init(uint32_t buf_addr, int res)
{
    volatile uint32_t *csi = (volatile uint32_t *)CSI_BASE;
    uint16_t w = mars_resolutions[res].w;
    uint16_t h = mars_resolutions[res].h;

    csi[CSI_EN / 4] = 0;
    csi[CSI_CFG / 4] = CSI_CFG_INPUT_FMT(0) | CSI_CFG_OUTPUT_FMT(0);
    csi[CSI_HSIZE / 4] = ((uint32_t)w << 16) | w;
    csi[CSI_VSIZE / 4] = ((uint32_t)h << 16) | h;
    csi[CSI_BUF_ADDR0 / 4] = buf_addr;
    csi[CSI_BUF_LEN / 4] = w;
    csi[CSI_INT_STA / 4] = 0xFFFFFFFF;
    csi[CSI_INT_EN / 4] = CSI_INT_FRAME_DONE;
    csi[CSI_EN / 4] = CSI_EN_CSI_EN;
    csi[CSI_CAP / 4] = CSI_CAP_VCAP_ON;
}

static inline int csi_frame_ready(void)
{
    volatile uint32_t *csi = (volatile uint32_t *)CSI_BASE;
    uint32_t status = csi[CSI_INT_STA / 4];
    if (status & CSI_INT_FRAME_DONE) {
        csi[CSI_INT_STA / 4] = CSI_INT_FRAME_DONE;
        return 1;
    }
    return 0;
}

static inline void csi_set_buffer(uint32_t buf_addr)
{
    volatile uint32_t *csi = (volatile uint32_t *)CSI_BASE;
    csi[CSI_BUF_ADDR0 / 4] = buf_addr;
}

#endif
