/*
 * Mercury — TC358743 (HDMI → MIPI CSI-2) bridge bring-up over I2C
 *
 * The register sequence is transcribed from the mainline Linux driver
 * (drivers/media/i2c/tc358743.c, which in turn follows Toshiba's
 * "REF_02" register-settings spreadsheet) — the same init path the
 * Auvidea B101/B102 boards use on the Raspberry Pi, specialized for
 * Mercury's fixed configuration:
 *
 *   HDMI in:  640×480p60 RGB (DVI, no audio/infoframes — PicoDVI)
 *   CSI out:  2 lanes, 594 Mbps/lane, RGB888, continuous clock
 *   Refclk:   27 MHz (the B101's onboard oscillator; change
 *             TC_REFCLK_HZ if your bridge board differs)
 *
 * I2C: TWI0 on PB6 (SCL) / PB7 (SDA), 100 kHz — same bus + state
 * machine as lib/si5351.c. TC358743 uses 16-bit register addresses
 * (MSB first) and little-endian data for 16/32-bit registers.
 *
 * NOTE: PB6/PB7 double as YM3438 D6/D7 in the sound examples — run the
 * bridge init before ym3438_hw_init() reclaims the pins (one-shot
 * config at boot is all the bridge needs).
 *
 * The Pico never reads EDID or checks HPD, so the EDID RAM is left
 * unprogrammed and HPD stays low. If you ever feed this bridge from a
 * real HDMI source, load a 640×480 EDID into 0x8C00+ first.
 *
 * Status: UNTESTED on silicon (no bridge board on the bench yet).
 * tc358743_status() dumps everything needed to debug lock issues.
 */

#ifndef JUPITER_TC358743_INIT_H
#define JUPITER_TC358743_INIT_H

#include <stdint.h>

/* ============================================================
 * Board configuration
 * ============================================================ */
#define TC_I2C_ADDR     0x0F        /* B101 default (7-bit) */
#define TC_REFCLK_HZ    27000000u
#define TC_CSI_LANES    2           /* V3s MIPI has 2 data lanes */

/* PLL: bps/lane = (refclk / prd) * fbd = (27M / 4) * 88 = 594 Mbps.
 * The D-PHY timing block below is the driver's known-good 594 Mbps
 * set — if you change the rate, those timings must change with it. */
#define TC_PLL_PRD      4
#define TC_PLL_FBD      88

/* ============================================================
 * Register map (16-bit addresses; names match tc358743_regs.h)
 * ============================================================ */
