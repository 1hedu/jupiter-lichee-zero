#ifndef JUPITER_V3S_H
#define JUPITER_V3S_H

#include <stdint.h>

#define BIT(n)              (1U << (n))
#define GENMASK(h, l)       (((1U << ((h)-(l)+1)) - 1) << (l))
#define REG32(addr)         (*(volatile uint32_t *)(addr))

/* Base addresses (dtsi + datasheet) */
#define CCU_BASE            0x01C20000
#define PIO_BASE            0x01C20800
#define TCON0_BASE          0x01C0C000
#define DE2_BASE            0x01000000
#define MIXER0_BASE         0x01100000
#define FB0_ADDR            0x43800000  /* VI0 game layer, buffer 0 */
#define FB1_ADDR            0x43900000  /* VI0 game layer, buffer 1 */
#define SPR_ADDR            0x43A00000  /* VI1 sprite layer          */
#define OVL_ADDR            0x43B00000  /* UI0 overlay layer, buf 0  */
#define OVL1_ADDR           0x43C00000  /* UI0 overlay layer, buf 1  */
#define UART0_BASE          0x01C28000

/* SRAM Controller */
#define SRAM_CTRL_BASE      0x01C00000
#define SRAM_CTRL_REG1      REG32(SRAM_CTRL_BASE + 0x04)

/* SRAM_C (CedarVE working memory, remappable to CPU via bit 24) */
#define SRAM_C_ADDR         0x01D00000

/* Watchdog (Timer module, offset 0xA0 from timer base 0x01C20C00) */
#define WDOG_BASE           0x01C20CA0
#define WDOG_IRQ_EN         REG32(WDOG_BASE + 0x00) /* IRQ enable        */
#define WDOG_IRQ_STA        REG32(WDOG_BASE + 0x04) /* IRQ status        */
#define WDOG_CTRL           REG32(WDOG_BASE + 0x10) /* Control           */
#define WDOG_CFG            REG32(WDOG_BASE + 0x14) /* Configuration     */
#define WDOG_MODE           REG32(WDOG_BASE + 0x18) /* Mode              */

/* UART0 registers (16550-compatible) */
#define UART0_THR           REG32(UART0_BASE + 0x00) /* TX holding    */
#define UART0_RBR           REG32(UART0_BASE + 0x00) /* RX buffer     */
#define UART0_LSR           REG32(UART0_BASE + 0x14) /* Line status   */
#define UART0_LSR_THRE      BIT(5)                    /* TX hold empty */
#define UART0_LSR_DR        BIT(0)                    /* Data ready    */

/* CCU: PLL */
#define CCU_PLL_VIDEO_CTRL  REG32(CCU_BASE + 0x0010)
#define PLL_EN              BIT(31)
#define PLL_LOCK            BIT(28)
#define PLL_N(n)            (((n) & 0x7F) << 8)
#define PLL_M(m)            (((m) & 0x0F) << 0)

/* CCU: bus gating */
#define CCU_BUS_CLK_GATE1   REG32(CCU_BASE + 0x0064)
#define GATE1_DE            BIT(12)
#define GATE1_TCON          BIT(4)

/* CCU: module clocks */
#define CCU_DE_CLK          REG32(CCU_BASE + 0x0104)
#define CCU_TCON_CLK        REG32(CCU_BASE + 0x0118)
#define SCLK_GATE           BIT(31)
#define SRC_PLL_VIDEO       (0U << 24)
#define DIV_M(m)            (((m) & 0x0F) << 0)

/* CCU: bus reset */
#define CCU_BUS_RST1        REG32(CCU_BASE + 0x02C4)
#define RST1_DE             BIT(12)
#define RST1_TCON           BIT(4)

/* PIO: Port E (LCD pins, port 4, offset 4*0x24=0x90) */
#define PE_CFG0             REG32(PIO_BASE + 0x90)
#define PE_CFG1             REG32(PIO_BASE + 0x94)
#define PE_CFG2             REG32(PIO_BASE + 0x98)
#define LCD_FUNC_ALL        0x33333333

/* TCON0 */
#define TCON_GCTL           REG32(TCON0_BASE + 0x00)
#define TCON_GCTL_EN        BIT(31)

#define TCON0_CTL           REG32(TCON0_BASE + 0x40)
#define TCON0_CTL_EN        BIT(31)
#define TCON0_CTL_CLKDLY(d) (((d) & 0x1F) << 4)

