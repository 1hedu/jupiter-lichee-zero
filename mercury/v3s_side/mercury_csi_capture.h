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

/* CSI controller registers — offsets from Linux
 * drivers/media/platform/sunxi/sun6i-csi/sun6i_csi_reg.h (the V3s CSI
 * is this sun6i block). The previous map here was wrong: it read the
 * "frame done" status from 0x64 (not a status register — it echoed the
 * 0xFFFFFFFF written at init, so every poll reported a frame), wrote
 * the frame height into CH_CFG (0x44, the input-format register) and
 * the frame width into CH_FIFO0_ADDR (0x50) — the DMA destination —
 * before switching capture on. */
#define CSI_EN_REG          0x00    /* bit 0: enable */
#define CSI_IF_CFG_REG      0x04
#define CSI_CAP_REG         0x08    /* bit 1: video capture on */
#define CSI_CH_CFG_REG      0x44    /* input/output format */
#define CSI_CH_FIFO0_ADDR   0x50    /* DMA destination (luma/packed) */
#define CSI_CH_INT_EN_REG   0x70
#define CSI_CH_INT_STA_REG  0x74    /* write-1-to-clear */
#define CSI_CH_HSIZE_REG    0x80
#define CSI_CH_VSIZE_REG    0x84
#define CSI_CH_BUF_LEN_REG  0x88
#define CSI_CH_INT_FD       (1u << 1)   /* frame done (EN and STA) */

/* The MIPI CSI-2 receive path in front of CSI0 (MIPI CSI-2 controller
 * at 0x01CB1000 + its D-PHY in RX mode) is NOT implemented yet — see
 * mercury/README.md. Until it is, the capture functions below never
 * touch the hardware: csi_capture_init() reports "not armed" and
 * csi_frame_ready() always returns 0, so callers can't display
 * uninitialised memory as a "captured frame" or aim DMA anywhere.
 * Set to 1 only together with a working MIPI RX bring-up. */
#ifndef MERCURY_MIPI_RX_READY
#define MERCURY_MIPI_RX_READY 0
#endif

/* Supported capture resolutions (match Pico output) */
#define MERCURY_RES_GB       0   /* 160×144 */
#define MERCURY_RES_NES      1   /* 256×224 */
#define MERCURY_RES_SNES     2   /* 256×224 */
#define MERCURY_RES_GENESIS  3   /* 320×224 */
#define MERCURY_RES_LCD      4   /* 480×272 */

static const struct { uint16_t w, h; } mercury_resolutions[] = {
    {160, 144},   /* GB */
    {256, 224},   /* NES */
    {256, 224},   /* SNES */
    {320, 224},   /* Genesis */
    {480, 272},   /* V3s LCD */
};

/* R3G3B2 → ARGB8888 expansion */
static inline void mercury_r3g3b2_to_argb(uint32_t *dst, const uint8_t *src,
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

/* Clock/reset setup belongs to the MIPI RX bring-up (the CCU bits the
 * old version set were unverified guesses) — no-op until then. */
static inline void csi_clocks_init(void) { }

/* MIPI CSI uses dedicated PHY pins — no GPIO mux needed */
static inline void csi_gpio_init(void) { }

/*
 * Arm CSI0 capture into buf_addr (physical, 4-byte aligned).
 * Returns 0 when armed, -1 when the MIPI receive path isn't available
 * (current state) — nothing is written to the hardware in that case.
 */
static inline int csi_capture_init(uint32_t buf_addr, int res)
{
    (void)buf_addr; (void)res;
#if MERCURY_MIPI_RX_READY
#error "CSI0 capture setup must be written together with the MIPI RX bring-up"
#endif
    return -1;
}

static inline int csi_frame_ready(void)
{
#if MERCURY_MIPI_RX_READY
    volatile uint32_t *csi = (volatile uint32_t *)CSI_BASE;
    if (csi[CSI_CH_INT_STA_REG / 4] & CSI_CH_INT_FD) {
        csi[CSI_CH_INT_STA_REG / 4] = CSI_CH_INT_FD;     /* w1c */
        return 1;
    }
#endif
    return 0;
}

#endif
