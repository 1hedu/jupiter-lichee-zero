/*
 * Jupiter SDK — video.c
 * Display initialization and buffer management.
 *
 * All register values matched to U-Boot 2017.01-rc2 known-working config.
 * Panel: HY0430IPS04-04, 480x272, RGB parallel.
 *
 * Layer architecture:
 *   VI0 (pipe 0, ch 0): Game world, XRGB8888, double-buffered
 *   UI0 (pipe 1, ch 2): Overlay/HUD, ARGB8888, per-pixel alpha
 */
#include "jupiter.h"

static void delay(volatile uint32_t n) { while (n--); }

static void ccu_init(void)
{
    CCU_PLL_VIDEO_CTRL = 0x91004107;  /* ~198 MHz */
    while (!(CCU_PLL_VIDEO_CTRL & PLL_LOCK))
        ;
    CCU_BUS_CLK_GATE1 |= GATE1_DE | GATE1_TCON;
    CCU_BUS_RST1      |= RST1_DE  | RST1_TCON;
    CCU_DE_CLK   = 0x82000003;
    CCU_TCON_CLK = 0x80000000;
}

static void gpio_init(void)
{
    PE_CFG0 = 0x33333303;  /* PE1 = GPIO (DE unused by panel), PE0 = LCD_CLK */
    PE_CFG1 = LCD_FUNC_ALL;
    PE_CFG2 = LCD_FUNC_ALL;
}

static void de2_init(void)
{
    DE2_RST      = MIX0_BIT;
    DE2_BUS_GATE = MIX0_BIT;
    DE2_GATE     = MIX0_BIT;
    DE2_DIV      = 0;
    delay(100000);

    /* Zero mixer — critical on cold boot */
    {
        volatile uint32_t *p = (volatile uint32_t *)MIXER0_BASE;
        for (uint32_t i = 0; i < 0xC000 / 4; i++)
            p[i] = 0;
    }

    MIX_GLB_CTL  = MIX_EN;
    MIX_GLB_SIZE = WH(LCD_W, LCD_H);

    /* VI0 — game world */
    VI_ATTR(0)       = 0x00008401;  /* enabled, XRGB8888 */
    VI_MBSIZE(0)     = WH(LCD_W, LCD_H);
    VI_COOR(0)       = 0;
    VI_PITCH0(0)     = LCD_PITCH;
    VI_TOP_LADDR0(0) = FB0_ADDR;
    VI_OVL_SIZE(0)   = WH(LCD_W, LCD_H);

    /* UI0 — overlay (per-pixel alpha) */
    UI_ATTR(0)  = UI_EN | UI_FMT_ARGB8888 | UI_GALPHA(0xFF);
    UI_SIZE(0)  = WH(LCD_W, LCD_H);
    UI_COORD(0) = 0;
    UI_PITCH(0) = LCD_PITCH;
    UI_ADDR(0)  = OVL_ADDR;
    UI_OVL_SIZE = WH(LCD_W, LCD_H);

    /* Blender: pipe 0=VI0(ch0), pipe 1=UI0(ch2) */
    BLD_FCOLOR(0) = 0xFF000000;
    BLD_INSIZE(0) = WH(LCD_W, LCD_H);
    BLD_OFFSET(0) = 0;
    BLD_MODE(0)   = BLEND_DEF;
    BLD_FCOLOR(1) = 0x00000000;
    BLD_INSIZE(1) = WH(LCD_W, LCD_H);
    BLD_OFFSET(1) = 0;
    BLD_MODE(1)   = BLEND_DEF;
    BLD_BKCOLOR   = 0xFF000000;
    BLD_OUT_SIZE  = WH(LCD_W, LCD_H);
    BLD_ROUTE     = ROUTE_P(0, 0) | ROUTE_P(1, 2);
    BLD_PIPE_CTL  = PIPE_EN(0) | PIPE_EN(1) | PIPE_FC(0);

    MIX_GLB_DBUF = DBUF_EN;
}

static void tcon_init(void)
{
    TCON_GCTL = 0; TCON0_CTL = 0; TCON0_DCLK = 0;
    TCON0_BASIC0 = B0_XY(LCD_W, LCD_H);
    TCON0_BASIC1 = B1_HT_HBP(LCD_HT, LCD_HBP + LCD_HSW);
    TCON0_BASIC2 = B2_VT_VBP(LCD_VT, LCD_VBP + LCD_VSW);
    TCON0_BASIC3 = B3_HSPW_VSPW(LCD_HSW, LCD_VSW);
    TCON0_IO_POL = LCD_IO_POL;
    TCON0_IO_TRI = 0;
    TCON0_DCLK = 0xF0000000 | LCD_DCLK_DIV;
    TCON0_CTL  = TCON0_CTL_EN | TCON0_CTL_CLKDLY(LCD_CLK_DELAY);
    TCON_GCTL  = TCON_GCTL_EN;
}