#define TCON0_DCLK          REG32(TCON0_BASE + 0x44)
#define DCLK_GATE           BIT(31)
#define DCLK_DIV(d)         (((d) & 0x7F) << 0)

#define TCON0_BASIC0        REG32(TCON0_BASE + 0x48)
#define TCON0_BASIC1        REG32(TCON0_BASE + 0x4C)
#define TCON0_BASIC2        REG32(TCON0_BASE + 0x50)
#define TCON0_BASIC3        REG32(TCON0_BASE + 0x54)
#define TCON0_GINT0         REG32(TCON0_BASE + 0x04)
#define TCON0_GINT1         REG32(TCON0_BASE + 0x08)
#define TCON0_IO_POL        REG32(TCON0_BASE + 0x88)
#define TCON0_IO_TRI        REG32(TCON0_BASE + 0x8C)

#define B0_XY(w,h)          ((((w)-1) & 0xFFF) << 16 | (((h)-1) & 0xFFF))
#define B1_HT_HBP(t,bp)     ((((t)-1) & 0x1FFF) << 16 | (((bp)-1) & 0xFFF))
#define B2_VT_VBP(t,bp)     (((((t)*2) & 0x1FFF) << 16) | ((((bp)-1) & 0x0FFF) << 0))
#define B3_HSPW_VSPW(h,v)   (((((h)-1) & 0x07FF) << 16) | ((((v)-1) & 0x07FF) << 0))

#define IO_DE_NEG           BIT(27)
#define IO_DCLK_POS         BIT(26)
#define IO_HS_POS           BIT(25)
#define IO_VS_POS           BIT(24)

/* DE2: internal CCU */
#define DE2_GATE            REG32(DE2_BASE + 0x00)
#define DE2_BUS_GATE        REG32(DE2_BASE + 0x04)
#define DE2_RST             REG32(DE2_BASE + 0x08)
#define DE2_DIV             REG32(DE2_BASE + 0x0C)
#define MIX0_BIT            BIT(0)

/* Mixer global */
#define MIX_GLB_CTL         REG32(MIXER0_BASE + 0x00)
#define MIX_GLB_STS         REG32(MIXER0_BASE + 0x04)
#define MIX_GLB_DBUF        REG32(MIXER0_BASE + 0x08)
#define MIX_GLB_SIZE        REG32(MIXER0_BASE + 0x0C)
#define MIX_EN              BIT(0)
#define DBUF_EN             BIT(0)

/* Blender (mixer + 0x1000) */
#define BLD                 (MIXER0_BASE + 0x1000)
#define BLD_PIPE_CTL        REG32(BLD + 0x00)
#define BLD_FCOLOR(x)       REG32(BLD + 0x04 + 0x10*(x))
#define BLD_INSIZE(x)       REG32(BLD + 0x08 + 0x10*(x))
#define BLD_OFFSET(x)       REG32(BLD + 0x0C + 0x10*(x))
#define BLD_ROUTE           REG32(BLD + 0x80)
#define BLD_BKCOLOR         REG32(BLD + 0x88)
#define BLD_OUT_SIZE        REG32(BLD + 0x8C)
#define BLD_MODE(x)         REG32(BLD + 0x90 + 0x04*(x))

#define PIPE_EN(n)          BIT(8 + (n))
#define PIPE_FC(n)          BIT(n)
#define BLEND_DEF           0x03010301
#define ROUTE_P(p,c)        (((c) & 0xF) << ((p)*4))

/* VI0 overlay (channel 0, mixer + 0x2000) — background layer */
#define VI0                 (MIXER0_BASE + 0x2000)
#define VI0_ATTR(ov)        REG32(VI0 + 0x00 + 0x30*(ov))
#define VI0_MBSIZE(ov)      REG32(VI0 + 0x04 + 0x30*(ov))
#define VI0_COOR(ov)        REG32(VI0 + 0x08 + 0x30*(ov))
#define VI0_PITCH0(ov)      REG32(VI0 + 0x0C + 0x30*(ov))
#define VI0_PITCH1(ov)      REG32(VI0 + 0x10 + 0x30*(ov))
#define VI0_PITCH2(ov)      REG32(VI0 + 0x14 + 0x30*(ov))
#define VI0_TOP_LADDR0(ov)  REG32(VI0 + 0x18 + 0x30*(ov))
#define VI0_TOP_LADDR1(ov)  REG32(VI0 + 0x1C + 0x30*(ov))
#define VI0_TOP_LADDR2(ov)  REG32(VI0 + 0x20 + 0x30*(ov))
#define VI0_OVL_SIZE(n)     REG32(VI0 + 0xE8 + 0x04*(n))

