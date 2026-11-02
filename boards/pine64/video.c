/*
 * A64 DE2 HDMI handover.
 *
 * Reconstructs the physically verified SD2 display configuration. The HDMI
 * PHY, clocks and TCON timings remain owned by the inherited U-Boot mode.
 * A64 mixer0 has VI0 + UI0 + UI1 + UI2; its UI0 is channel 1, not V3s ch2.
 *
 * Register references: U-Boot v2025.10 drivers/video/sunxi/{sunxi_de2,lcdc}.c;
 * Linux drivers/gpu/drm/sun4i/sun8i_vi_scaler.{c,h}; A64 manual section 6.2.
 */
#include "jupiter.h"
#include "pine64_video.h"
#include "hstimer.h"

#define DREG(off) REG32(0x01000000u + (off))
#define MREG(off) REG32(0x01100000u + (off))
#define TREG(off) REG32(0x01c0d000u + (off))
#define CREG(off) REG32(0x01c20000u + (off))
#define VREG(off) MREG(0x20000u + (off))
#define UIREG(n, off) MREG(0x3000u + (n) * 0x1000u + (off))
#define SIZE(w, h) ((((h) - 1u) << 16) | ((w) - 1u))
#define XY(x, y) (((y) << 16) | (x))
#define VBLANK_ENABLE (1u << 30)
#define VBLANK_PENDING (1u << 14)

static int ready;
static int warned_timeout;
static uint32_t output_w, output_h, canvas_x, canvas_y;
static uint32_t total_lines, frame_ticks;
static const int16_t *m7_offsets;
static uint32_t m7_count, m7_band_lines, m7_base;
static volatile uint32_t m7_band;
void pine64_service(void) __attribute__((weak));

static void service_foreground(void)
{
#ifndef PINE64_HOST_TEST
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    if ((cpsr & 31u) != 0x13u) return; /* never poll USB from an IRQ */
#endif
    if (pine64_service) pine64_service();
}

static void display_barrier(void)
{
#ifndef PINE64_HOST_TEST
    __asm__ volatile("dsb sy" ::: "memory");
#endif
}

static int framebuffer_valid(uint32_t address, uint32_t bytes)
{
    return !(address & 3u) && address >= 0x40000000u &&
           bytes <= 0x80000000u && address <= 0xc0000000u - bytes;
}

static int inherited_mode_valid(void)
{
    /* Do not touch registers behind a disabled clock. HDMI reset bits10/11
     * are checked as well as the DE and TCON reset bits. */
    if ((CREG(0x064) & 0x1810u) != 0x1810u ||
        (CREG(0x2c4) & 0x1c10u) != 0x1c10u ||
        !(CREG(0x104) & (1u << 31)) ||
        !(CREG(0x11c) & (1u << 31)) ||
        !(CREG(0x150) & (1u << 31))) return 0;

    uint32_t ctl = TREG(0x90);
    if (!(TREG(0x00) & (1u << 31)) || !(ctl & (1u << 31)) ||
        (ctl & (1u << 20))) return 0;
    uint32_t src = TREG(0x94) & 0x0fff0fffu;
    if (src != (TREG(0x98) & 0x0fff0fffu) ||
        src != (TREG(0x9c) & 0x0fff0fffu)) return 0;
    output_w = ((src >> 16) & 0xfffu) + 1u;
    output_h = (src & 0xfffu) + 1u;
    if (output_w < LCD_W || output_h < LCD_H ||
        output_w > 4096 || output_h > 4096) return 0;
    canvas_x = (output_w - LCD_W) / 2u;
    canvas_y = (output_h - LCD_H) / 2u;
    total_lines = ((TREG(0xa4) >> 16) & 0x1fffu) / 2u;
    if (total_lines < output_h) total_lines = 0;
    return 1;
}

static int wait_for_vblank(void)
{
    /* TCON flags are W0C, not W1C. Retain other pending flags/enables. */
    TREG(0x04) = (TREG(0x04) | VBLANK_ENABLE) & ~VBLANK_PENDING;
    display_barrier();
    uint32_t start = timer_read();
    for (uint32_t count = 0; count < 2000000u; ++count) {
        if (TREG(0x04) & VBLANK_PENDING) {
            TREG(0x04) &= ~VBLANK_PENDING;
            display_barrier();
            return 1;
        }
        if (timer_elapsed(start, timer_read()) >= 2400000u) break;
    }
    return 0;
}

