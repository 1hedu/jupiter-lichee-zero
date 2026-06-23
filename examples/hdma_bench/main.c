/*
 * HDMA Work Distribution Benchmark
 *
 * Compares two approaches:
 *   A) Burst: all rendering + audio + logic in one chunk after vblank
 *   B) HDMA: spread across frame using HSTimer interrupts
 *
 * Measures worst-case frame time, average frame time, and whether
 * frames are completed before the next vblank.
 *
 * Build: make GAME=examples/hdma_bench/main.c
 */
#include "jupiter.h"
#include "nes.h"
#include "hstimer.h"
#include "pmu.h"
#include <string.h>

/* Simulate a heavy workload: render a complex NES scene */
static uint8_t bg_chr[256 * 16];
static uint8_t palette_ram[32];
static uint8_t nametable[NES_NT_TILES];
static uint8_t attribute[NES_NT_ATTRS];
static nes_oam_entry_t oam[NES_MAX_SPRITES];

/* Dummy "game logic" — burn cycles to simulate real work */
static volatile uint32_t logic_result;
static void simulate_game_logic(void)
{
    volatile uint32_t acc = 0;
    for (volatile int i = 0; i < 50000; i++)
        acc += i * 7 + (acc >> 3);
    logic_result = acc;
}

/* Dummy "audio mix" — burn cycles */
static void simulate_audio_mix(void)
{
    volatile uint32_t acc = 0;
    for (volatile int i = 0; i < 30000; i++)
        acc += i * 13 + (acc >> 5);
    logic_result = acc;
}

/* Build a busy NES scene with 32 sprites */
static void setup_scene(void)
{
    memset(bg_chr, 0, sizeof(bg_chr));
    /* Tile 1: checkerboard */
    for (int r = 0; r < 8; r++) {
        bg_chr[1*16+r]   = (r & 1) ? 0xAA : 0x55;
        bg_chr[1*16+r+8] = (r & 1) ? 0x55 : 0xAA;
    }
    /* Tile 2: solid */
    for (int r = 0; r < 8; r++) {
        bg_chr[2*16+r] = 0xFF; bg_chr[2*16+r+8] = 0x00;
    }
    /* Tile 3: diagonal */
    for (int r = 0; r < 8; r++) {
        bg_chr[3*16+r] = (0xFF << r) | (0xFF >> (8-r));
        bg_chr[3*16+r+8] = ~bg_chr[3*16+r];
    }

    memset(palette_ram, 0x0D, sizeof(palette_ram));
    palette_ram[0] = 0x02; palette_ram[1] = 0x16;
    palette_ram[2] = 0x26; palette_ram[3] = 0x30;
    palette_ram[5] = 0x17; palette_ram[6] = 0x27; palette_ram[7] = 0x37;
    palette_ram[17] = 0x15; palette_ram[18] = 0x25; palette_ram[19] = 0x30;

    /* Busy nametable: alternating tiles */
    for (int y = 0; y < NES_NT_H; y++)
        for (int x = 0; x < NES_NT_W; x++)
            nametable[y * NES_NT_W + x] = ((x + y) % 3) + 1;

    memset(attribute, 0, sizeof(attribute));
    for (int i = 0; i < NES_NT_ATTRS; i++)
        attribute[i] = NES_ATTR(i & 3, (i >> 2) & 3, (i >> 4) & 3, (i >> 6) & 3);

    /* 32 sprites scattered around */
    for (int i = 0; i < 32; i++) {
        oam[i] = (nes_oam_entry_t){
            .y = (uint8_t)(20 + (i * 6) % 180),
            .x = (uint8_t)(10 + (i * 8) % 230),
            .tile = (uint8_t)((i % 3) + 1),
            .attr = NES_SPR_PAL(i & 3),
        };
    }
}

/* ================================================================== */
/* HDMA state machine                                                   */
/* ================================================================== */
static volatile int hdma_phase = 0;
static volatile uint32_t *hdma_fb;
static volatile uint32_t *hdma_ovl;
static nes_bg_t *hdma_bg;
static uint32_t hdma_render_top_cycles;
static uint32_t hdma_render_bot_cycles;
static uint32_t hdma_logic_cycles;
static uint32_t hdma_audio_cycles;