/* VI1 overlay (channel 1, mixer + 0x3000) — sprite layer */
#define VI1                 (MIXER0_BASE + 0x3000)
#define VI1_ATTR(ov)        REG32(VI1 + 0x00 + 0x30*(ov))
#define VI1_MBSIZE(ov)      REG32(VI1 + 0x04 + 0x30*(ov))
#define VI1_COOR(ov)        REG32(VI1 + 0x08 + 0x30*(ov))
#define VI1_PITCH0(ov)      REG32(VI1 + 0x0C + 0x30*(ov))
#define VI1_TOP_LADDR0(ov)  REG32(VI1 + 0x18 + 0x30*(ov))
#define VI1_OVL_SIZE(n)     REG32(VI1 + 0xE8 + 0x04*(n))

/* Backward compat — existing code uses VI_xxx for VI0 */
#define VI                  VI0
#define VI_ATTR(ov)         VI0_ATTR(ov)
#define VI_MBSIZE(ov)       VI0_MBSIZE(ov)
#define VI_COOR(ov)         VI0_COOR(ov)
#define VI_PITCH0(ov)       VI0_PITCH0(ov)
#define VI_PITCH1(ov)       VI0_PITCH1(ov)
#define VI_PITCH2(ov)       VI0_PITCH2(ov)
#define VI_TOP_LADDR0(ov)   VI0_TOP_LADDR0(ov)
#define VI_TOP_LADDR1(ov)   VI0_TOP_LADDR1(ov)
#define VI_TOP_LADDR2(ov)   VI0_TOP_LADDR2(ov)
#define VI_OVL_SIZE(n)      VI0_OVL_SIZE(n)

/* UI overlay — channel 2 on V3s (vi_num=2, so UI0 = channel 2)
 * Address = mixer + 0x2000 + 0x1000 * ch = mixer + 0x4000
 * NOT mixer + 0x3000 (that's VI1)
 */
#define UI                  (MIXER0_BASE + 0x4000)
#define UI_ATTR(ov)         REG32(UI + 0x00 + 0x20*(ov))
#define UI_SIZE(ov)         REG32(UI + 0x04 + 0x20*(ov))
#define UI_COORD(ov)        REG32(UI + 0x08 + 0x20*(ov))
#define UI_PITCH(ov)        REG32(UI + 0x0C + 0x20*(ov))
#define UI_ADDR(ov)         REG32(UI + 0x10 + 0x20*(ov))
#define UI_OVL_SIZE         REG32(UI + 0x88)

#define UI_EN               BIT(0)
#define UI_FMT_ARGB8888     (0U << 8)
#define UI_FMT_XRGB8888     (4U << 8)
#define UI_FMT_RGB888       (8U << 8)
#define UI_FMT_RGB565       (10U << 8)
#define UI_GALPHA(a)        (((a) & 0xFF) << 24)

/* Size helper (for MIX_GLB_SIZE, BLD_OUT_SIZE, BLD_INSIZE, UI_SIZE) */
#define WH(w,h)             ((((h)-1) << 16) | ((w)-1))

/* Panel: HY0430IPS04-04 — 4.3" 480×272 RGB parallel
 *
 * Clock infrastructure kept from U-Boot register dump (proven working):
 *   PLL_VIDEO  = 0x91004107 → ~198 MHz
 *   DE_CLK     = 0x82000003 → src=2, div=4
 *   TCON_CLK   = 0x80000000 → src=PLL_VIDEO, div=1
 *   IO_POL     = 0x10000000 → DCLK phase=1
 *
 * Only resolution, timing, and DCLK divider change from 800×480.
 * DCLK divider: 198 MHz / 22 = 9.0 MHz → 525*286*60 = 9.0 MHz ✓
 * CLK_DELAY: (VT - VH) = 286 - 272 = 14, capped to 30 max field
 */
#define LCD_W               480
#define LCD_H               272
#define LCD_HT              525     /* 480 + 2 + 1 + 42 = 525 */
#define LCD_HBP             42
#define LCD_HFP             2
#define LCD_HSW             1
#define LCD_VT              286     /* 272 + 2 + 1 + 11 = 286 */
#define LCD_VBP             11
#define LCD_VFP             2
#define LCD_VSW             1
#define LCD_DCLK_DIV        22      /* 198 MHz / 22 = 9.0 MHz */
#define LCD_CLK_DELAY       14      /* VT - VH = 286 - 272 */
#define LCD_IO_POL          0x10000000  /* DCLK phase=1, proven working */

