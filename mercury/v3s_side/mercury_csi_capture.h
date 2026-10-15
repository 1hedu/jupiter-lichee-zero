/*
 * Mercury — V3s MIPI CSI-2 receive path (D-PHY RX + CSI-2 controller + CSI0)
 *
 * Captures the Pico's DVI picture after the TC358743 turned it into
 * MIPI CSI-2:
 *
 *   Pico (PicoDVI 640x480p60) → TC358743 → 2-lane CSI-2, 594 Mbps/lane
 *     → V3s MIPI D-PHY (RX)          0x01CB2000
 *     → MIPI CSI-2 controller        0x01CB1000  (packet layer, lane merge)
 *     → CSI0                         0x01CB0000  (DMA into DRAM)
 *
 * Every register value and the order they're written in comes from
 * mainline Linux, which drives this exact chain on the V3s:
 *   drivers/phy/allwinner/phy-sun6i-mipi-dphy.c        (D-PHY, RX mode)
 *   drivers/media/platform/sunxi/sun6i-mipi-csi2/      (CSI-2 controller)
 *   drivers/media/platform/sunxi/sun6i-csi/            (CSI0 + DMA)
 *   drivers/clk/sunxi-ng/ccu-sun8i-v3s.c               (clocks/resets)
 * and the block addresses/clock hookup from the V3s MIPI CSI-2 DT
 * series (csi0 @1cb0000, mipi_csi2 @1cb1000, dphy @1cb2000; bus clock
 * CSI, mod clocks CSI_SCLK and MIPI_CSI, reset BUS_CSI).
 *
 * Capture is byte pass-through: CSI0 runs in RAW 8-bit mode, the way
 * Linux captures packed YUV, so each line lands in DRAM exactly as the
 * CSI-2 packets carried it — RGB888 (3 bytes/pixel, the bridge's
 * default here) or UYVY with -DMERCURY_CSI_UYVY (2 bytes/pixel; the
 * bridge must be set to the same, tc358743_init.h follows this define).
 * The whole 640x480 frame is captured and csi_frame_to_argb() samples
 * the Pico's canvas back out of it (PicoDVI shows 320x240 at 2x).
 *
 * Bring-up order (Linux's, and it matters): configure the receiver
 * first, THEN start the bridge's stream — the D-PHY has to see the
 * lanes go LP-11 → HS (tc358743_stream_on() makes that transition).
 *
 * Status: written from the Linux drivers; first silicon run pending.
 * csi_mipi_status() exposes the CSI-2 controller's packet counters,
 * last packet header and error flags so a failing link says where.
 */

#ifndef JUPITER_CSI_CAPTURE_H
#define JUPITER_CSI_CAPTURE_H

#include <stdint.h>
#include "pmu.h"

/* ============================================================
 * Link configuration (must match tc358743_init.h)
 * ============================================================ */
#ifndef MERCURY_CSI_LANES
#define MERCURY_CSI_LANES       2
#endif
#ifndef MERCURY_CSI_LANE_MBPS
#define MERCURY_CSI_LANE_MBPS   594     /* TC358743: 27 MHz / 4 * 88 */
#endif

/* Lanes in use; runtime-changeable (must match the bridge's
 * tc358743_set_csi(), then csi_capture_init() again). */
static int csi_lanes = MERCURY_CSI_LANES;

#define MERCURY_CSI_SRC_W       640     /* PicoDVI frame as received */
#define MERCURY_CSI_SRC_H       480

#ifdef MERCURY_CSI_UYVY
#define MERCURY_CSI_DT          0x1E    /* CSI-2 data type: YUV422 8-bit */
#define MERCURY_CSI_BPP         2
#else
#define MERCURY_CSI_DT          0x24    /* CSI-2 data type: RGB888 */
#define MERCURY_CSI_BPP         3
#endif
#define MERCURY_CSI_LINE_BYTES  (MERCURY_CSI_SRC_W * MERCURY_CSI_BPP)
#define MERCURY_CSI_FRAME_BYTES (MERCURY_CSI_LINE_BYTES * MERCURY_CSI_SRC_H)

