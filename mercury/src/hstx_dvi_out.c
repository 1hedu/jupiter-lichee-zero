/*
 * Mercury — HSTX DVI scanout (RP2350: Pico 2 / Pico 2 W)
 *
 * Runs on Core 1. The RP2350's HSTX peripheral TMDS-encodes in
 * hardware, so unlike the RP2040 PicoDVI route (dvi_out.c) the CPU
 * does no per-pixel work at all: two ping-pong DMA channels stream
 * command lists + framebuffer rows into the HSTX FIFO, and the DMA IRQ
 * just hands the finished channel its next segment from
 * hstx_dvi_stream.c (host-tested in mercury/test/).
 *
 * Output: 640x480p60 DVI with the w x h framebuffer centered. HSTX
 * exists only on GP12-19; the TMDS pair layout is a board preset in
 * jupiter32x.h (default: Pico DVI Sock — GP12/13 D0, GP14/15 CK,
 * GP16/17 D2, GP18/19 D1, 270 ohm series resistors per line).
 *
 * Timing: HSTX shifts 2 bits per clk_hstx cycle and clk_hstx = clk_sys
 * (SDK runtime_init_clocks), so the bit rate is 2 x clk_sys. The build
 * sets clk_sys = 126 MHz (CMakeLists: PLL 1512 MHz / 6 / 2) -> 252 Mbps
 * -> 25.2 MHz pixel clock -> exactly 60.0 Hz. (At the RP2350's 150 MHz
 * default it would be ~71 Hz — non-standard, which a TC358743 bridge's
 * format detection may reject.)
 *
 * Register setup transcribed from raspberrypi/pico-examples
 * hstx/dvi_out_hstx_encoder. Its expand_tmds rotation config maps the
 * pixel byte as R[7:5] G[4:2] B[1:0] onto DVI lanes 2/1/0 — exactly
 * Mercury's R3G3B2 framebuffer format.
 *
 * Scanout interface: j32x_scanout_start() + j32x_scanout_loop().
 */
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"      /* __wfi */
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "jupiter32x.h"
#include "hstx_dvi_stream.h"

static hstx_stream_t g_stream;
static uint g_dma_ping, g_dma_pong;
static volatile bool g_pong_next;   /* which channel finishes next */

/* Frame boundary (vblank, line 0 being scheduled): flip buffers and
 * latch the geometry for the coming frame. */
static void __not_in_flash_func(on_frame_start)(hstx_stream_t *s)
{
    j32x_swap_apply();
    s->fb = g_state.fb[g_state.front];
    s->w  = g_state.width  ? g_state.width  : J32X_WIDTH;
    s->h  = g_state.height ? g_state.height : J32X_HEIGHT;
    g_state.frame_count++;
}

static void __not_in_flash_func(dma_irq_handler)(void)
{
    /* The channel that just finished is reloaded (without triggering);
     * the other one is already streaming, and chains back to this one
     * when it completes. */
    uint ch_num = g_pong_next ? g_dma_pong : g_dma_ping;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    g_pong_next = !g_pong_next;

    const uint32_t *addr;
    uint32_t words;
    hstx_stream_next(&g_stream, &addr, &words);
    ch->read_addr = (uintptr_t)addr;
    ch->transfer_count = words;
}

static void configure_channel(uint ch, uint chain_to,
                              const uint32_t *addr, uint32_t words)
{
    dma_channel_config c = dma_channel_get_default_config(ch);
    channel_config_set_chain_to(&c, chain_to);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(ch, &c, &hstx_fifo_hw->fifo, addr, words, false);
}

void j32x_scanout_start(void)
{
    /* TMDS encoder: 8bpp R3G3B2 (see header comment) */
    hstx_ctrl_hw->expand_tmds =
        2  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
        0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
        2  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
        29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
        1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
        26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

    /* Pixels: 4 x 8-bit per FIFO word. Control symbols: 1 x 32-bit. */
    hstx_ctrl_hw->expand_shift =
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
        8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB    |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    /* 5 clk_hstx cycles per 10-bit symbol, 2 bits shifted per cycle */
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    /* Pin mapping (jupiter32x.h presets). HSTX output bit n drives
     * GP(12+n). Each TMDS pair is (base, base+1); the "-" pin gets
     * the INV flag — base+1 normally, base on inverted boards. With
     * the DVI Sock default this reproduces pico-examples exactly:
     * clock on bits 2/3, lanes 0/1/2 on bits 0/6/4. */
    _Static_assert(MERCURY_HSTX_CLK >= 12 && MERCURY_HSTX_CLK <= 18 &&
                   MERCURY_HSTX_D0  >= 12 && MERCURY_HSTX_D0  <= 18 &&
                   MERCURY_HSTX_D1  >= 12 && MERCURY_HSTX_D1  <= 18 &&
                   MERCURY_HSTX_D2  >= 12 && MERCURY_HSTX_D2  <= 18,
                   "HSTX pairs must sit entirely within GP12-19");
    const uint32_t inv_p = MERCURY_HSTX_INVERT ? HSTX_CTRL_BIT0_INV_BITS : 0;
    const uint32_t inv_n = MERCURY_HSTX_INVERT ? 0 : HSTX_CTRL_BIT0_INV_BITS;
    int ck = MERCURY_HSTX_CLK - 12;
    hstx_ctrl_hw->bit[ck    ] = HSTX_CTRL_BIT0_CLK_BITS | inv_p;
    hstx_ctrl_hw->bit[ck + 1] = HSTX_CTRL_BIT0_CLK_BITS | inv_n;
    static const int lane_pin[3] = {
        MERCURY_HSTX_D0, MERCURY_HSTX_D1, MERCURY_HSTX_D2
    };
    for (uint lane = 0; lane < 3; ++lane) {
        int bit = lane_pin[lane] - 12;
        uint32_t sel =
            (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit    ] = sel | inv_p;
        hstx_ctrl_hw->bit[bit + 1] = sel | inv_n;
    }
    for (uint pin = PIN_HSTX_FIRST; pin <= PIN_HSTX_LAST; ++pin)
        gpio_set_function(pin, 0);          /* GPIO_FUNC_HSTX on GP12-19 */

    /* Stream + ping-pong DMA: queue the first two segments, then the
     * IRQ keeps one segment ahead forever. */
    hstx_stream_init(&g_stream, on_frame_start, NULL);
    g_dma_ping = (uint)dma_claim_unused_channel(true);
    g_dma_pong = (uint)dma_claim_unused_channel(true);

    const uint32_t *addr;
    uint32_t words;
    hstx_stream_next(&g_stream, &addr, &words);
    configure_channel(g_dma_ping, g_dma_pong, addr, words);
    hstx_stream_next(&g_stream, &addr, &words);
    configure_channel(g_dma_pong, g_dma_ping, addr, words);
    g_pong_next = false;                    /* ping finishes first */

    /* IRQ on THIS core (core 1) — keeps it off the rasterizer's core */
    dma_hw->ints0 = (1u << g_dma_ping) | (1u << g_dma_pong);
    dma_hw->inte0 = (1u << g_dma_ping) | (1u << g_dma_pong);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    /* DMA wins the bus over the rasterizer's framebuffer writes */
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS |
                            BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    dma_channel_start(g_dma_ping);
}

void j32x_scanout_loop(void)
{
    /* Everything happens in the DMA IRQ (swap included, at vblank). */
    while (1)
        __wfi();
}