/* ---- Public API ---- */

static void video_diag(void)
{
    /* Note: uart_puthex prints its own "0x" prefix — labels end at '=' */
    uart_puts("  [diag] PLL_VIDEO="); uart_puthex(CCU_PLL_VIDEO_CTRL);
    uart_puts(" locked="); uart_putdec(!!(CCU_PLL_VIDEO_CTRL & PLL_LOCK));
    uart_puts("\n  [diag] DE_CLK="); uart_puthex(CCU_DE_CLK);
    uart_puts(" TCON_CLK="); uart_puthex(CCU_TCON_CLK);
    uart_puts("\n  [diag] MIX_GLB_CTL="); uart_puthex(MIX_GLB_CTL);
    uart_puts(" MIX_GLB_SIZE="); uart_puthex(MIX_GLB_SIZE);
    uart_puts("\n  [diag] VI0_ATTR="); uart_puthex(VI_ATTR(0));
    uart_puts(" VI0_LADDR="); uart_puthex(VI_TOP_LADDR0(0));
    uart_puts("\n  [diag] BLD_ROUTE="); uart_puthex(BLD_ROUTE);
    uart_puts(" BLD_PIPE_CTL="); uart_puthex(BLD_PIPE_CTL);
    uart_puts("\n  [diag] TCON_GCTL="); uart_puthex(TCON_GCTL);
    uart_puts(" TCON0_CTL="); uart_puthex(TCON0_CTL);
    uart_puts(" TCON0_DCLK="); uart_puthex(TCON0_DCLK);
    uart_puts("\n");
}

void video_init(void)
{
    /* Idempotent. The full init clears U-Boot's FB + FB0 + FB1 to black
     * and reconfigures DE2 / TCON from scratch — destructive if called
     * a second time inside the menu's demo wrapper after the demo (or
     * the menu) has already painted something. Second and later calls
     * just refresh the DE2 layer pointers back to the boot defaults
     * (FB0_ADDR + OVL_ADDR) so a demo that assumes fresh-boot routing
     * gets it, without nuking the framebuffers. */
    static int s_video_inited = 0;
    if (s_video_inited) {
        /* Re-assert PE pinmux: input demos (input_test, opn2_input,
         * opn2_hw_input, input_mt32) call input_init() BEFORE
         * video_init() in their boot sequence, expecting video_init's
         * gpio_init to clobber the PE pins back to LCD function — at
         * which point the per-poll n64_pin_input() / Genesis SELECT
         * writes re-assert their pin state. Skipping gpio_init on
         * re-entry left the PE pins in whatever state the menu had
         * (e.g. PE20 still configured as the menu's last N64 input
         * state, but other PE pins unset), and N64 polling failed
         * because some other PE state got out of sync.
         *
         * gpio_init is cheap (3 register writes) and non-destructive
         * to framebuffers. */
        gpio_init();
        VI_TOP_LADDR0(0) = FB0_ADDR;
        UI_ADDR(0)       = OVL_ADDR;
        MIX_GLB_DBUF     = DBUF_EN;
        uart_puts("[video] init re-entered — gpio_init + refreshed DE2 pointers\n");
        return;
    }
    s_video_inited = 1;

    /* Kill U-Boot's TCON immediately so it stops scanning out the
     * old framebuffer (penguin logo). Without this, the old image
     * bleeds through until our new config takes effect. */
    uart_puts("[video] killing U-Boot TCON...\n");
    TCON_GCTL  = 0;
    TCON0_CTL  = 0;
    TCON0_DCLK = 0;

    /* Clear U-Boot's framebuffer region and both of ours.
     * If D-cache is on (mmu_init ran first), these writes land in cache.
     * We must flush to DRAM so the DE2 DMA sees them. */
    uart_puts("[video] clearing framebuffers...\n");
    memset32(0x43E00000, 0, 0x200000 / 4);  /* U-Boot's FB */
    memset32(FB0_ADDR, 0, LCD_W * LCD_H);
    memset32(FB1_ADDR, 0, LCD_W * LCD_H);
    /* UI0 overlay buffers too: de2_init enables the UI0 layer scanning
     * OVL_ADDR with per-pixel alpha, so uninitialized DRAM there gets
     * composited over the game layer until the app first clears it. */
    memset32(OVL_ADDR, 0, LCD_W * LCD_H);
    memset32(OVL1_ADDR, 0, LCD_W * LCD_H);
    dcache_clean_range(0x43E00000, 0x200000);
    dcache_clean_range(FB0_ADDR, LCD_W * LCD_H * 4);
    dcache_clean_range(FB1_ADDR, LCD_W * LCD_H * 4);
    dcache_clean_range(OVL_ADDR, LCD_W * LCD_H * 4);
    dcache_clean_range(OVL1_ADDR, LCD_W * LCD_H * 4);

    uart_puts("[video] ccu_init...\n");
    ccu_init();
    uart_puts("[video] gpio_init...\n");
    gpio_init();
    uart_puts("[video] de2_init...\n");
    de2_init();
    uart_puts("[video] tcon_init...\n");
    tcon_init();

    uart_puts("[video] init complete. Register dump:\n");
    video_diag();
}