/* Two capture buffers in the free VRAM megabytes at the top of DRAM
 * (video_init() zeroes this 2 MB; cedar's encoder/NAL staging also
 * uses it, so don't run those alongside capture). Each frame is
 * 900 KB (RGB888) or 600 KB (UYVY) — one per megabyte. */
#ifndef MERCURY_CSI_BUF0
#define MERCURY_CSI_BUF0        0x43E00000u
#define MERCURY_CSI_BUF1        0x43F00000u
#endif
_Static_assert(MERCURY_CSI_FRAME_BYTES <= 0x100000, "frame > 1 MB slot");

/* ============================================================
 * Registers
 * ============================================================ */
#define CSI_BASE                0x01CB0000
#define MIPI_CSI_BASE           0x01CB1000
#define MIPI_DPHY_BASE          0x01CB2000

#define CSI_R(o)    (*(volatile uint32_t *)(CSI_BASE + (o)))
#define MCSI_R(o)   (*(volatile uint32_t *)(MIPI_CSI_BASE + (o)))
#define DPHY_R(o)   (*(volatile uint32_t *)(MIPI_DPHY_BASE + (o)))
#define CCU_R(o)    (*(volatile uint32_t *)(0x01C20000 + (o)))

/* CSI0 — sun6i_csi_reg.h */
#define CSI_EN_REG              0x00
#define CSI_EN_CSI_EN           (1u << 0)
#define CSI_IF_CFG_REG          0x04
#define CSI_IF_CFG_IF_MIPI      (1u << 7)       /* progressive, 8-bit */
#define CSI_CAP_REG             0x08
#define CSI_CAP_VCAP_ON         (1u << 1)       /* continuous capture */
#define CSI_CH_CFG_REG          0x44
#define CSI_CH_CFG_FIELD_EITHER (2u << 10)      /* input RAW, out RAW_8 = 0 */
#define CSI_CH_FIFO0_ADDR       0x50            /* value = address >> 2 */
#define CSI_CH_STA_REG          0x6C
#define CSI_CH_INT_EN_REG       0x70
#define CSI_CH_INT_STA_REG      0x74            /* write 1 to clear */
#define CSI_INT_VS              (1u << 7)
#define CSI_INT_HB_OF           (1u << 6)
#define CSI_INT_FIFO2_OF        (1u << 4)
#define CSI_INT_FIFO1_OF        (1u << 3)
#define CSI_INT_FIFO0_OF        (1u << 2)
#define CSI_INT_FD              (1u << 1)
#define CSI_INT_CD              (1u << 0)
#define CSI_INT_OVERFLOW        (CSI_INT_HB_OF | CSI_INT_FIFO2_OF | \
                                 CSI_INT_FIFO1_OF | CSI_INT_FIFO0_OF)
#define CSI_CH_HSIZE_REG        0x80            /* len [28:16], start [12:0] */
#define CSI_CH_VSIZE_REG        0x84
#define CSI_CH_BUF_LEN_REG      0x88            /* luma/packed line bytes */

/* MIPI CSI-2 controller — sun6i_mipi_csi2_reg.h (channel 0 regs) */
#define MCSI_CTL_REG            0x00
#define MCSI_CTL_RESET_N        (1u << 31)
#define MCSI_CTL_VERSION_EN     (1u << 30)
#define MCSI_CTL_UNPK_EN        (1u << 1)
#define MCSI_CTL_EN             (1u << 0)
#define MCSI_CFG_REG            0x04
#define MCSI_VCDT_RX_REG        0x08
#define MCSI_RX_PKT_NUM_REG     0x0C
#define MCSI_VERSION_REG        0x3C
#define MCSI_CH_INT_EN_REG      0x50
#define MCSI_CH_INT_PD_REG      0x58
#define MCSI_PD_EOT_ERR         (1u << 29)
#define MCSI_PD_CHKSUM_ERR      (1u << 28)
#define MCSI_PD_ECC_WRN         (1u << 27)
#define MCSI_PD_ECC_ERR         (1u << 26)
#define MCSI_PD_LINE_SYNC_ERR   (1u << 25)
#define MCSI_PD_FRAME_SYNC_ERR  (1u << 24)
#define MCSI_PD_PH_UPDATE       (1u << 16)
#define MCSI_PD_FRAME_START     (1u << 9)
#define MCSI_PD_FRAME_END       (1u << 8)
#define MCSI_PD_FIFO_OVER       (1u << 0)
#define MCSI_PD_ERRORS          (0x3Fu << 24)
#define MCSI_CH_CUR_PH_REG      0x70            /* last packet header */
#define MCSI_CH_FRAME_NUM_REG   0x7C
#define MCSI_CH_LINE_NUM_REG    0x80