#define TC_CHIPID          0x0000   /* 16-bit, high byte must be 0x00 */
#define TC_SYSCTL          0x0002   /* 16-bit reset/sleep control */
#define TC_CONFCTL         0x0004   /* 16-bit */
#define TC_FIFOCTL         0x0006   /* 16-bit CSI FIFO trigger level */
#define TC_PLLCTL0         0x0020   /* 16-bit */
#define TC_PLLCTL1         0x0022   /* 16-bit */
#define TC_CECHCLK         0x0028   /* 16-bit */
#define TC_CECLCLK         0x002A   /* 16-bit */
/* CSI TX (32-bit registers) */
#define TC_CLW_CNTRL       0x0140
#define TC_D0W_CNTRL       0x0144
#define TC_D1W_CNTRL       0x0148
#define TC_D2W_CNTRL       0x014C
#define TC_D3W_CNTRL       0x0150
#define TC_STARTCNTRL      0x0204
#define TC_LINEINITCNT     0x0210
#define TC_LPTXTIMECNT     0x0214
#define TC_TCLK_HEADERCNT  0x0218
#define TC_TCLK_TRAILCNT   0x021C
#define TC_THS_HEADERCNT   0x0220
#define TC_TWAKEUP         0x0224
#define TC_TCLK_POSTCNT    0x0228
#define TC_THS_TRAILCNT    0x022C
#define TC_HSTXVREGCNT     0x0230
#define TC_HSTXVREGEN      0x0234
#define TC_TXOPTIONCNTRL   0x0238
#define TC_CSI_CONFW       0x0500
#define TC_CSI_START       0x0518
/* HDMI RX (8-bit registers) */
#define TC_SYS_STATUS      0x8520
#define TC_PHY_CTL0        0x8531
#define TC_PHY_CTL1        0x8532
#define TC_PHY_CTL2        0x8533
#define TC_PHY_EN          0x8534
#define TC_PHY_BIAS        0x8536
#define TC_PHY_CSQ         0x853F
#define TC_SYS_FREQ0       0x8540
#define TC_SYS_FREQ1       0x8541
#define TC_DDC_CTL         0x8543
#define TC_HPD_CTL         0x8544
#define TC_AVM_CTL         0x8546
#define TC_HDMI_DET        0x8552
#define TC_HDCP_MODE       0x8560
#define TC_VI_MODE         0x8570
#define TC_VOUT_SET2       0x8573
#define TC_VOUT_SET3       0x8574
#define TC_VI_REP          0x8576
#define TC_FH_MIN0         0x85AA
#define TC_FH_MIN1         0x85AB
#define TC_FH_MAX0         0x85AC
#define TC_FH_MAX1         0x85AD
#define TC_HV_RST          0x85AF
#define TC_EDID_MODE       0x85C7
/* HDMI audio (8-bit) — configured to driver defaults, all muted */
#define TC_FORCE_MUTE      0x8600
#define TC_AUTO_CMD0       0x8602
#define TC_AUTO_CMD1       0x8603
#define TC_AUTO_CMD2       0x8604
#define TC_BUFINIT_START   0x8606
#define TC_FS_MUTE         0x8607
#define TC_FS_IMODE        0x8620
#define TC_LOCKDET_REF0    0x8630
#define TC_LOCKDET_REF1    0x8631
#define TC_LOCKDET_REF2    0x8632
#define TC_ACR_MODE        0x8640
#define TC_ACR_MDF0        0x8641
#define TC_ACR_MDF1        0x8642
#define TC_SDO_MODE1       0x8652
#define TC_DIV_MODE        0x8665
#define TC_NCO_F0_MOD      0x8670
/* Infoframe packet handling (8-bit) */
#define TC_PK_INT_MODE     0x8709
#define TC_NO_PKT_LIMIT    0x870B
#define TC_NO_PKT_CLR      0x870C
#define TC_ERR_PK_LIMIT    0x870D
#define TC_NO_PKT_LIMIT2   0x870E
#define TC_NO_GDB_LIMIT    0x9007

/* SYSCTL bits */
#define TC_MASK_IRRST      0x0800
#define TC_MASK_CECRST     0x0400
#define TC_MASK_CTXRST     0x0200
#define TC_MASK_HDMIRST    0x0100
#define TC_MASK_SLEEP      0x0001
/* CONFCTL bits */
#define TC_MASK_AUDCHNUM_2    0x0C00
#define TC_MASK_YCBCRFMT      0x00C0   /* 0 = RGB888 */
#define TC_MASK_AUDOUTSEL_I2S 0x0010
#define TC_MASK_AUTOINDEX     0x0004
#define TC_MASK_ABUFEN        0x0002
#define TC_MASK_VBUFEN        0x0001
/* PLLCTL1 bits */
#define TC_MASK_PLL_FRS    0x0C00      /* 0 = >500MHz hsck */
#define TC_MASK_CKEN       0x0010
#define TC_MASK_RESETB     0x0002
#define TC_MASK_PLL_EN     0x0001
/* CSI_CONFW op/addr fields */
#define TC_MODE_SET        0xA0000000u
#define TC_MODE_CLEAR      0xC0000000u
#define TC_ADDR_CSI_CONTROL    0x03000000u
#define TC_ADDR_CSI_INT_ENA    0x06000000u
#define TC_ADDR_CSI_ERR_INTENA 0x14000000u
#define TC_ADDR_CSI_ERR_HALT   0x15000000u
#define TC_MASK_CSI_MODE   0x8000
#define TC_MASK_TXHSMD     0x0080
#define TC_MASK_NOL_2      0x0002
#define TC_MASK_INTER      0x00000004u
#define TC_MASK_INER       0x00000200u
#define TC_MASK_WCER       0x00000100u
#define TC_MASK_QUNK       0x00000010u
#define TC_MASK_TXBRK      0x00000002u
/* SYS_STATUS bits — the debug window into the HDMI RX */
#define TC_MASK_S_SYNC     0x80        /* video sync locked */
#define TC_MASK_S_AVMUTE   0x40
#define TC_MASK_S_HDMI     0x10        /* HDMI (vs DVI) detected */
#define TC_MASK_S_PHY_SCDT 0x08        /* PHY de-serializer lock */
#define TC_MASK_S_PHY_PLL  0x04
#define TC_MASK_S_TMDS     0x02        /* TMDS clock present */
#define TC_MASK_S_DDC5V    0x01