#define LCD_BPP             4
#define LCD_PITCH           (LCD_W * LCD_BPP)
#define LCD_FB_BYTES        (LCD_PITCH * LCD_H)

/* ================================================================== */
/* GIC (ARM Generic Interrupt Controller, PL400)                       */
/* ================================================================== */

#define GICD_BASE           0x01C81000  /* Distributor */
#define GICC_BASE           0x01C82000  /* CPU Interface */

/* Distributor registers */
#define GICD_CTLR           REG32(GICD_BASE + 0x000) /* Control */
#define GICD_ISENABLER(n)   REG32(GICD_BASE + 0x100 + 4*(n)) /* Set-Enable */
#define GICD_ICENABLER(n)   REG32(GICD_BASE + 0x180 + 4*(n)) /* Clear-Enable */
#define GICD_IPRIORITYR(n)  REG32(GICD_BASE + 0x400 + 4*(n)) /* Priority */
#define GICD_ITARGETSR(n)   REG32(GICD_BASE + 0x800 + 4*(n)) /* Target */
#define GICD_ICFGR(n)       REG32(GICD_BASE + 0xC00 + 4*(n)) /* Config */

/* CPU Interface registers */
#define GICC_CTLR           REG32(GICC_BASE + 0x00) /* Control */
#define GICC_PMR            REG32(GICC_BASE + 0x04) /* Priority Mask */
#define GICC_IAR            REG32(GICC_BASE + 0x0C) /* Interrupt Acknowledge */
#define GICC_EOIR           REG32(GICC_BASE + 0x10) /* End of Interrupt */

/* DMA is GIC SPI 50 = IRQ ID 82 (SPIs start at ID 32) */
#define IRQ_DMA             82
#define IRQ_DMA_REG         (IRQ_DMA / 32)           /* ISENABLER register index = 2 */
#define IRQ_DMA_BIT         BIT(IRQ_DMA % 32)        /* Bit within register = BIT(18) */

/* DMA IRQ types (per-channel, 4 bits each in DMA_IRQ_EN/STAT) */
#define DMA_IRQ_HALF        BIT(0)  /* Half-transfer */
#define DMA_IRQ_PKG         BIT(1)  /* Package (descriptor) end */
#define DMA_IRQ_QUEUE       BIT(2)  /* Queue (chain) end */

/* ================================================================== */
/* Audio Codec + DMA (from audio branch)                               */
/* ================================================================== */

#define CODEC_BASE          0x01C22C00
#define CODEC_ANALOG_BASE   0x01C23000
#define DMA_BASE            0x01C02000

/* --- CCU: Audio PLL (offset 0x0008) --- */
#define CCU_PLL_AUDIO_CTRL  REG32(CCU_BASE + 0x0008)
#define CCU_PLL_AUDIO_PAT   REG32(CCU_BASE + 0x0284)

/* --- CCU: Bus gating & reset for audio codec --- */
#define CCU_BUS_CLK_GATE0   REG32(CCU_BASE + 0x0060)
#define CCU_BUS_CLK_GATE2   REG32(CCU_BASE + 0x0068)
#define CCU_BUS_RST0        REG32(CCU_BASE + 0x02C0)
#define CCU_BUS_RST3        REG32(CCU_BASE + 0x02D0)

#define GATE0_DMA           BIT(6)
#define GATE2_CODEC         BIT(0)
#define RST0_DMA            BIT(6)
#define RST3_CODEC          BIT(0)

/* --- CCU: Audio codec module clock (AC_DIG_CLK, offset 0x0140) --- */
#define CCU_AC_DIG_CLK      REG32(CCU_BASE + 0x0140)

/* --- Codec digital registers (offsets from CODEC_BASE) --- */
#define CODEC_DAC_DPC       REG32(CODEC_BASE + 0x00)
#define CODEC_DAC_FIFOC     REG32(CODEC_BASE + 0x04)
#define CODEC_DAC_FIFOS     REG32(CODEC_BASE + 0x08)
#define CODEC_DAC_TXDATA    REG32(CODEC_BASE + 0x20)
#define CODEC_DAC_CNT       REG32(CODEC_BASE + 0x40)
#define CODEC_DAC_DAP_CTL   REG32(CODEC_BASE + 0x60)
#define DAC_DAP_EN          (0x60000000U)