/* D-PHY (RX side) — phy-sun6i-mipi-dphy.c */
#define DPHY_GCTL_REG           0x00
#define DPHY_RX_CTL_REG         0x08
#define DPHY_RX_TIME0_REG       0x30
#define DPHY_RX_TIME1_REG       0x34
#define DPHY_RX_TIME2_REG       0x38
#define DPHY_RX_TIME3_REG       0x40
#define DPHY_ANA0_REG           0x4C
#define DPHY_ANA1_REG           0x50
#define DPHY_ANA2_REG           0x54
#define DPHY_ANA3_REG           0x58
#define DPHY_ANA4_REG           0x5C

/* ============================================================
 * Clocks and resets — ccu-sun8i-v3s.c
 * ============================================================ */

/* PLL_PERIPH0 output: 24 MHz * N * K / 2 */
static inline uint32_t csi_pll_periph0_hz(void)
{
    uint32_t v = CCU_R(0x028);
    uint32_t n = ((v >> 8) & 0x1F) + 1, k = ((v >> 4) & 0x3) + 1;
    return 24000000u / 2 * n * k;
}

/* MIPI_CSI clock: PLL_PERIPH0 / 4 (150 MHz from the usual 600 MHz) —
 * the D-PHY's mod clock, Linux sets 150 MHz too. */
#define CSI_MIPI_CLK_DIV        4

static inline void csi_clocks_init(void)
{
    /* PLL_ISP = 24 MHz * 99 / 8 = 297 MHz, integer mode (bit 24 set =
     * integer; same encoding video.c uses for PLL_VIDEO). PLL_VIDEO
     * stays the LCD's. */
    if (!(CCU_R(0x02C) & (1u << 31))) {
        CCU_R(0x02C) = (1u << 31) | (1u << 24) | ((99 - 1) << 8) | (8 - 1);
        for (int i = 0; i < 100000 && !(CCU_R(0x02C) & (1u << 28)); i++)
            delay_us(1);
    }

    CCU_R(0x064) |= (1u << 8);              /* BUS_CLK_GATING1: CSI */
    CCU_R(0x100) |= (1u << 1);              /* DRAM_CFG gate: CSI DMA */

    /* CSI_SCLK (0x134 [31] gate, [26:24] src, [19:16] M): PLL_ISP / 1 =
     * 297 MHz — the rate Linux pins for both CSI0 and the CSI-2
     * controller. The low half of 0x134 is CSI1_MCLK: left alone. */
    uint32_t sclk = CCU_R(0x134) & 0x0000FFFFu;
    CCU_R(0x134) = sclk | (1u << 31) | (1u << 24) | (0u << 16);

    /* MIPI_CSI (0x16C [31] gate, [25:24] src 1 = PLL_PERIPH0, [2:0] M) */
    CCU_R(0x16C) = (1u << 31) | (1u << 24) | (CSI_MIPI_CLK_DIV - 1);

    /* Pulse the shared CSI reset (CSI0, CSI-2 controller, D-PHY) */
    CCU_R(0x2C4) &= ~(1u << 8);
    delay_us(10);
    CCU_R(0x2C4) |= (1u << 8);
    delay_us(10);
}

/* MIPI CSI uses the dedicated PHY pads — no GPIO mux needed */
static inline void csi_gpio_init(void) { }