void video_swap(uint32_t fb_addr)
{
    VI_TOP_LADDR0(0) = fb_addr;
    MIX_GLB_DBUF = DBUF_EN;
}

void video_set_overlay(uint32_t ovl_addr)
{
    UI_ADDR(0) = ovl_addr;
    MIX_GLB_DBUF = DBUF_EN;
}

void video_commit(void)
{
    MIX_GLB_DBUF = DBUF_EN;
}

void video_vi1_init(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    /* VI1 (channel 1): small opaque positioned rectangle for PIP/label.
     * Rendered on blender pipe 2, above VI0 and UI0. */
    VI1_ATTR(0)       = 0x00008401;  /* enabled, XRGB8888 */
    VI1_MBSIZE(0)     = WH(w, h);
    VI1_COOR(0)       = 0;
    VI1_PITCH0(0)     = w * 4;
    VI1_TOP_LADDR0(0) = SPR_ADDR;
    VI1_OVL_SIZE(0)   = WH(w, h);

    /* Add pipe 2 to blender, routed to channel 1 (VI1).
     * Pipe 2 composites above pipes 0 and 1. */
    BLD_FCOLOR(2) = 0xFF000000;
    BLD_INSIZE(2) = WH(w, h);
    BLD_OFFSET(2) = WH(x, y);  /* screen position */
    BLD_MODE(2)   = BLEND_DEF;

    /* Update route: pipe0=ch0(VI0), pipe1=ch2(UI0), pipe2=ch1(VI1) */
    BLD_ROUTE    = ROUTE_P(0, 0) | ROUTE_P(1, 2) | ROUTE_P(2, 1);
    BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_EN(2) | PIPE_FC(0);

    MIX_GLB_DBUF = DBUF_EN;
}

void video_wait_vblank(void)
{
    /* TCON0_GINT0: bits 31:16 = IRQ enables (bit 31 = vblank), bits 15:0 =
     * pending flags (bit 15 = vblank). Pending flags are write-ZERO-to-
     * clear: to ack a flag, write the register back with that flag's bit
     * cleared (Linux sun4i_tcon does `status & ~bit`). Writing 1 to a
     * pending bit leaves it untouched, so read-modify-write with only the
     * target bit cleared preserves every other pending flag and all the
     * enable bits. */
    TCON0_GINT0 |= (1u << 31);  /* enable vblank flag (other bits written
                                 * back unchanged — 1s don't clear) */

    /* Ack any stale vblank: clear ONLY bit 15 (W0C) */
    TCON0_GINT0 = TCON0_GINT0 & ~(1u << 15);

    /* Wait for next vblank */
    while (!(TCON0_GINT0 & (1u << 15)));

    /* Ack it (W0C: write back with bit 15 cleared) */
    TCON0_GINT0 = TCON0_GINT0 & ~(1u << 15);
}

/* ================================================================
 *  Jupiter Modes — numbered hardware-native display configurations
 *  (an SNES homage: each mode is a fixed DE2 layer stack, zero
 *  software compositing — the mixer does all the work).
 *
 *    Mode 0  FLAT     VI0 only
 *    Mode 1  BG+OBJ   VI0 + UI0 alpha overlay        (video_init default)
 *    Mode 2  TRIPLANE VI0 + VI1 window + UI0
 *    Mode 3  GHOST    Mode 1 with programmable global alpha on UI0
 *    Mode 4  CINEMA   VI0 scans out NV12 directly (CSC in hardware),
 *                     UI0 stays ARGB on top — Cedar decode → glass
 *                     with zero CPU pixel work
 *    Mode 6  RASTER   Mode 1 + hstimer scanline hook (see hstimer.h);
 *                     register-poke per line, no redraws
 *    Mode 7  —        affine stays software on the V3s (the NEON
 *                     mode7_scanline helpers); video_mode(7) selects
 *                     the Mode 1 stack and the rest is up to you
 *
 *  Call after video_init(). See docs/VIDEO_MODES.md.
 * ================================================================ */