/* DAC_DPC bits */
#define DAC_DPC_EN_DA       BIT(31)
#define DAC_DPC_DVOL(v)     (((v) & 0x3F) << 12)

/* DAC_FIFOC bits */
#define DAC_FIFOC_FS(r)     (((r) & 0x7) << 29)
#define DAC_FIFOC_FIR_VER   BIT(28)
#define DAC_FIFOC_SEND_LAST BIT(26)
#define DAC_FIFOC_TX_FIFO_MODE BIT(24)
#define DAC_FIFOC_DRQ_CLR   BIT(21)
#define DAC_FIFOC_TX_TRIG(l) (((l) & 0x3F) << 8)
#define DAC_FIFOC_MONO      BIT(6)
#define DAC_FIFOC_TX_16BIT  0
#define DAC_FIFOC_TX_24BIT  BIT(5)
#define DAC_FIFOC_DRQ_EN    BIT(4)
#define DAC_FIFOC_FLUSH     BIT(0)

/* DAC_FIFOS bits */
#define DAC_FIFOS_TXE       BIT(23)
#define DAC_FIFOS_CNT_MASK  GENMASK(22, 8)
#define DAC_FIFOS_CNT_SHIFT 8

/* Sample rate codes */
#define AUDIO_RATE_48000    0
#define AUDIO_RATE_32000    1
#define AUDIO_RATE_24000    2
#define AUDIO_RATE_16000    3
#define AUDIO_RATE_12000    4
#define AUDIO_RATE_8000     5
#define AUDIO_RATE_192000   6
#define AUDIO_RATE_96000    7

/* --- Codec analog control --- */
#define CODEC_ANA_CTL       REG32(CODEC_ANALOG_BASE + 0x00)

/* --- DMA Engine (sun6i-compatible, 8 channels) --- */
#define DMA_IRQ_EN(n)       REG32(DMA_BASE + 0x00 + 0x04*(n))
#define DMA_IRQ_STAT(n)     REG32(DMA_BASE + 0x10 + 0x04*(n))
#define DMA_GATE            REG32(DMA_BASE + 0x20)
#define DMA_STA             REG32(DMA_BASE + 0x30)

#define DMA_CH_BASE(ch)     (DMA_BASE + 0x100 + (ch) * 0x40)
#define DMA_CH_EN(ch)       REG32(DMA_CH_BASE(ch) + 0x00)
#define DMA_CH_PAUSE(ch)    REG32(DMA_CH_BASE(ch) + 0x04)
#define DMA_CH_DESC(ch)     REG32(DMA_CH_BASE(ch) + 0x08)
#define DMA_CH_CFG(ch)      REG32(DMA_CH_BASE(ch) + 0x0C)
#define DMA_CH_CUR_SRC(ch)  REG32(DMA_CH_BASE(ch) + 0x10)
#define DMA_CH_CUR_DST(ch)  REG32(DMA_CH_BASE(ch) + 0x14)
#define DMA_CH_BCNT(ch)     REG32(DMA_CH_BASE(ch) + 0x18)
#define DMA_CH_PARA(ch)     REG32(DMA_CH_BASE(ch) + 0x1C)

typedef struct __attribute__((aligned(4))) {
    uint32_t cfg;
    uint32_t src_addr;
    uint32_t dst_addr;
    uint32_t byte_cnt;
    uint32_t param;
    uint32_t next;
} dma_desc_t;

#define DMA_CFG_SRC_DRQ(d)      (((d) & 0x1F) << 0)
#define DMA_CFG_SRC_BURST(b)    (((b) & 0x3) << 7)
#define DMA_CFG_SRC_WIDTH(w)    (((w) & 0x3) << 9)
#define DMA_CFG_SRC_LINEAR      (0U << 5)
#define DMA_CFG_SRC_IO          (1U << 5)
#define DMA_CFG_DST_DRQ(d)      (((d) & 0x1F) << 16)
#define DMA_CFG_DST_BURST(b)    (((b) & 0x3) << 23)
#define DMA_CFG_DST_WIDTH(w)    (((w) & 0x3) << 25)
#define DMA_CFG_DST_LINEAR      (0U << 21)
#define DMA_CFG_DST_IO          (1U << 21)

#define DRQ_DRAM            1
#define DRQ_CODEC           15

#define AUDIO_BUF_ADDR      0x43D00000
#define AUDIO_BUF_HALF      2048
#define AUDIO_BUF_SIZE      (AUDIO_BUF_HALF * 2 * 2)

#endif
