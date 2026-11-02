#ifndef JUPITER_PINE64_H
#define JUPITER_PINE64_H

#include <stdint.h>

#define BIT(n)              (1U << (n))
/* Full-width masks must not shift by 32; matches the corrected V3s API. */
#define GENMASK(h, l)       ((0xFFFFFFFFU >> (31 - (h))) & ~((1U << (l)) - 1U))
#ifndef REG32
#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

/* Allwinner A64. U-Boot owns the HDMI PHY and its established mode. */
#define CCU_BASE            0x01C20000
#define PIO_BASE            0x01C20800
#define TCON0_BASE          0x01C0C000
#define TCON1_BASE          0x01C0D000
#define DE2_BASE            0x01000000
#define MIXER0_BASE         0x01100000
#define FB0_ADDR            0x43800000  /* VI0 game layer, buffer 0 */
#define FB1_ADDR            0x43900000  /* VI0 game layer, buffer 1 */
#define SPR_ADDR            0x43A00000  /* UI1 PIP/sprite layer       */
#define OVL_ADDR            0x43B00000  /* UI0 overlay layer, buf 0  */
#define OVL1_ADDR           0x43C00000  /* UI0 overlay layer, buf 1  */
#define UART0_BASE          0x01C28000
#define UART1_BASE          0x01C28400      /* MIDI / general-purpose UART */
#define UART2_BASE          0x01C28800

/* SRAM Controller */
#define SRAM_CTRL_BASE      0x01C00000
#define SRAM_CTRL_REG1      REG32(SRAM_CTRL_BASE + 0x04)

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

/* UART1 registers (same 16550 layout at +0x400 offset) */
#define UART1_THR           REG32(UART1_BASE + 0x00) /* TX holding / DLL */
#define UART1_RBR           REG32(UART1_BASE + 0x00) /* RX buffer / DLL  */
#define UART1_DLL           REG32(UART1_BASE + 0x00) /* Divisor low      */
#define UART1_DLH           REG32(UART1_BASE + 0x04) /* Divisor high     */
#define UART1_IER           REG32(UART1_BASE + 0x04) /* IRQ enable       */
#define UART1_IIR           REG32(UART1_BASE + 0x08) /* IRQ identify     */
#define UART1_FCR           REG32(UART1_BASE + 0x08) /* FIFO control     */
#define UART1_LCR           REG32(UART1_BASE + 0x0C) /* Line control     */
#define UART1_MCR           REG32(UART1_BASE + 0x10) /* Modem control    */
#define UART1_LSR           REG32(UART1_BASE + 0x14) /* Line status      */
#define UART1_USR           REG32(UART1_BASE + 0x7C) /* UART status      */
#define UART1_LSR_THRE      BIT(5)
#define UART1_LSR_DR        BIT(0)
#define UART1_USR_TFNF      BIT(1)                   /* TX FIFO not full */
#define UART1_USR_RFNE      BIT(3)                   /* RX FIFO not empty*/
#define UART1_IER_ERBFI     BIT(0)                   /* RX data IRQ      */
#define UART1_LCR_DLAB      BIT(7)
#define UART1_LCR_8N1       (0x03)                   /* 8-bit, no parity, 1 stop */
#define UART1_FCR_FIFOE     BIT(0)
#define UART1_FCR_RFIFOR    BIT(1)
#define UART1_FCR_XFIFOR    BIT(2)
#define UART1_FCR_RT_QTR    (1u << 6)                /* RX trigger = 1/4 full */

/* CCU UART bus gates / resets (A64) */
#define CCU_BUS_CLK_GATE3   REG32(CCU_BASE + 0x006C)
#define CCU_BUS_RST4        REG32(CCU_BASE + 0x02D8)
#define GATE3_UART0         BIT(16)
#define GATE3_UART1         BIT(17)
#define GATE3_UART2         BIT(18)
#define RST4_UART0          BIT(16)
#define RST4_UART1          BIT(17)
#define RST4_UART2          BIT(18)

/* GIC IRQ IDs for UARTs (SPIs start at ID 32) */
#define IRQ_UART0           32
#define IRQ_UART1           33
#define IRQ_UART2           34

/* CCU: PLL */
#define CCU_PLL_VIDEO_CTRL  REG32(CCU_BASE + 0x0010)
#define PLL_EN              BIT(31)
#define PLL_LOCK            BIT(28)
#define PLL_N(n)            (((n) & 0x7F) << 8)
#define PLL_M(m)            (((m) & 0x0F) << 0)

/* CCU: bus gating */
#define CCU_BUS_CLK_GATE1   REG32(CCU_BASE + 0x0064)
#define GATE1_DE            BIT(12)
#define GATE1_TCON0         BIT(3)
#define GATE1_TCON1         BIT(4)

/* CCU: module clocks */
#define CCU_DE_CLK          REG32(CCU_BASE + 0x0104)
#define CCU_TCON0_CLK       REG32(CCU_BASE + 0x0118)
#define CCU_TCON1_CLK       REG32(CCU_BASE + 0x011C)
#define SCLK_GATE           BIT(31)
#define SRC_PLL_VIDEO       (0U << 24)
#define DIV_M(m)            (((m) & 0x0F) << 0)

/* CCU: bus reset */
#define CCU_BUS_RST1        REG32(CCU_BASE + 0x02C4)
#define RST1_DE             BIT(12)
#define RST1_TCON0          BIT(3)
#define RST1_TCON1          BIT(4)

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
#define BLD_CK_CTL          REG32(BLD + 0xB0)
#define BLD_CK_CFG          REG32(BLD + 0xB4)
#define BLD_CK_MAX(x)       REG32(BLD + 0xC0 + 0x04*(x))
#define BLD_CK_MIN(x)       REG32(BLD + 0xE0 + 0x04*(x))
#define BLD_PREMULTIPLY     REG32(BLD + 0x84)

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