/* ============================================================
 * D-PHY RX — sun6i_dphy_rx_power_on()
 * ============================================================ */
static inline void csi_dphy_rx_on(void)
{
    uint32_t dphy_hz = csi_pll_periph0_hz() / CSI_MIPI_CLK_DIV;
    uint32_t sym = MERCURY_CSI_LANE_MBPS * 1000000u;   /* symbols/s/lane */

    DPHY_R(DPHY_RX_TIME0_REG) = (255u << 24) | (255u << 16) | (255u << 8);

    /* RX_DLY and LPRST_DLY: the BSP's formulas, as Linux computes them */
    uint32_t rx_dly    = 8 * (dphy_hz / (sym / 8));
    uint32_t lprst_dly = 4 * (dphy_hz / (sym / 2));
    DPHY_R(DPHY_RX_TIME1_REG) = ((rx_dly & 0xFFF) << 20) | 255u;
    DPHY_R(DPHY_RX_TIME2_REG) = 4u;                     /* HS_RX_ANA0 */
    DPHY_R(DPHY_RX_TIME3_REG) = (lprst_dly & 0xFFFF) << 16;

    /* Analog: ANA0 PWS | SLV(7) | SFB(2); ANA1 SVTT(4);
     * ANA4 DMPLVC | DMPLVD(1); ANA2 ENIB; ANA3 LDOR | LDOC | LDOD */
    DPHY_R(DPHY_ANA0_REG) = (1u << 31) | (7u << 12) | (2u << 2);
    DPHY_R(DPHY_ANA1_REG) = 4u << 24;
    DPHY_R(DPHY_ANA4_REG) = (1u << 24) | (1u << 20);
    DPHY_R(DPHY_ANA2_REG) = 1u << 1;
    DPHY_R(DPHY_ANA3_REG) = (1u << 18) | (1u << 25) | (1u << 24);
    delay_us(3);                                       /* LDO ramp */

    /* EN_DBC | RX_CLK_FORCE | per-lane RX enable ("force" bits) */
    uint32_t rx = (1u << 31) | (1u << 24);
    for (int l = 0; l < csi_lanes; l++) rx |= 1u << (20 + l);
    DPHY_R(DPHY_RX_CTL_REG) = rx;

    DPHY_R(DPHY_GCTL_REG) = (((uint32_t)(csi_lanes - 1) & 3u) << 4) | 1u;
}

/* ============================================================
 * CSI-2 controller — sun6i_mipi_csi2_configure() + _enable()
 * ============================================================ */
static uint32_t csi_mipi_version;

static inline void csi_mipi_ctrl_on(void)
{
    /* Deassert reset with VERSION_EN set, read the version, drop
     * VERSION_EN again — Linux notes capture is unreliable without
     * this dance. */
    MCSI_R(MCSI_CTL_REG) = MCSI_CTL_RESET_N | MCSI_CTL_VERSION_EN |
                           MCSI_CTL_UNPK_EN;
    csi_mipi_version = MCSI_R(MCSI_VERSION_REG);
    MCSI_R(MCSI_CTL_REG) &= ~MCSI_CTL_VERSION_EN;

    /* one channel, N lanes */
    MCSI_R(MCSI_CFG_REG) = (0u << 8) | ((uint32_t)(csi_lanes - 1) & 3u);

    /* VC n -> channel n; channel 0 takes our data type */
    MCSI_R(MCSI_VCDT_RX_REG) = (3u << 30) | (2u << 22) | (1u << 14) |
                               (0u << 6) | (MERCURY_CSI_DT & 0x3F);

    MCSI_R(MCSI_CH_INT_PD_REG) = 0xFFFFFFFFu;
    MCSI_R(MCSI_CTL_REG) |= MCSI_CTL_EN;
}

/* ============================================================
 * CSI0 — sun6i_csi_bridge_s_stream() order
 * ============================================================ */
static int csi_cur_buf;                 /* buffer the DMA is filling */
static const uint32_t csi_bufs[2] = { MERCURY_CSI_BUF0, MERCURY_CSI_BUF1 };