/* Phase 1 ISR: render bottom half + audio mix */
static void hdma_phase1_isr(void)
{
    uint32_t t0 = pmu_cycles();

    /* Render bottom half: scanlines 112-223 */
    /* We can't easily split the NES renderer mid-frame, so instead
     * we do the audio mix and game logic here */
    simulate_audio_mix();
    hdma_audio_cycles = pmu_cycles() - t0;

    t0 = pmu_cycles();
    simulate_game_logic();
    hdma_logic_cycles = pmu_cycles() - t0;

    hdma_phase = 2;  /* signal main loop that work is done */
}

/* ================================================================== */
/* Benchmark runner                                                     */
/* ================================================================== */

#define BENCH_FRAMES 120  /* 2 seconds at 60fps */

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    irq_init();
    hstimer_init();
    irq_global_enable();

    uart_puts("\n========================================\n");
    uart_puts("  HDMA Work Distribution Benchmark\n");
    uart_puts("========================================\n\n");

    setup_scene();
    video_init();

    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++) {
        fb0[i] = 0xFF000000; fb1[i] = 0xFF000000;
    }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    nes_bg_t bg = {
        .chr = bg_chr, .nametable = nametable, .attribute = attribute,
        .palette_ram = palette_ram, .scroll_x = 0, .scroll_y = 0,
        .line_scroll_x = NULL, .enabled = 1,
    };
    hdma_bg = &bg;

    /* ============================================================ */
    /* BENCHMARK A: Burst mode (all work after vblank)               */
    /* ============================================================ */
    uart_puts("--- MODE A: Burst (all work after vblank) ---\n");

    uint32_t burst_max = 0, burst_total = 0;
    uint32_t burst_render_total = 0, burst_logic_total = 0, burst_audio_total = 0;
    int buf = 0;

    for (int f = 0; f < BENCH_FRAMES; f++) {
        video_wait_vblank();
        uint32_t frame_start = pmu_cycles();

        volatile uint32_t *fb = buf ? fb1 : fb0;
        volatile uint32_t *ovl = (volatile uint32_t *)(buf ? OVL1_ADDR : OVL_ADDR);
        uint32_t fb_addr = buf ? FB1_ADDR : FB0_ADDR;
        uint32_t ovl_addr = buf ? OVL1_ADDR : OVL_ADDR;

        bg.scroll_x = f;

        /* ALL work in one burst */
        uint32_t t0 = pmu_cycles();
        memset((void *)ovl, 0, LCD_FB_BYTES);
        nes_render((uint32_t *)fb, (uint32_t *)ovl, LCD_W, LCD_H,
                   &bg, bg_chr, oam, 32, 1);
        uint32_t render_cyc = pmu_cycles() - t0;

        t0 = pmu_cycles();
        simulate_audio_mix();
        uint32_t audio_cyc = pmu_cycles() - t0;

        t0 = pmu_cycles();
        simulate_game_logic();
        uint32_t logic_cyc = pmu_cycles() - t0;

        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);

        uint32_t frame_cyc = pmu_cycles() - frame_start;
        burst_total += frame_cyc;
        burst_render_total += render_cyc;
        burst_logic_total += logic_cyc;
        burst_audio_total += audio_cyc;
        if (frame_cyc > burst_max) burst_max = frame_cyc;

        buf = !buf;
    }

    uart_puts("  avg frame: "); uart_putdec(burst_total / BENCH_FRAMES / 1200); uart_puts("us\n");
    uart_puts("  max frame: "); uart_putdec(burst_max / 1200); uart_puts("us\n");
    uart_puts("  avg render: "); uart_putdec(burst_render_total / BENCH_FRAMES / 1200); uart_puts("us\n");
    uart_puts("  avg audio:  "); uart_putdec(burst_audio_total / BENCH_FRAMES / 1200); uart_puts("us\n");
    uart_puts("  avg logic:  "); uart_putdec(burst_logic_total / BENCH_FRAMES / 1200); uart_puts("us\n");
    uart_puts("  budget:     16667us (60fps)\n");
    uart_puts("  headroom:   "); uart_putdec(16667 - burst_total / BENCH_FRAMES / 1200); uart_puts("us\n\n");

    /* ============================================================ */
    /* BENCHMARK B: HDMA mode (work spread across frame)             */
    /* ============================================================ */
    uart_puts("--- MODE B: HDMA (work spread via HSTimer) ---\n");

    uint32_t hdma_max = 0, hdma_total = 0;
    uint32_t hdma_burst_max = 0;  /* worst-case burst within vblank */

    for (int f = 0; f < BENCH_FRAMES; f++) {
        video_wait_vblank();
        uint32_t frame_start = pmu_cycles();

        volatile uint32_t *fb = buf ? fb1 : fb0;
        volatile uint32_t *ovl = (volatile uint32_t *)(buf ? OVL1_ADDR : OVL_ADDR);
        uint32_t fb_addr = buf ? FB1_ADDR : FB0_ADDR;
        uint32_t ovl_addr = buf ? OVL1_ADDR : OVL_ADDR;

        bg.scroll_x = f;
        hdma_fb = fb;
        hdma_ovl = ovl;

        /* Schedule phase 1: audio + logic at mid-frame (~140 scanlines in) */
        hdma_phase = 0;
        hstimer_set_scanline(0, 140, hdma_phase1_isr);

        /* Vblank burst: only rendering (audio + logic deferred to ISR) */
        uint32_t t0 = pmu_cycles();
        memset((void *)ovl, 0, LCD_FB_BYTES);
        nes_render((uint32_t *)fb, (uint32_t *)ovl, LCD_W, LCD_H,
                   &bg, bg_chr, oam, 32, 1);
        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        uint32_t burst_cyc = pmu_cycles() - t0;

        video_swap(fb_addr);
        video_set_overlay(ovl_addr);

        /* Wait for HDMA phase to complete (ISR does audio + logic) */
        {
            uint32_t wait_start = pmu_cycles();
            while (hdma_phase < 2) {
                if (pmu_cycles() - wait_start > 24000000) { /* 20ms timeout */
                    if (f == 0) {
                        /* First frame: print diagnostics */
                        uart_puts("\n[hdma] ISR TIMEOUT! Diagnostics:\n");
                        uart_puts("  IRQ_EN=0x"); uart_puthex(REG32(0x01C60000));
                        uart_puts("  IRQ_STAS=0x"); uart_puthex(REG32(0x01C60004));
                        uart_puts("\n  TMR0_CTRL=0x"); uart_puthex(REG32(0x01C60010));
                        uart_puts("  TMR0_CURNT=0x"); uart_puthex(REG32(0x01C6001C));
                        uart_puts("\n  BUS_GATE0=0x"); uart_puthex(REG32(CCU_BASE + 0x060));
                        uart_puts("  BUS_RST0=0x"); uart_puthex(REG32(CCU_BASE + 0x2C0));
                        uart_puts("\n  phase="); uart_putdec(hdma_phase);
                        uart_puts("\n");

                        /* Try polling instead of IRQ */
                        uart_puts("  Trying poll: ");
                        uint32_t stas = REG32(0x01C60004);
                        if (stas & 1) {
                            uart_puts("PENDING! IRQ routing broken\n");
                            REG32(0x01C60004) = 1; /* clear */
                        } else {
                            uart_puts("no pending — timer not firing\n");
                        }
                    }
                    /* Do the work ourselves since ISR didn't fire */
                    simulate_audio_mix();
                    simulate_game_logic();
                    hdma_phase = 2;
                    break;
                }
            }
        }

        uint32_t frame_cyc = pmu_cycles() - frame_start;
        hdma_total += frame_cyc;
        if (frame_cyc > hdma_max) hdma_max = frame_cyc;
        if (burst_cyc > hdma_burst_max) hdma_burst_max = burst_cyc;

        buf = !buf;
    }

    hstimer_stop(0);

    uart_puts("  avg frame: "); uart_putdec(hdma_total / BENCH_FRAMES / 1200); uart_puts("us\n");
    uart_puts("  max frame: "); uart_putdec(hdma_max / 1200); uart_puts("us\n");
    uart_puts("  max vblank burst: "); uart_putdec(hdma_burst_max / 1200); uart_puts("us\n");
    uart_puts("  budget:     16667us (60fps)\n");
    uart_puts("  headroom:   "); uart_putdec(16667 - hdma_total / BENCH_FRAMES / 1200); uart_puts("us\n\n");

    /* ============================================================ */
    /* Summary                                                        */
    /* ============================================================ */
    uart_puts("--- SUMMARY ---\n");
    uart_puts("  Burst max vblank load: "); uart_putdec(burst_max / 1200); uart_puts("us\n");
    uart_puts("  HDMA  max vblank load: "); uart_putdec(hdma_burst_max / 1200); uart_puts("us\n");
    int improvement = (int)(burst_max / 1200) - (int)(hdma_burst_max / 1200);
    uart_puts("  Vblank pressure reduced by: "); uart_putdec(improvement); uart_puts("us\n");
    uart_puts("  (audio+logic moved to mid-frame ISR)\n\n");

    uart_puts("Done. Screen shows NES render with 32 sprites.\n");

    while (1);
    return 0;
}