static void vblank_timeout(void)
{
    if (!warned_timeout) {
        uart_puts("[video] HDMI vblank timed out; continuing without sync\n");
        warned_timeout = 1;
    }
}

static void set_default_layers(void)
{
    /* Restore every field touched by scaler/PIP demos on menu re-entry. */
    VREG(0x00) = 0;
    CSC_CTRL(CSC0_BASE) = 0;
    BLD_CK_CTL = BLD_CK_CFG = 0;
    for (uint32_t i = 1; i < 4; ++i) {
        VI_ATTR(i) = 0;
        UI_ATTR(i) = 0;
        PINE64_UI1_ATTR(i) = 0;
    }
    UIREG(1, 0x00) = 0;
    UIREG(2, 0x00) = 0;
    MREG(0x2000) = 0x00008401u; /* RGB select + XRGB8888 + enable */
    MREG(0x2004) = SIZE(LCD_W, LCD_H);
    MREG(0x2008) = 0;
    MREG(0x200c) = LCD_PITCH;
    MREG(0x2018) = FB0_ADDR;
    MREG(0x20e8) = SIZE(LCD_W, LCD_H);
    MREG(0x20f0) = MREG(0x20f4) = 0; /* coarse horizontal downsampling */
    MREG(0x20f8) = MREG(0x20fc) = 0; /* coarse vertical downsampling */
    /* Latest SDK correction: per-pixel times global alpha, so GHOST
     * and raster alpha actually respond to GALPHA. */
    UIREG(0, 0x00) = UI_EN | UI_FMT_ARGB8888 |
                      UI_AMODE_COMBINED | UI_GALPHA(255);
    UIREG(0, 0x04) = SIZE(LCD_W, LCD_H);
    UIREG(0, 0x08) = 0;
    UIREG(0, 0x0c) = LCD_PITCH;
    UIREG(0, 0x10) = OVL_ADDR;
    UIREG(0, 0x88) = SIZE(LCD_W, LCD_H);
    for (uint32_t i = 0; i < 2; ++i) {
        MREG(0x1004 + i * 0x10) = i ? 0 : 0xff000000u;
        MREG(0x1008 + i * 0x10) = SIZE(LCD_W, LCD_H);
        MREG(0x100c + i * 0x10) = XY(canvas_x, canvas_y);
        MREG(0x1090 + i * 4) = 0x03010301u;
    }
    MREG(0x1084) = 0; /* straight alpha */
    MREG(0x1088) = 0xff000000u;
    MREG(0x108c) = SIZE(output_w, output_h);
    MREG(0x10fc) = 0;
    MREG(0x1080) = 0x10; /* pipe0 VI0 ch0, pipe1 UI0 ch1 */
    MREG(0x1000) = 0x301; /* both pipes, opaque pipe0 base */
    display_barrier();
    MREG(0x08) = 1;
}

static void mixer0_init(void)
{
    uint32_t div = DREG(0x0c);
    if (!(DREG(0x10) & 1u))
        DREG(0x0c) = (div & ~15u) | ((div >> 4) & 15u);
    /* A64 SRAM_C must remain assigned to DE2. */
    REG32(0x01c00004u) &= ~(1u << 24);
    DREG(0x08) |= 1;
    DREG(0x04) |= 1;
    DREG(0x00) |= 1;
    display_barrier();
    MREG(0x00) = 0;
    for (uint32_t off = 0; off < 0x6000; off += 4) MREG(off) = 0;
    static const uint32_t bypass[] = {
        0x20000, 0x30000, 0x40000, 0x50000,
        0xa0000, 0xa2000, 0xa4000, 0xa6000, 0xa8000, 0xaa000, 0xb0000
    };
    for (uint32_t i = 0; i < sizeof(bypass) / sizeof(bypass[0]); ++i)
        MREG(bypass[i]) = 0;
    MREG(0x00) = 1;
    MREG(0x0c) = SIZE(output_w, output_h);
    set_default_layers();
    if (!wait_for_vblank()) vblank_timeout();
    /* A64 crossbar bit0=1 sends mixer0 to TCON1 and mixer1 to TCON0. */
    DREG(0x10) |= 1;
    display_barrier();
    MREG(0x08) = 1;
}

