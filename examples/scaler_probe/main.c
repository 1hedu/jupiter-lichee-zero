/*
 * Jupiter SDK — VSU Scaler Probe
 *
 * The V3s shares DE2 IP with the H3, but Linux sets scaler_mask=0.
 * Is the scaler silicon actually there? Let's find out.
 *
 * Plan:
 *   1. Poke VSU_CTRL at mixer+0x20000 — write, read back
 *   2. Poke VSU_OUTSIZE, YHSTEP, YVSTEP — same test
 *   3. If readable: try a real 240x136 → 480x272 upscale
 *   4. Report everything on UART
 *
 * Register map (from kernel sun8i_vi_scaler.h):
 *   VSU_CTRL    = base + 0x00   (bit 0 = enable, bit 4 = coeff_rdy)
 *   VSU_OUTSIZE = base + 0x40
 *   VSU_YINSIZE = base + 0x80
 *   VSU_YHSTEP  = base + 0x88   (fixed-point 20.12: 0x1000 = 1:1)
 *   VSU_YVSTEP  = base + 0x8C
 *   VSU_YHPHASE = base + 0x90
 *   VSU_YVPHASE = base + 0x94
 *   VSU_CINSIZE = base + 0xC0
 *   VSU_CHSTEP  = base + 0xC8
 *   VSU_CVSTEP  = base + 0xCC
 *
 * DE2 scaler base for channel 0 (VI0):
 *   mixer + 0x20000 (confirmed from kernel DE2_VI_SCALER_UNIT_BASE)
 */
#include "jupiter.h"

/* VSU register base for VI0 scaler */
#define VSU_BASE        (MIXER0_BASE + 0x20000)
#define VSU_CTRL        REG32(VSU_BASE + 0x00)
#define VSU_OUTSIZE     REG32(VSU_BASE + 0x40)
#define VSU_YINSIZE     REG32(VSU_BASE + 0x80)
#define VSU_YHSTEP      REG32(VSU_BASE + 0x88)
#define VSU_YVSTEP      REG32(VSU_BASE + 0x8C)
#define VSU_YHPHASE     REG32(VSU_BASE + 0x90)
#define VSU_YVPHASE     REG32(VSU_BASE + 0x94)
#define VSU_CINSIZE     REG32(VSU_BASE + 0xC0)
#define VSU_CHSTEP      REG32(VSU_BASE + 0xC8)
#define VSU_CVSTEP      REG32(VSU_BASE + 0xCC)

/* Scaler fixed-point: 20.12 format. 0x1000 = 1x, 0x800 = 2x upscale */
#define VSU_STEP(src, dst) (((src) << 12) / (dst))

/* Filter coefficient RAM (32 taps × 4 phases × Y/C × H/V) */
#define VSU_YHCOEFF0    (VSU_BASE + 0x200)
#define VSU_YHCOEFF1    (VSU_BASE + 0x300)
#define VSU_YVCOEFF     (VSU_BASE + 0x400)
#define VSU_CHCOEFF0    (VSU_BASE + 0x600)
#define VSU_CHCOEFF1    (VSU_BASE + 0x700)
#define VSU_CVCOEFF     (VSU_BASE + 0x800)

static void probe_reg(const char *name, volatile uint32_t *reg,
                      uint32_t test_val)
{
    uint32_t old = *reg;
    *reg = test_val;
    uint32_t got = *reg;
    *reg = old;  /* restore */

    uart_puts("  ");
    uart_puts(name);
    uart_puts(": wrote ");
    uart_puthex(test_val);
    uart_puts(" read ");
    uart_puthex(got);
    if (got == test_val)
        uart_puts(" *** MATCH ***");
    else if (got == 0)
        uart_puts(" (dead)");
    else
        uart_puts(" (partial?)");
    uart_puts("\n");
}