static inline void csi0_on(void)
{
    CSI_R(CSI_CAP_REG) = 0;
    CSI_R(CSI_EN_REG) &= ~CSI_EN_CSI_EN;
    CSI_R(CSI_CH_INT_EN_REG) = 0;
    CSI_R(CSI_CH_INT_STA_REG) = 0xFF;

    CSI_R(CSI_IF_CFG_REG) = CSI_IF_CFG_IF_MIPI;
    CSI_R(CSI_CH_CFG_REG) = CSI_CH_CFG_FIELD_EITHER;
    CSI_R(CSI_CH_HSIZE_REG) = ((uint32_t)MERCURY_CSI_LINE_BYTES << 16) | 0;
    CSI_R(CSI_CH_VSIZE_REG) = ((uint32_t)MERCURY_CSI_SRC_H << 16) | 0;
    CSI_R(CSI_CH_BUF_LEN_REG) = MERCURY_CSI_LINE_BYTES;

    csi_cur_buf = 0;
    CSI_R(CSI_CH_FIFO0_ADDR) = csi_bufs[0] >> 2;

    CSI_R(CSI_CH_INT_EN_REG) = CSI_INT_VS | CSI_INT_OVERFLOW |
                               CSI_INT_FD | CSI_INT_CD;
    CSI_R(CSI_EN_REG) |= CSI_EN_CSI_EN;
    CSI_R(CSI_CAP_REG) |= CSI_CAP_VCAP_ON;
}

/*
 * Bring up the receive chain (clocks → D-PHY config → CSI-2 controller
 * → D-PHY power-on → CSI0 DMA), mirroring sun6i_mipi_csi2_s_stream()
 * + sun6i_csi_bridge_s_stream(). Call BEFORE tc358743_stream_on().
 * Returns 0 (the hardware has no "did it work" bit at this stage —
 * csi_mipi_status() is the proof once the bridge streams).
 */
static inline int csi_capture_init(void)
{
    csi_clocks_init();
    csi_mipi_ctrl_on();
    csi_dphy_rx_on();
    csi0_on();
    return 0;
}

/* ============================================================
 * Runtime
 * ============================================================ */

/* Overflow recovery as Linux's IRQ handler does it: bounce CSI_EN. */
static inline void csi_recover(void)
{
    CSI_R(CSI_EN_REG) &= ~CSI_EN_CSI_EN;
    CSI_R(CSI_EN_REG) |= CSI_EN_CSI_EN;
}

static uint32_t csi_overflows;

/*
 * Wait up to timeout_us for a completed frame. On frame-done the DMA
 * address is flipped to the other buffer right away — inside the
 * vertical blanking, before the next frame start latches it — so the
 * returned buffer stays untouched until the next call. Returns the
 * physical address of the finished frame, or 0 on timeout.
 */
static inline uint32_t csi_wait_frame(uint32_t timeout_us)
{
    uint32_t start = pmu_cycles();
    uint32_t limit = timeout_us * PMU_CPU_MHZ;

    /* drop a frame-done that predates this call: its buffer may
     * already be refilling */
    CSI_R(CSI_CH_INT_STA_REG) = CSI_INT_FD | CSI_INT_VS | CSI_INT_CD;

    while ((pmu_cycles() - start) < limit) {
        uint32_t sta = CSI_R(CSI_CH_INT_STA_REG);
        if (sta & CSI_INT_OVERFLOW) {
            CSI_R(CSI_CH_INT_STA_REG) = sta & CSI_INT_OVERFLOW;
            csi_overflows++;
            csi_recover();
            continue;
        }
        if (sta & CSI_INT_FD) {
            int done = csi_cur_buf;
            csi_cur_buf ^= 1;
            CSI_R(CSI_CH_FIFO0_ADDR) = csi_bufs[csi_cur_buf] >> 2;
            CSI_R(CSI_CH_INT_STA_REG) = CSI_INT_FD;
            return csi_bufs[done];
        }
    }
    return 0;
}