void video_init(void)
{
    if (ready) {
        video_mode7_lineshift_off();
        set_default_layers();
        return;
    }
    if (!inherited_mode_valid()) {
        output_w = output_h = total_lines = 0;
        uart_puts("[video] no progressive HDMI mode inherited; connect monitor before power\n");
        uart_puts("[video] CCU gate="); uart_puthex(CREG(0x064));
        uart_puts(" reset="); uart_puthex(CREG(0x2c4)); uart_puts("\n");
        return;
    }
    /* Preserve already-painted buffers; publish dirty pixels to DE2. */
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(OVL_ADDR, LCD_FB_BYTES);
    mixer0_init();
    ready = 1;
    uart_puts("[video] PINE64 HDMI "); uart_putdec(output_w);
    uart_puts("x"); uart_putdec(output_h);
    uart_puts("; canvas 480x272 at "); uart_putdec(canvas_x);
    uart_puts(","); uart_putdec(canvas_y);
    uart_puts("; VI0 + UI0 + UI1 PIP\n");
}

void video_commit(void)
{
    if (ready) { display_barrier(); MREG(0x08) = 1; }
}

void video_swap(uint32_t address)
{
    service_foreground();
    if (ready && framebuffer_valid(address, LCD_FB_BYTES)) {
        MREG(0x2018) = address;
        video_commit();
    }
}

void video_set_overlay(uint32_t address)
{
    if (ready && framebuffer_valid(address, LCD_FB_BYTES)) {
        UIREG(0, 0x10) = address;
        video_commit();
    }
}

void video_vi1_init(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!ready) return;
    if (!w || !h || x >= LCD_W || y >= LCD_H ||
        w > LCD_W - x || h > LCD_H - y) {
        uart_puts("[video] PIP rectangle is outside the canvas\n");
        return;
    }
    UIREG(1, 0x00) = 0xff000401u; /* opaque XRGB8888 */
    UIREG(1, 0x04) = SIZE(w, h);
    UIREG(1, 0x08) = 0;
    UIREG(1, 0x0c) = w * 4u;
    UIREG(1, 0x10) = SPR_ADDR;
    UIREG(1, 0x88) = SIZE(w, h);
    MREG(0x1024) = 0xff000000u;
    MREG(0x1028) = SIZE(w, h);
    MREG(0x102c) = XY(canvas_x + x, canvas_y + y);
    MREG(0x1098) = 0x03010301u;
    MREG(0x1080) = 0x210; /* third pipe is A64 UI1/channel2 */
    MREG(0x1000) = 0x701;
    video_commit();
}

void video_wait_vblank(void)
{
    service_foreground();
    if (ready && !wait_for_vblank()) vblank_timeout();
}

int pine64_video_measure_timing(void)
{
    frame_ticks = 0;
    if (!ready || !total_lines || !wait_for_vblank()) return 0;
    uint32_t start = timer_read();
    /* Average four real complete frames, independent of CPU clock/EDID rate. */
    for (unsigned i = 0; i < 4; ++i)
        if (!wait_for_vblank()) return 0;
    uint32_t measured = timer_elapsed(start, timer_read()) / 4u;
    if (measured < 80000u || measured > 2400000u) return 0;
    frame_ticks = measured;
    return 1;
}

int pine64_video_scale_rgb(uint32_t address, uint32_t w, uint32_t h)
{
    if (!ready || !w || !h || w > LCD_W || h > LCD_H ||
        w < LCD_W / 8u || h < LCD_H / 8u ||
        !framebuffer_valid(address, w * h * 4u)) return 0;
    VREG(0x00) = 0;
    CSC_CTRL(CSC0_BASE) = 0;
    VI_ATTR(0) = VI_ATTR_EN | VI_FMT_XRGB8888;
    MREG(0x2004) = SIZE(w, h);
    MREG(0x200c) = w * 4u;
    MREG(0x2018) = address;
    MREG(0x20e8) = SIZE(w, h);
    VREG(0x40) = SIZE(LCD_W, LCD_H);
    VREG(0x80) = VREG(0xc0) = SIZE(w, h);
    /* DE2 VSU has 20 fractional bits, not the original probe's 12. */
    VREG(0x88) = VREG(0xc8) = (w << 20) / LCD_W;
    VREG(0x8c) = VREG(0xcc) = (h << 20) / LCD_H;
    VREG(0x90) = VREG(0x98) = VREG(0xd0) = VREG(0xd8) = 0;
    /* Nearest-left sampling: unity weight in the central sample for ALL
     * 32 phases. H has eight taps split over two banks; V has four. */
    for (uint32_t i = 0; i < 32; ++i) {
        VREG(0x200 + i * 4) = VREG(0x600 + i * 4) = 0x40000000u;
        VREG(0x300 + i * 4) = VREG(0x700 + i * 4) = 0;
        VREG(0x400 + i * 4) = VREG(0x800 + i * 4) = 0x00004000u;
    }
    dcache_clean_range(address, w * h * 4u);
    display_barrier();
    VREG(0x00) = 0x11; /* enable and coefficients-ready */
    video_commit();
    return (VREG(0x00) & 0x11u) == 0x11u &&
           VREG(0x40) == SIZE(LCD_W, LCD_H);
}