/* Render a test pattern at small resolution */
static void render_small(volatile uint32_t *fb, uint32_t w, uint32_t h)
{
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++) {
            /* Color bars: 8 vertical strips */
            uint32_t bar = (x * 8) / w;
            uint32_t colors[8] = {
                0x00FF0000, 0x00FF8000, 0x00FFFF00, 0x0000FF00,
                0x0000FFFF, 0x000000FF, 0x00FF00FF, 0x00FFFFFF
            };
            fb[y * w + x] = colors[bar & 7];
        }
}

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — VSU Scaler Probe ===\n");
    timer_init();

    memset32_neon(FB0_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    mmu_init();
    uart_puts("Display + MMU active.\n\n");

    /* ---- Phase 1: Register probe ---- */
    uart_puts("--- Phase 1: Register Probe (mixer+0x20000) ---\n");
    uart_puts("Reading VSU_CTRL cold: ");
    uart_puthex(VSU_CTRL);
    uart_puts("\n\n");

    probe_reg("VSU_CTRL   ", &VSU_CTRL,    0x00000001);
    probe_reg("VSU_OUTSIZE", &VSU_OUTSIZE, WH(480, 272));
    probe_reg("VSU_YINSIZE", &VSU_YINSIZE, WH(240, 136));
    probe_reg("VSU_YHSTEP ", &VSU_YHSTEP,  0x00000800);  /* 2x upscale */
    probe_reg("VSU_YVSTEP ", &VSU_YVSTEP,  0x00000800);
    probe_reg("VSU_YHPHASE", &VSU_YHPHASE, 0x00000000);
    probe_reg("VSU_YVPHASE", &VSU_YVPHASE, 0x00000000);

    /* Also try a coefficient register */
    volatile uint32_t *coeff = (volatile uint32_t *)VSU_YHCOEFF0;
    uint32_t coeff_old = coeff[0];
    coeff[0] = 0x40000000;
    uint32_t coeff_got = coeff[0];
    coeff[0] = coeff_old;
    uart_puts("  YHCOEFF[0]: wrote ");
    uart_puthex(0x40000000);
    uart_puts(" read ");
    uart_puthex(coeff_got);
    if (coeff_got == 0x40000000)
        uart_puts(" *** MATCH ***");
    else
        uart_puts(" (dead)");
    uart_puts("\n");

    uart_puts("\n");

    /* ---- Phase 2: If registers are live, attempt a real upscale ---- */
    /* Check if OUTSIZE stuck — that's our canary */
    VSU_OUTSIZE = WH(480, 272);
    uint32_t check = VSU_OUTSIZE;
    VSU_OUTSIZE = 0;

    if (check == WH(480, 272)) {
        uart_puts("--- Phase 2: Attempting 240x136 -> 480x272 upscale ---\n");

        /* Render small test pattern into FB1 */
        uint32_t small_w = 240, small_h = 136;
        volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
        render_small(fb1, small_w, small_h);
        dcache_clean_range(FB1_ADDR, small_w * small_h * 4);

        /* Reconfigure VI0 for small input */
        VI_MBSIZE(0)     = WH(small_w, small_h);
        VI_PITCH0(0)     = small_w * 4;
        VI_TOP_LADDR0(0) = FB1_ADDR;
        VI_OVL_SIZE(0)   = WH(small_w, small_h);

        /* Set up VSU: 240x136 in → 480x272 out (2x integer) */
        VSU_YINSIZE  = WH(small_w, small_h);
        VSU_OUTSIZE  = WH(LCD_W, LCD_H);
        VSU_YHSTEP   = VSU_STEP(small_w, LCD_W);    /* 0x800 = 2x */
        VSU_YVSTEP   = VSU_STEP(small_h, LCD_H);
        VSU_YHPHASE  = 0;
        VSU_YVPHASE  = 0;

        /* Chroma (same for XRGB, just mirror Y settings) */
        VSU_CINSIZE  = WH(small_w, small_h);
        VSU_CHSTEP   = VSU_STEP(small_w, LCD_W);
        VSU_CVSTEP   = VSU_STEP(small_h, LCD_H);

        /* Load nearest-neighbor coefficients (passthrough: all weight on tap 0) */
        volatile uint32_t *yh0 = (volatile uint32_t *)VSU_YHCOEFF0;
        volatile uint32_t *yv  = (volatile uint32_t *)VSU_YVCOEFF;
        volatile uint32_t *ch0 = (volatile uint32_t *)VSU_CHCOEFF0;
        volatile uint32_t *cv  = (volatile uint32_t *)VSU_CVCOEFF;
        for (int i = 0; i < 32; i++) {
            yh0[i] = (i == 0) ? 0x40000000 : 0;
            yv[i]  = (i == 0) ? 0x40000000 : 0;
            ch0[i] = (i == 0) ? 0x40000000 : 0;
            cv[i]  = (i == 0) ? 0x40000000 : 0;
        }

        /* Enable scaler */
        VSU_CTRL = (1 << 0) | (1 << 4);  /* EN + COEFF_RDY */

        /* Blender still expects 480x272 output — no change needed */
        MIX_GLB_DBUF = DBUF_EN;

        uart_puts("  VSU enabled. Check panel.\n");
        uart_puts("  YHSTEP=");
        uart_puthex(VSU_STEP(small_w, LCD_W));
        uart_puts(" YVSTEP=");
        uart_puthex(VSU_STEP(small_h, LCD_H));
        uart_puts("\n");

        /* Read back to confirm */
        uart_puts("  VSU_CTRL readback: ");
        uart_puthex(VSU_CTRL);
        uart_puts("\n");

        uart_puts("\n  If you see SCALED color bars, the V3s has a scaler.\n");
        uart_puts("  If you see a black screen or 240x136 in the corner,\n");
        uart_puts("  the registers are mappable but the hardware is absent.\n");
    } else {
        uart_puts("--- Phase 2: SKIPPED (registers appear dead) ---\n");
        uart_puts("  No hardware scaler detected.\n");
        uart_puts("  Software NEON upscaler is the path forward.\n");

        /* Show something so we know the board didn't crash */
        volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
        render_small(fb0, LCD_W, LCD_H);
        dcache_clean_fb(FB0_ADDR);
        video_swap(FB0_ADDR);
    }

    uart_puts("\n--- Done. Halting. ---\n");

    while (1)
        __asm__ volatile("wfi");
}