/* ============================================================
 * TWI0 master (PB6=SCL, PB7=SDA) — same engine as lib/si5351.c,
 * extended with multi-byte transfers for 16-bit reg addresses.
 * ============================================================ */
#define TC_TWI_BASE     0x01C2AC00
#define TC_TWI(reg)     (*(volatile uint32_t *)(TC_TWI_BASE + (reg)))
#define TC_TWI_DATA     0x08
#define TC_TWI_CNTR     0x0C
#define TC_TWI_STAT     0x10
#define TC_TWI_CCR      0x14
#define TC_TWI_SRST     0x18
#define TC_TWI_BUS_EN   (1 << 6)
#define TC_TWI_M_STA    (1 << 5)
#define TC_TWI_M_STP    (1 << 4)
#define TC_TWI_INT_FLAG (1 << 3)
#define TC_TWI_A_ACK    (1 << 2)

static inline int tc_twi_wait(void)
{
    for (volatile int i = 0; i < 100000; i++)
        if (TC_TWI(TC_TWI_CNTR) & TC_TWI_INT_FLAG) return 0;
    return -1;
}

static inline int tc_twi_start(void)
{
    TC_TWI(TC_TWI_CNTR) = TC_TWI_BUS_EN | TC_TWI_M_STA | TC_TWI_INT_FLAG;
    if (tc_twi_wait()) return -1;
    uint32_t s = TC_TWI(TC_TWI_STAT) & 0xFF;
    return (s == 0x08 || s == 0x10) ? 0 : -1;    /* START / repeated START */
}

static inline void tc_twi_stop(void)
{
    TC_TWI(TC_TWI_CNTR) = TC_TWI_BUS_EN | TC_TWI_M_STP | TC_TWI_INT_FLAG;
    for (volatile int i = 0; i < 10000; i++)
        if (!(TC_TWI(TC_TWI_CNTR) & TC_TWI_M_STP)) break;
}

static inline int tc_twi_send(uint8_t byte)
{
    TC_TWI(TC_TWI_DATA) = byte;
    TC_TWI(TC_TWI_CNTR) = TC_TWI_BUS_EN | TC_TWI_INT_FLAG;
    if (tc_twi_wait()) return -1;
    uint32_t s = TC_TWI(TC_TWI_STAT) & 0xFF;
    return (s == 0x18 || s == 0x28 || s == 0x40) ? 0 : -1; /* addr/data ACK */
}

static inline int tc_twi_recv(int ack)
{
    TC_TWI(TC_TWI_CNTR) = TC_TWI_BUS_EN | TC_TWI_INT_FLAG |
                          (ack ? TC_TWI_A_ACK : 0);
    if (tc_twi_wait()) return -1;
    return (int)(TC_TWI(TC_TWI_DATA) & 0xFF);
}

/* Write n data bytes (little-endian value order) to a 16-bit register */
static inline int tc_wr(uint16_t reg, const uint8_t *val, int n)
{
    if (tc_twi_start()) return -1;
    if (tc_twi_send(TC_I2C_ADDR << 1)) goto fail;
    if (tc_twi_send(reg >> 8)) goto fail;
    if (tc_twi_send(reg & 0xFF)) goto fail;
    for (int i = 0; i < n; i++)
        if (tc_twi_send(val[i])) goto fail;
    tc_twi_stop();
    return 0;
fail:
    tc_twi_stop();
    return -1;
}

static inline int tc_rd(uint16_t reg, uint8_t *val, int n)
{
    if (tc_twi_start()) return -1;
    if (tc_twi_send(TC_I2C_ADDR << 1)) goto fail;
    if (tc_twi_send(reg >> 8)) goto fail;
    if (tc_twi_send(reg & 0xFF)) goto fail;
    if (tc_twi_start()) goto fail;                    /* repeated START */
    if (tc_twi_send((TC_I2C_ADDR << 1) | 1)) goto fail;
    for (int i = 0; i < n; i++) {
        int b = tc_twi_recv(i < n - 1);               /* NACK last byte */
        if (b < 0) goto fail;
        val[i] = (uint8_t)b;
    }
    tc_twi_stop();
    return 0;
fail:
    tc_twi_stop();
    return -1;
}