/* Latest SDK Jupiter Modes, mapped onto A64's VI0 + UI0 + UI1 mixer.
 * U-Boot's native HDMI timings and the centered 480x272 canvas stay in
 * force. Mode 5 uses the real UI1 RGB layer for the second viewport. */
void video_mode(int mode)
{
    if (!ready) return;
    if (mode != 7) video_mode7_lineshift_off();
    UI_ATTR(0) = UI_EN | UI_FMT_ARGB8888 |
                 UI_AMODE_COMBINED | UI_GALPHA(255);
    BLD_CK_CTL = BLD_CK_CFG = 0;
    if (mode == 0) {
        BLD_ROUTE = ROUTE_P(0, 0);
        BLD_PIPE_CTL = PIPE_EN(0) | PIPE_FC(0);
    } else if (mode == 2) {
        BLD_ROUTE = ROUTE_P(0, 0) | ROUTE_P(1, 1) | ROUTE_P(2, 2);
        BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_EN(2) | PIPE_FC(0);
    } else if (mode == 5) {
        BLD_ROUTE = ROUTE_P(0, 0) | ROUTE_P(1, 2) | ROUTE_P(2, 1);
        BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_EN(2) | PIPE_FC(0);
    } else {
        BLD_ROUTE = ROUTE_P(0, 0) | ROUTE_P(1, 1);
        BLD_PIPE_CTL = PIPE_EN(0) | PIPE_EN(1) | PIPE_FC(0);
    }
    video_commit();
}

void video_mode3_alpha(uint8_t alpha)
{
    if (!ready) return;
    UI_ATTR(0) = UI_EN | UI_FMT_ARGB8888 |
                 UI_AMODE_COMBINED | UI_GALPHA(alpha);
    video_commit();
}

void video_mode4_nv12(uint32_t luma, uint32_t chroma,
                      uint32_t w, uint32_t h, uint32_t stride,
                      uint32_t x, uint32_t y)
{
    if (!ready || !w || !h || (w & 1u) || (h & 1u) || (stride & 1u) ||
        x >= LCD_W || y >= LCD_H || w > LCD_W - x || h > LCD_H - y ||
        stride < w || stride > 8192u ||
        !framebuffer_valid(luma, stride * h) ||
        !framebuffer_valid(chroma, stride * (h / 2u))) return;
    static const uint32_t bt601[12] = {
        0x000004a8, 0x00000000, 0x00000662, 0xfffc8451,
        0x000004a8, 0xfffffe6f, 0xfffffcc0, 0x00021e4d,
        0x000004a8, 0x00000811, 0x00000000, 0xfffbaca9
    };
    video_mode7_lineshift_off();
    VREG(0x00) = 0;
    VI_ATTR(0) = VI_ATTR_EN | VI_FMT_NV12;
    VI_MBSIZE(0) = VI_OVL_SIZE(0) = SIZE(w, h);
    VI_COOR(0) = 0;
    VI_PITCH0(0) = VI_PITCH1(0) = VI_PITCH2(0) = stride;
    VI_TOP_LADDR0(0) = luma;
    VI_TOP_LADDR1(0) = chroma;
    VI_TOP_LADDR2(0) = 0;
    MREG(0x20f0) = MREG(0x20f4) = MREG(0x20f8) = MREG(0x20fc) = 0;
    BLD_INSIZE(0) = SIZE(w, h);
    BLD_OFFSET(0) = XY(canvas_x + x, canvas_y + y);
    /* A64's VSU upsamples the half-sized NV12 chroma plane even for
     * 1:1 luma. 20-bit steps and -1/4 chroma V phase follow the
     * primary sun8i_vi_scaler setup. Unity nearest-sample coefficients
     * keep this small bring-up path independent of filter tables. */
    VREG(0x40) = VREG(0x80) = SIZE(w, h);
    VREG(0xc0) = SIZE(w / 2u, h / 2u);
    VREG(0x88) = VREG(0x8c) = 1u << 20;
    VREG(0xc8) = VREG(0xcc) = 1u << 19;
    VREG(0x90) = VREG(0x98) = VREG(0xd0) = 0;
    VREG(0xd8) = 0xfffc0000u;
    for (uint32_t i = 0; i < 32; ++i) {
        VREG(0x200 + i * 4) = VREG(0x600 + i * 4) = 0x40000000u;
        VREG(0x300 + i * 4) = VREG(0x700 + i * 4) = 0;
        VREG(0x400 + i * 4) = 0x00004000u;
        VREG(0x800 + i * 4) = i < 16 ? 0x00004000u : 0x00400000u;
    }
    for (uint32_t i = 0; i < 12; ++i) CSC_COEFF(CSC0_BASE, i) = bt601[i];
    CSC_CTRL(CSC0_BASE) = CSC_EN;
    display_barrier();
    VREG(0x00) = 0x11;
    video_commit();
}