void video_mode(int mode)
{
    switch (mode) {
    case 0:  /* FLAT: only pipe 0 (VI0) */
        BLD_ROUTE    = ROUTE_P(0, 0);
        BLD_PIPE_CTL = PIPE_EN(0) | PIPE_FC(0);
        break;
    case 2:  /* TRIPLANE: caller positions VI1 via video_vi1_init() */
        BLD_ROUTE    = ROUTE_P(0, 0) | ROUTE_P(1, 2) | ROUTE_P(2, 1);
        BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_EN(2) | PIPE_FC(0);
        break;
    default: /* 1 / 3 / 6 / 7: the standard VI0 + UI0 stack */
        UI_ATTR(0)   = UI_EN | UI_FMT_ARGB8888 | UI_GALPHA(0xFF);
        BLD_ROUTE    = ROUTE_P(0, 0) | ROUTE_P(1, 2);
        BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_FC(0);
        break;
    }
    MIX_GLB_DBUF = DBUF_EN;
}

/* Mode 3 GHOST: hardware fade of the whole UI0 overlay (0=invisible,
 * 255=opaque). Per-pixel alpha still applies on top of this. */
void video_mode3_alpha(uint8_t alpha)
{
    UI_ATTR(0) = UI_EN | UI_FMT_ARGB8888 | UI_GALPHA(alpha);
    MIX_GLB_DBUF = DBUF_EN;
}

/* Mode 4 CINEMA: point VI0 straight at an NV12 frame (e.g. the Cedar
 * decoder's output — cedar_dec_luma_addr()/cedar_dec_chroma_addr()).
 * stride is the mb-aligned luma width; the frame is shown 1:1 in a
 * window at (x,y) (no scaler on V3s silicon). The hardware CSC
 * (BT601 limited range, per Linux sun8i_csc.c) converts during scan.
 * UNVERIFIED ON SILICON — see docs/VIDEO_MODES.md. */
void video_mode4_nv12(uint32_t luma, uint32_t chroma,
                      uint32_t w, uint32_t h, uint32_t stride,
                      uint32_t x, uint32_t y)
{
    static const uint32_t yuv2rgb_bt601[12] = {
        0x000004A8, 0x00000000, 0x00000662, 0xFFFC8451,
        0x000004A8, 0xFFFFFE6F, 0xFFFFFCC0, 0x00021E4D,
        0x000004A8, 0x00000811, 0x00000000, 0xFFFBACA9,
    };

    VI_ATTR(0)       = VI_ATTR_EN | VI_FMT_NV12;
    VI_MBSIZE(0)     = WH(w, h);
    VI_COOR(0)       = 0;
    VI_PITCH0(0)     = stride;        /* luma: 1 byte/px            */
    VI_PITCH1(0)     = stride;        /* interleaved UV, same bytes */
    VI_PITCH2(0)     = stride;
    VI_TOP_LADDR0(0) = luma;
    VI_TOP_LADDR1(0) = chroma;
    VI_OVL_SIZE(0)   = WH(w, h);

    /* Blender pipe 0 becomes a positioned window of the video frame.
     * OFFSET is plain (y<<16)|x — unlike the size regs, no minus-one
     * encoding (Linux sun8i BLEND_ATTR_COORD). */
    BLD_INSIZE(0) = WH(w, h);
    BLD_OFFSET(0) = (y << 16) | x;

    for (int i = 0; i < 12; i++)
        CSC_COEFF(CSC0_BASE, i) = yuv2rgb_bt601[i];
    CSC_CTRL(CSC0_BASE) = CSC_EN;

    MIX_GLB_DBUF = DBUF_EN;
}

/* Leave Mode 4: restore VI0 to full-screen XRGB scanout. */
void video_mode4_off(void)
{
    CSC_CTRL(CSC0_BASE) = 0;
    VI_ATTR(0)       = VI_ATTR_EN | VI_FMT_XRGB8888;
    VI_MBSIZE(0)     = WH(LCD_W, LCD_H);
    VI_PITCH0(0)     = LCD_PITCH;
    VI_TOP_LADDR0(0) = FB0_ADDR;
    VI_OVL_SIZE(0)   = WH(LCD_W, LCD_H);
    BLD_INSIZE(0) = WH(LCD_W, LCD_H);
    BLD_OFFSET(0) = 0;
    MIX_GLB_DBUF = DBUF_EN;
}