#define VI_ATTR_EN          BIT(0)
#define VI_ATTR_RGB         BIT(15)
#define VI_FMT_XRGB8888     ((4U << 8) | VI_ATTR_RGB)
#define VI_FMT_NV12         (8U << 8)

/* A64 mixer0 CCSC00, per Linux sun50i_a64_mixer0_cfg/sun8i_csc.h.
 * A64 has no second VI channel in this mixer; do not alias V3s VI1. */
#define CSC0_BASE           (MIXER0_BASE + 0xAA050)
#define CSC_CTRL(base)      REG32((base) + 0x00)
#define CSC_COEFF(base,i)   REG32((base) + 0x10 + 4*(i))
#define CSC_EN              BIT(0)
#define VEP_FCE_EN          REG32(MIXER0_BASE + 0xA0000)
#define VEP_BWS_EN          REG32(MIXER0_BASE + 0xA2000)
#define VEP_LTI_EN          REG32(MIXER0_BASE + 0xA4000)
#define VEP_PEAK_EN         REG32(MIXER0_BASE + 0xA6000)
#define VEP_ASE_EN          REG32(MIXER0_BASE + 0xA8000)
#define VEP_FCC_EN          REG32(MIXER0_BASE + 0xAA000)
#define VEP_DCSC_EN         REG32(MIXER0_BASE + 0xB0000)

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

/* A64 mixer0 has one VI channel; UI0 is at +0x3000. */
#define UI                  (MIXER0_BASE + 0x3000)
#define UI_ATTR(ov)         REG32(UI + 0x00 + 0x20*(ov))
#define UI_SIZE(ov)         REG32(UI + 0x04 + 0x20*(ov))
#define UI_COORD(ov)        REG32(UI + 0x08 + 0x20*(ov))
#define UI_PITCH(ov)        REG32(UI + 0x0C + 0x20*(ov))
#define UI_ADDR(ov)         REG32(UI + 0x10 + 0x20*(ov))
#define UI_OVL_SIZE         REG32(UI + 0x88)

/* A64 UI1 supplies the RGB PIP / second playfield. Its slots have a
 * 0x20 stride, unlike the VI slot's 0x30, and ATTR has no RGB-select bit. */
#define PINE64_UI1_BASE     (MIXER0_BASE + 0x4000)
#define PINE64_UI1_ATTR(ov) REG32(PINE64_UI1_BASE + 0x00 + 0x20*(ov))
#define PINE64_UI1_SIZE(ov) REG32(PINE64_UI1_BASE + 0x04 + 0x20*(ov))
#define PINE64_UI1_COORD(ov) REG32(PINE64_UI1_BASE + 0x08 + 0x20*(ov))
#define PINE64_UI1_PITCH(ov) REG32(PINE64_UI1_BASE + 0x0C + 0x20*(ov))
#define PINE64_UI1_ADDR(ov) REG32(PINE64_UI1_BASE + 0x10 + 0x20*(ov))
#define PINE64_UI1_OVL_SIZE REG32(PINE64_UI1_BASE + 0x88)

#define UI_EN               BIT(0)
#define UI_FMT_ARGB8888     (0U << 8)
#define UI_FMT_XRGB8888     (4U << 8)
#define UI_FMT_RGB888       (8U << 8)
#define UI_FMT_RGB565       (10U << 8)
#define UI_GALPHA(a)        (((a) & 0xFF) << 24)
#define UI_AMODE_PIXEL      (0U << 1)
#define UI_AMODE_LAYER      (1U << 1)
#define UI_AMODE_COMBINED   (2U << 1)

/* Size helper (for MIX_GLB_SIZE, BLD_OUT_SIZE, BLD_INSIZE, UI_SIZE) */
#define WH(w,h)             ((((h)-1) << 16) | ((w)-1))

/* Jupiter logical canvas, centered within the inherited HDMI output. */
#define LCD_W               480
#define LCD_H               272

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
/* DMA register layout shared by the A64 board audio implementation. */
/* ================================================================== */
#define DMA_BASE            0x01C02000

/* --- DMA Engine (sun6i-compatible, 8 channels used by this SDK) --- */
#define DMA_IRQ_EN(n)       REG32(DMA_BASE + 0x00 + 0x04*(n))
#define DMA_IRQ_STAT(n)     REG32(DMA_BASE + 0x10 + 0x04*(n))
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
#define DMA_CFG_SRC_BURST(b)    (((b) & 0x3) << 6)
#define DMA_CFG_SRC_WIDTH(w)    (((w) & 0x3) << 9)
#define DMA_CFG_SRC_LINEAR      (0U << 5)
#define DMA_CFG_SRC_IO          (1U << 5)
#define DMA_CFG_DST_DRQ(d)      (((d) & 0x1F) << 16)
#define DMA_CFG_DST_BURST(b)    (((b) & 0x3) << 22)
#define DMA_CFG_DST_WIDTH(w)    (((w) & 0x3) << 25)
#define DMA_CFG_DST_LINEAR      (0U << 21)
#define DMA_CFG_DST_IO          (1U << 21)

#define DRQ_DRAM            1
#define DRQ_CODEC           15

#define AUDIO_BUF_ADDR      0x43D00000
#define AUDIO_BUF_HALF      2048
#define AUDIO_BUF_SIZE      (AUDIO_BUF_HALF * 2 * 2)

/* Services are polled in normal thread context; never from an IRQ. */
void pine64_service(void);
void pine64_audio_stop(void);
const char *pine64_cedar_encode_path(void);
void pine64_system_reset(void) __attribute__((noreturn));
void pine64_notice(const char *title, const char *details);
void pine64_demo_active(int active);

#endif
