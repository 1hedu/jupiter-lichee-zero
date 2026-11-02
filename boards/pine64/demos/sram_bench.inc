#include "support.h"
#include "pmu.h"
#include <stdio.h>

static uint32_t sram_ddr_words[16384] __attribute__((aligned(64)));
static volatile uint32_t sram_bench_sink;

int main(void)
{
    timer_init(); mmu_init(); pmu_init(); video_init();
    sram_init();
    uart_puts("\n=== PINE64 SRAM ownership / DDR reference ===\n");
    uart_puts("A64 SRAM_C: DE2 line buffers, including scaler.\n");
    uart_puts("A64 SRAM_A2: secure TF-A. SRAM_C1: Cedar VE.\n");
    uart_puts("No verified free SRAM allocation is provided by this boot ABI.\n");
    uart_puts("SRAM benchmark: RESERVED, no SRAM writes performed.\n");
    for (uint32_t i = 0; i < 16384; ++i) sram_ddr_words[i] = i;
    volatile uint32_t *words = sram_ddr_words;
    uint32_t sum = 0, start = timer_read();
    for (uint32_t r = 0; r < 64; ++r)
        for (uint32_t i = 0; i < 16384; ++i) sum += words[i];
    uint32_t read_ticks = timer_elapsed(start, timer_read());
    sram_bench_sink = sum;
    start = timer_read();
    for (uint32_t r = 0; r < 64; ++r)
        for (uint32_t i = 0; i < 16384; ++i) words[i] = i;
    __asm__ volatile("dsb sy" ::: "memory");
    uint32_t write_ticks = timer_elapsed(start, timer_read());
    uint32_t read_bw = read_ticks ? (uint32_t)(4194304ull * 24u / read_ticks) : 0;
    uint32_t write_bw = write_ticks ? (uint32_t)(4194304ull * 24u / write_ticks) : 0;
    uart_puts("DDR 64 KiB warm read: "); uart_putdec(read_bw); uart_puts(" MB/s\n");
    uart_puts("DDR 64 KiB stores: "); uart_putdec(write_bw); uart_puts(" MB/s\n");
    uint32_t *screen = (uint32_t *)FB0_ADDR;
    memset32_neon(FB0_ADDR, 0xff081428u, LCD_FB_BYTES);
    pine_demo_draw_text(screen, "A64 SRAM OWNERSHIP", 16, 16, 2, 0xff80e0ffu);
    pine_demo_draw_text(screen, "SRAM RESERVED", 16, 50, 2, 0xffffd080u);
    pine_demo_draw_text(screen, "C: DISPLAY  A2: FIRMWARE  C1: VIDEO ENGINE", 16, 80, 1, 0xffffffffu);
    pine_demo_draw_text(screen, "NO SRAM PROBE OR REMAP PERFORMED", 16, 100, 1, 0xffffffffu);
    char line[64];
    snprintf(line, sizeof(line), "DDR READ: %lu MB/S", (unsigned long)read_bw);
    pine_demo_draw_text(screen, line, 16, 144, 2, 0xff80ff80u);
    snprintf(line, sizeof(line), "DDR STORE: %lu MB/S", (unsigned long)write_bw);
    pine_demo_draw_text(screen, line, 16, 176, 2, 0xff80ff80u);
    pine_demo_draw_text(screen, "A/B: MENU", 16, 244, 1, 0xffc0c0c0u);
    pine_demo_present(FB0_ADDR);
    pine_demo_wait();
    return 0;
}