void video_mode4_off(void)
{
    if (!ready) return;
    VREG(0x00) = 0;
    CSC_CTRL(CSC0_BASE) = 0;
    VI_ATTR(0) = VI_ATTR_EN | VI_FMT_XRGB8888;
    VI_MBSIZE(0) = VI_OVL_SIZE(0) = SIZE(LCD_W, LCD_H);
    VI_COOR(0) = 0;
    VI_PITCH0(0) = LCD_PITCH;
    VI_TOP_LADDR0(0) = FB0_ADDR;
    VI_TOP_LADDR1(0) = VI_TOP_LADDR2(0) = 0;
    BLD_INSIZE(0) = SIZE(LCD_W, LCD_H);
    BLD_OFFSET(0) = XY(canvas_x, canvas_y);
    video_commit();
}

void video_mode5_split(uint32_t top, uint32_t bottom)
{
    if (!ready || !framebuffer_valid(top, LCD_FB_BYTES / 2u) ||
        !framebuffer_valid(bottom, LCD_FB_BYTES / 2u)) return;
    video_mode7_lineshift_off();
    VREG(0x00) = 0;
    CSC_CTRL(CSC0_BASE) = 0;
    VI_ATTR(0) = VI_ATTR_EN | VI_FMT_XRGB8888;
    VI_MBSIZE(0) = VI_OVL_SIZE(0) = SIZE(LCD_W, LCD_H / 2u);
    VI_COOR(0) = 0;
    VI_PITCH0(0) = LCD_PITCH;
    VI_TOP_LADDR0(0) = top;
    PINE64_UI1_ATTR(0) = UI_EN | UI_FMT_XRGB8888 | UI_GALPHA(255);
    PINE64_UI1_SIZE(0) = PINE64_UI1_OVL_SIZE = SIZE(LCD_W, LCD_H / 2u);
    PINE64_UI1_COORD(0) = 0;
    PINE64_UI1_PITCH(0) = LCD_PITCH;
    PINE64_UI1_ADDR(0) = bottom;
    for (uint32_t pipe = 0; pipe < 3; ++pipe) {
        BLD_FCOLOR(pipe) = pipe < 2 ? 0xff000000u : 0;
        BLD_INSIZE(pipe) = SIZE(LCD_W, pipe < 2 ? LCD_H / 2u : LCD_H);
        BLD_OFFSET(pipe) = XY(canvas_x, canvas_y + (pipe == 1 ? LCD_H / 2u : 0));
        BLD_MODE(pipe) = BLEND_DEF;
    }
    video_mode(5);
}

void video_mode5_off(void)
{
    if (!ready) return;
    PINE64_UI1_ATTR(0) = 0;
    video_mode4_off();
    BLD_INSIZE(1) = SIZE(LCD_W, LCD_H);
    BLD_OFFSET(1) = XY(canvas_x, canvas_y);
    video_mode(1);
}