static inline int tc_wr8(uint16_t reg, uint8_t v)  { return tc_wr(reg, &v, 1); }
static inline int tc_wr16(uint16_t reg, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    return tc_wr(reg, b, 2);
}
static inline int tc_wr32(uint16_t reg, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8),
                     (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    return tc_wr(reg, b, 4);
}
static inline uint8_t tc_rd8(uint16_t reg)
{
    uint8_t v = 0; tc_rd(reg, &v, 1); return v;
}
static inline uint16_t tc_rd16(uint16_t reg)
{
    uint8_t b[2] = {0, 0}; tc_rd(reg, b, 2);
    return (uint16_t)(b[0] | (b[1] << 8));
}

static inline void tc_delay(volatile int loops) { while (loops--) ; }

/* ============================================================
 * Bring-up sequence — mirrors tc358743_initial_setup() +
 * set_pll() + set_csi() + enable_stream() from the Linux driver
 * ============================================================ */

/* Pulse reset bits in SYSCTL (tc358743_reset) */
static inline void tc358743_reset_blocks(uint16_t mask)
{
    uint16_t sysctl = tc_rd16(TC_SYSCTL);
    tc_wr16(TC_SYSCTL, sysctl | mask);
    tc_delay(10000);
    tc_wr16(TC_SYSCTL, sysctl & ~mask);
}

static inline void tc358743_sleep(int on)
{
    uint16_t sysctl = tc_rd16(TC_SYSCTL);
    tc_wr16(TC_SYSCTL, on ? (sysctl | TC_MASK_SLEEP)
                          : (sysctl & ~TC_MASK_SLEEP));
}

/*
 * Full bridge init. Call once at boot, after the TWI0 pins are muxed
 * (this muxes them itself) and before anything else claims PB6/PB7.
 * Returns 0 on success, -1 if the chip doesn't answer / wrong ID.
 */