/* Receiver sanity: do the enables we wrote read back? All-zero reads
 * mean the block isn't clocked or is held in reset. */
static inline int csi_rx_alive(void)
{
    return (MCSI_R(MCSI_CTL_REG) & (MCSI_CTL_RESET_N | MCSI_CTL_EN)) ==
               (MCSI_CTL_RESET_N | MCSI_CTL_EN) &&
           (DPHY_R(DPHY_GCTL_REG) & 1u) &&
           (CSI_R(CSI_EN_REG) & CSI_EN_CSI_EN);
}

/* Link diagnostics: CSI-2 controller state since the previous call. */
typedef struct {
    uint32_t pending;     /* MCSI_PD_* flags seen (then cleared) */
    uint32_t pkt_hdr;     /* last packet header: DT in [5:0], VC [7:6],
                           * word count [23:8] */
    uint32_t pkt_count;   /* total packets received */
    uint32_t frame_num, line_num;
    uint32_t csi_sta;     /* CSI0 interrupt status bits */
} csi_mipi_status_t;

static inline csi_mipi_status_t csi_mipi_status(void)
{
    csi_mipi_status_t s;
    s.pending   = MCSI_R(MCSI_CH_INT_PD_REG);
    s.pkt_hdr   = MCSI_R(MCSI_CH_CUR_PH_REG);
    s.pkt_count = MCSI_R(MCSI_RX_PKT_NUM_REG);
    s.frame_num = MCSI_R(MCSI_CH_FRAME_NUM_REG);
    s.line_num  = MCSI_R(MCSI_CH_LINE_NUM_REG);
    s.csi_sta   = CSI_R(CSI_CH_INT_STA_REG);
    MCSI_R(MCSI_CH_INT_PD_REG) = s.pending;
    return s;
}

/* ============================================================
 * Frame → ARGB: undo PicoDVI's 2x and the colour encoding
 * ============================================================ */

/*
 * PicoDVI shows a 320x240 canvas at 2x with the w x h framebuffer
 * centred in it (dvi_out.c). Canvas pixel (x, y) is the 2x2 block at
 * frame (2x, 2y); sample its top-left. swap_rb flips the RGB888 byte
 * order in case the bridge sends R first (CSI-2 RGB888 is B, G, R).
 */
static inline void csi_frame_to_argb(uint32_t *dst, const uint8_t *frame,
                                     uint32_t w, uint32_t h, int swap_rb)
{
    uint32_t x_off = (MERCURY_CSI_SRC_W / 2 - w) / 2;
    uint32_t y_off = (MERCURY_CSI_SRC_H / 2 - h) / 2;
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *row = frame +
            (2 * (y + y_off)) * MERCURY_CSI_LINE_BYTES;
        uint32_t *out = dst + y * w;
#ifdef MERCURY_CSI_UYVY
        (void)swap_rb;
        /* one UYVY pair = one 2x-wide canvas pixel: U Y0 V Y1 */
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *p = row + (x + x_off) * 4;
            int c = (int)p[1] - 16, d = (int)p[0] - 128, e = (int)p[2] - 128;
            int r = (298 * c + 409 * e + 128) >> 8;          /* BT.601, */
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8; /* limited */
            int b = (298 * c + 516 * d + 128) >> 8;           /* range   */
            r = r < 0 ? 0 : r > 255 ? 255 : r;
            g = g < 0 ? 0 : g > 255 ? 255 : g;
            b = b < 0 ? 0 : b > 255 ? 255 : b;
            out[x] = 0xFF000000u | ((uint32_t)r << 16) |
                     ((uint32_t)g << 8) | (uint32_t)b;
        }
#else
        int ri = swap_rb ? 0 : 2, bi = swap_rb ? 2 : 0;
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *p = row + (2 * (x + x_off)) * 3;
            out[x] = 0xFF000000u | ((uint32_t)p[ri] << 16) |
                     ((uint32_t)p[1] << 8) | (uint32_t)p[bi];
        }
#endif
    }
}

#endif