static void mode7_isr(void)
{
    if (!m7_offsets) return;
    uint32_t band = m7_band++;
    if (band >= m7_count) return;
    int64_t address = (int64_t)m7_base + (int32_t)m7_offsets[band] * 4;
    if (address < 0 || address > UINT32_MAX ||
        !framebuffer_valid((uint32_t)address, LCD_FB_BYTES)) return;
    VI_TOP_LADDR0(0) = (uint32_t)address;
    display_barrier();
    MIX_GLB_DBUF = 1;
}

static void mode7_canvas_start_isr(void)
{
    if (!m7_offsets) return;
    /* Band zero belongs to the first canvas line, not eight native
     * lines after vblank. The A64 HSTimer ISR acknowledges before it
     * calls us and has no stop epilogue, so replacing its one-shot
     * with the repeating interval here is safe. */
    mode7_isr();
    hstimer_set_repeating(0, m7_band_lines, mode7_isr);
}

static int mode7_arm_canvas_start(void)
{
    /* Calibrate before sampling the live line: measurement waits several
     * frames and would otherwise make a previously computed delay stale. */
    if (!total_lines || (!frame_ticks && !pine64_video_measure_timing()))
        return 0;

    /* A64 BSP lowlevel_sun50iw1/de_lcd_type.h: TCON_DEBUG(+0xfc)
     * bits11:0 are tcon1_current_line; bits27:16 are TCON0, even in
     * the separate TCON1 block. tcon_get_cur_line() uses this field.
     * BASIC4 VBP includes VSYNC + back porch, encoded minus one;
     * VT/2 is the progressive frame's line count (A64 manual 6.2.7.9,
     * U-Boot lcdc_tcon1_mode_set and the vendor tcon1_cfg agree).
     * Line indices include blanking. Add the centered canvas border
     * before taking the modular distance from the actual scan position,
     * rather than assuming the vblank flag means line zero. */
    uint32_t active_start = (TREG(0xa4) & 0xfffu) + 1u;
    uint32_t current_line = TREG(0xfc) & 0xfffu;
    if (active_start >= total_lines || output_h > total_lines - active_start ||
        current_line >= total_lines) return 0;
    uint32_t first_line = active_start + canvas_y;
    uint32_t delay_lines = (first_line + total_lines - current_line) % total_lines;
    if (delay_lines)
        hstimer_set_scanline(0, delay_lines, mode7_canvas_start_isr);
    else
        mode7_canvas_start_isr();
    return 1;
}

void video_mode7_lineshift(const int16_t *offsets, uint32_t count,
                           uint32_t lines_per_band)
{
    video_mode7_lineshift_off();
    if (!ready || !offsets || !count || count > LCD_H ||
        !lines_per_band || lines_per_band > LCD_H) return;
    m7_count = count;
    m7_band_lines = lines_per_band;
    m7_band = 0;
    m7_base = VI_TOP_LADDR0(0);
    display_barrier();
    m7_offsets = offsets;
    /* A64 has only HSTimer0. Both the pending first-band one-shot and
     * the later repeating bands use that same, cancellable timer. */
    if (!mode7_arm_canvas_start()) {
        video_mode7_lineshift_off();
        uart_puts("[video] Mode 7 scanline phase unavailable; line shift disabled\n");
    }
}

void video_mode7_line_reset(uint32_t base)
{
    if (!ready || !m7_offsets || !framebuffer_valid(base, LCD_FB_BYTES)) return;
    hstimer_stop(0);
    m7_base = base;
    m7_band = 0;
    VI_TOP_LADDR0(0) = base;
    video_commit();
    if (!mode7_arm_canvas_start()) {
        video_mode7_lineshift_off();
        uart_puts("[video] Mode 7 scanline phase unavailable; line shift disabled\n");
    }
}

void video_mode7_lineshift_off(void)
{
    if (!m7_offsets) return;
    hstimer_stop(0);
    m7_offsets = 0;
    if (ready && framebuffer_valid(m7_base, LCD_FB_BYTES)) {
        VI_TOP_LADDR0(0) = m7_base;
        video_commit();
    }
}

int pine64_video_is_ready(void) { return ready; }
uint32_t pine64_video_output_width(void) { return output_w; }
uint32_t pine64_video_output_height(void) { return output_h; }
uint32_t pine64_video_total_lines(void) { return total_lines; }
uint32_t pine64_video_frame_ticks(void) { return frame_ticks; }
uint32_t pine64_video_line_ticks(void)
{
    return total_lines ? frame_ticks / total_lines : 0;
}