static inline int tc358743_init(void)
{
    /* --- TWI0 setup (clock gate, PB6/PB7 mux func 2, 100 kHz) --- */
    volatile uint32_t *ccu = (volatile uint32_t *)0x01C20000;
    volatile uint32_t *pio = (volatile uint32_t *)0x01C20800;
    ccu[0x6C / 4]  |= 1;                     /* TWI0 bus gate */
    ccu[0x2D8 / 4] |= 1;                     /* TWI0 reset de-assert */
    uint32_t cfg = pio[0x24 / 4];            /* PB_CFG0: PB6, PB7 */
    cfg &= ~((0x7u << 24) | (0x7u << 28));
    cfg |=  ((0x2u << 24) | (0x2u << 28));   /* func 2 = TWI0 */
    pio[0x24 / 4] = cfg;

    TC_TWI(TC_TWI_SRST) = 1;
    tc_delay(1000);
    TC_TWI(TC_TWI_SRST) = 0;
    TC_TWI(TC_TWI_CCR)  = (2 << 3) | 11;     /* ~100 kHz */
    TC_TWI(TC_TWI_CNTR) = TC_TWI_BUS_EN;

    /* --- Who's there? CHIPID high byte must be 0x00 --- */
    uint16_t id = tc_rd16(TC_CHIPID);
    if ((id & 0xFF00) != 0x0000) return -1;

    /* --- initial_setup: hold IR + CEC in reset, reset CSI-TX + HDMI-RX,
     *     leave sleep --- */
    uint16_t sysctl = tc_rd16(TC_SYSCTL);
    tc_wr16(TC_SYSCTL, sysctl | TC_MASK_IRRST | TC_MASK_CECRST);
    tc358743_reset_blocks(TC_MASK_CTXRST | TC_MASK_HDMIRST);
    tc358743_sleep(0);

    tc_wr16(TC_FIFOCTL, 374);   /* CSI FIFO level — driver's safe value */

    /* --- set_ref_clk (27 MHz) --- */
    uint32_t sys_freq = TC_REFCLK_HZ / 10000;          /* 2700 */
    tc_wr8(TC_SYS_FREQ0, sys_freq & 0xFF);
    tc_wr8(TC_SYS_FREQ1, (sys_freq >> 8) & 0xFF);
    tc_wr8(TC_PHY_CTL0, tc_rd8(TC_PHY_CTL0) & ~0x02);  /* <30MHz ref: 0 */
    uint32_t fh_min = TC_REFCLK_HZ / 100000;           /* 270 */
    uint32_t fh_max = (fh_min * 66) / 10;              /* 1782 */
    tc_wr8(TC_FH_MIN0, fh_min & 0xFF);
    tc_wr8(TC_FH_MIN1, (fh_min >> 8) & 0xFF);
    tc_wr8(TC_FH_MAX0, fh_max & 0xFF);
    tc_wr8(TC_FH_MAX1, (fh_max >> 8) & 0xFF);
    uint32_t lockdet = TC_REFCLK_HZ / 100;             /* 270000 */
    tc_wr8(TC_LOCKDET_REF0, lockdet & 0xFF);
    tc_wr8(TC_LOCKDET_REF1, (lockdet >> 8) & 0xFF);
    tc_wr8(TC_LOCKDET_REF2, (lockdet >> 16) & 0x0F);
    tc_wr8(TC_NCO_F0_MOD, 0x01);                       /* 27 MHz mode */
    uint16_t cec_freq = (656 * sys_freq) / 4200;
    tc_wr16(TC_CECHCLK, cec_freq);
    tc_wr16(TC_CECLCLK, cec_freq);

    /* --- DDC / EDID mode (EDID RAM mode, though we never load one) --- */
    tc_wr8(TC_DDC_CTL, (tc_rd8(TC_DDC_CTL) & ~0x03) | 0x02); /* 100ms */
    tc_wr8(TC_EDID_MODE, (tc_rd8(TC_EDID_MODE) & ~0x03) | 0x02);

    /* --- set_hdmi_phy (REF_02 "Source HDMI" defaults) --- */
    tc_wr8(TC_PHY_EN, tc_rd8(TC_PHY_EN) & ~0x01);      /* PHY off */
    tc_wr8(TC_PHY_CTL1, ((1600 / 200) << 4) | (1 - 1));/* auto-rst 1.6ms */
    tc_wr8(TC_PHY_CTL2, tc_rd8(TC_PHY_CTL2) & ~0x07);  /* no auto resets */
    tc_wr8(TC_PHY_BIAS, 0x40);
    tc_wr8(TC_PHY_CSQ, 0x0A);
    tc_wr8(TC_AVM_CTL, 45);
    tc_wr8(TC_HDMI_DET, tc_rd8(TC_HDMI_DET) & ~0x30);  /* det delay: sync */
    tc_wr8(TC_HV_RST, tc_rd8(TC_HV_RST) & ~0x30);      /* no H/V PI reset */
    tc_wr8(TC_PHY_EN, tc_rd8(TC_PHY_EN) | 0x01);       /* PHY on */

    /* --- HDCP off (manual authentication = never authenticate) --- */
    tc_wr8(TC_HDCP_MODE, (tc_rd8(TC_HDCP_MODE) & ~0x02) | 0x02);

    /* --- audio block: driver defaults, output muted/idle --- */
    tc_wr8(TC_FORCE_MUTE, 0x00);
    tc_wr8(TC_AUTO_CMD0, 0xF3);
    tc_wr8(TC_AUTO_CMD1, 0x02);
    tc_wr8(TC_AUTO_CMD2, 0x0C);
    tc_wr8(TC_BUFINIT_START, 500 / 100);
    tc_wr8(TC_FS_MUTE, 0x00);
    tc_wr8(TC_FS_IMODE, 0x22);
    tc_wr8(TC_ACR_MODE, 0x01);
    tc_wr8(TC_ACR_MDF0, 0x60 | 0x05);
    tc_wr8(TC_ACR_MDF1, 0x07);
    tc_wr8(TC_SDO_MODE1, 0x02);
    tc_wr8(TC_DIV_MODE, (100 / 100) << 4);
    /* CONFCTL audio routing bits (or'd, per driver) */
    tc_wr16(TC_CONFCTL, tc_rd16(TC_CONFCTL) | TC_MASK_AUDCHNUM_2 |
            TC_MASK_AUDOUTSEL_I2S | TC_MASK_AUTOINDEX);

    /* --- infoframe packet handling defaults --- */
    tc_wr8(TC_PK_INT_MODE, 0xFF);
    tc_wr8(TC_NO_PKT_LIMIT, 0x2C);
    tc_wr8(TC_NO_PKT_CLR, 0x53);
    tc_wr8(TC_ERR_PK_LIMIT, 0x01);
    tc_wr8(TC_NO_PKT_LIMIT2, 0x30);
    tc_wr8(TC_NO_GDB_LIMIT, 0x10);

    /* --- video path: DVI sources are RGB full range; auto color out,
     *     RGB888 (YCBCRFMT = 0), no 422 --- */
    tc_wr8(TC_VI_MODE, tc_rd8(TC_VI_MODE) & ~0x08);
    tc_wr8(TC_VOUT_SET2, (tc_rd8(TC_VOUT_SET2) & ~0x03) | 0x01);
    tc_wr8(TC_VOUT_SET3, 0x08);
    tc_wr8(TC_VOUT_SET2, tc_rd8(TC_VOUT_SET2) & ~(0x80 | 0x40));
    tc_wr8(TC_VI_REP, tc_rd8(TC_VI_REP) & ~0xE0);      /* RGB full */
    tc_wr16(TC_CONFCTL, tc_rd16(TC_CONFCTL) & ~TC_MASK_YCBCRFMT);

    /* --- set_pll: PRD/FBD for 594 Mbps/lane, FRS=0 (hsck>500M) --- */
    tc358743_sleep(1);
    tc_wr16(TC_PLLCTL0, ((TC_PLL_PRD - 1) << 12) | ((TC_PLL_FBD - 1) & 0x1FF));
    uint16_t pll1 = tc_rd16(TC_PLLCTL1);
    pll1 &= ~(TC_MASK_PLL_FRS | TC_MASK_RESETB | TC_MASK_PLL_EN);
    pll1 |= (0 << 10) | TC_MASK_RESETB | TC_MASK_PLL_EN;
    tc_wr16(TC_PLLCTL1, pll1);
    tc_delay(1000);                                    /* >10 µs */
    tc_wr16(TC_PLLCTL1, tc_rd16(TC_PLLCTL1) | TC_MASK_CKEN);
    tc358743_sleep(0);

    /* --- set_csi: 2 lanes, 594 Mbps D-PHY timing set (REF_02) --- */
    tc358743_reset_blocks(TC_MASK_CTXRST);
    tc_wr32(TC_D2W_CNTRL, 0x00000001);                 /* lanes 2,3 off */
    tc_wr32(TC_D3W_CNTRL, 0x00000001);
    tc_wr32(TC_LINEINITCNT,    0x00000E80);
    tc_wr32(TC_LPTXTIMECNT,    0x00000003);
    tc_wr32(TC_TCLK_HEADERCNT, 0x00001403);
    tc_wr32(TC_TCLK_TRAILCNT,  0x00000000);
    tc_wr32(TC_THS_HEADERCNT,  0x00000103);
    tc_wr32(TC_TWAKEUP,        0x00004882);
    tc_wr32(TC_TCLK_POSTCNT,   0x00000008);
    tc_wr32(TC_THS_TRAILCNT,   0x00000002);
    tc_wr32(TC_HSTXVREGCNT,    0x00000000);
    tc_wr32(TC_HSTXVREGEN, 0x01 | 0x02 | 0x04);        /* CLK + D0 + D1 */
    tc_wr32(TC_TXOPTIONCNTRL, 0x00000001);             /* continuous clk */
    tc_wr32(TC_STARTCNTRL, 0x00000001);
    tc_wr32(TC_CSI_START,  0x00000001);
    tc_wr32(TC_CSI_CONFW, TC_MODE_SET | TC_ADDR_CSI_CONTROL |
            TC_MASK_CSI_MODE | TC_MASK_TXHSMD | TC_MASK_NOL_2);
    tc_wr32(TC_CSI_CONFW, TC_MODE_SET | TC_ADDR_CSI_ERR_INTENA |
            TC_MASK_TXBRK | TC_MASK_QUNK | TC_MASK_WCER | TC_MASK_INER);
    tc_wr32(TC_CSI_CONFW, TC_MODE_CLEAR | TC_ADDR_CSI_ERR_HALT |
            TC_MASK_TXBRK | TC_MASK_QUNK);
    tc_wr32(TC_CSI_CONFW, TC_MODE_SET | TC_ADDR_CSI_INT_ENA | TC_MASK_INTER);

    /* --- open the video buffer: frames flow HDMI → CSI --- */
    tc_wr16(TC_CONFCTL, tc_rd16(TC_CONFCTL) | TC_MASK_VBUFEN);

    return 0;
}

/*
 * Debug: SYS_STATUS snapshot. Healthy Pico-connected state is
 * S_TMDS | S_PHY_PLL | S_PHY_SCDT | S_SYNC (0x8E); S_HDMI stays 0
 * because PicoDVI transmits DVI. S_DDC5V set only if +5V is wired.
 */
static inline uint8_t tc358743_status(void)
{
    return tc_rd8(TC_SYS_STATUS);
}

#endif
