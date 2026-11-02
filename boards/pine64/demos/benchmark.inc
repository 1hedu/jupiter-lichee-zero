/* A53 diagnostics: keep the established MMU/cache configuration intact.
 * Durations come from the 24 MHz oscillator, not an assumed CPU clock. */
#include "support.h"
#include "pmu.h"
#include <stdio.h>

static uint32_t a53_scratch[1024u * 1024u] __attribute__((aligned(64)));
static volatile uint32_t a53_sink;

static void a53_result(const char *label, uint32_t bytes, uint32_t ticks)
{
    uart_puts(label); uart_putdec(bytes / 1024u); uart_puts(" KiB: ");
    uart_putdec(ticks_to_us(ticks)); uart_puts(" us, ");
    if (ticks) uart_putdec((uint32_t)((uint64_t)bytes * 24u / ticks));
    else uart_puts("below timer resolution");
    uart_puts(" MB/s\n");
}

int main(void)
{
    timer_init(); mmu_init(); pmu_init(); video_init();
    uint32_t midr, sctlr, clidr;
    __asm__ volatile("mrc p15,0,%0,c0,c0,0" : "=r"(midr));
    __asm__ volatile("mrc p15,0,%0,c1,c0,0" : "=r"(sctlr));
    __asm__ volatile("mrc p15,1,%0,c0,c0,1" : "=r"(clidr));
    uart_puts("\n=== PINE64 Cortex-A53 diagnostic benchmark ===\nMIDR=");
    uart_puthex(midr); uart_puts(" SCTLR="); uart_puthex(sctlr);
    uart_puts(" CLIDR="); uart_puthex(clidr); uart_puts("\n");
    uart_puts("MMU/cache state retained. Timing: fixed 24 MHz oscillator.\n");
    uart_puts("Scratch: private 4 MiB BSS allocation; no fixed code-address writes.\n");
    uint32_t *screen = (uint32_t *)FB1_ADDR;
    memset32_neon(FB1_ADDR, 0xff081428u, LCD_FB_BYTES);
    pine_demo_draw_text(screen, "CORTEX-A53 BENCHMARK", 16, 16, 2, 0xff80e0ffu);
    pine_demo_draw_text(screen, "RUNNING - DETAILS ON SERIAL", 16, 42, 1, 0xffffffffu);
    pine_demo_present(FB1_ADDR);

    uart_puts("\nDependent pointer loads, 67-word stride (warm):\n");
    for (uint32_t words = 256; words <= 1024u * 1024u; words <<= 1) {
        for (uint32_t i = 0; i < words; ++i)
            a53_scratch[i] = (i + 67u) & (words - 1u);
        uint32_t idx = 0;
        for (uint32_t i = 0; i < words * 2u; ++i) idx = a53_scratch[idx];
        a53_sink = idx;
        uint32_t iterations = words < 65536u ? 200000u : 100000u;
        uint32_t t = timer_read();
        for (uint32_t i = 0; i < iterations; ++i) idx = a53_scratch[idx];
        uint32_t elapsed = timer_elapsed(t, timer_read());
        a53_sink = idx;
        uint32_t tenths_ns = (uint32_t)((uint64_t)elapsed * 10000u /
                                       ((uint64_t)iterations * 24u));
        uart_putdec(words / 256u); uart_puts(" KiB: ");
        uart_putdec(tenths_ns / 10u); uart_puts(".");
        uart_putdec(tenths_ns % 10u); uart_puts(" ns/load\n");
    }

    uart_puts("\nSequential reads, stores, and NEON fill:\n");
    for (uint32_t bytes = 4096; bytes <= 1024u * 1024u; bytes <<= 2) {
        uint32_t words = bytes / 4u, reps = bytes < 65536 ? 128 : 16;
        volatile uint32_t *buf = a53_scratch;
        uint32_t sum = 0, t = timer_read();
        for (uint32_t r = 0; r < reps; ++r)
            for (uint32_t i = 0; i < words; ++i) sum += buf[i];
        a53_sink = sum;
        a53_result("READ total ", bytes * reps, timer_elapsed(t, timer_read()));
        t = timer_read();
        for (uint32_t r = 0; r < reps; ++r)
            for (uint32_t i = 0; i < words; ++i) buf[i] = i;
        __asm__ volatile("dsb sy" ::: "memory");
        a53_result("STORE total ", bytes * reps, timer_elapsed(t, timer_read()));
        t = timer_read();
        for (uint32_t r = 0; r < reps; ++r)
            memset32_neon((uint32_t)a53_scratch, 0x12345678u, bytes);
        __asm__ volatile("dsb sy" ::: "memory");
        a53_result("NEON total ", bytes * reps, timer_elapsed(t, timer_read()));
    }

    uint32_t t = timer_read();
    memset32_neon(FB0_ADDR, 0xff081428u, LCD_FB_BYTES);
    uint32_t fill = timer_elapsed(t, timer_read());
    t = timer_read();
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    uint32_t clean = timer_elapsed(t, timer_read());
    t = timer_read();
    memcpy_neon(a53_scratch, (void *)FB0_ADDR, LCD_FB_BYTES);
    uint32_t copy = timer_elapsed(t, timer_read());
    int valid = 1;
    for (uint32_t i = 0; i < LCD_W * LCD_H; ++i)
        if (a53_scratch[i] != 0xff081428u) { valid = 0; break; }
    uart_puts("\n480x272 NEON fill: "); uart_putdec(ticks_to_us(fill));
    uart_puts(" us; cache clean: "); uart_putdec(ticks_to_us(clean));
    uart_puts(" us; NEON copy: "); uart_putdec(ticks_to_us(copy));
    uart_puts(valid ? " us; readback PASS\n" : " us; readback FAIL\n");

    pine_demo_fill_rect(screen, 16, 42, 448, 195, 0xff081428u);
    pine_demo_draw_text(screen, "COMPLETE - RESULTS ON SERIAL", 16, 46, 1, 0xffffffffu);
    char line[64];
    snprintf(line, sizeof(line), "FRAME FILL: %lu US", (unsigned long)ticks_to_us(fill));
    pine_demo_draw_text(screen, line, 16, 80, 2, 0xffffffffu);
    snprintf(line, sizeof(line), "CACHE CLEAN: %lu US", (unsigned long)ticks_to_us(clean));
    pine_demo_draw_text(screen, line, 16, 104, 2, 0xffffffffu);
    snprintf(line, sizeof(line), "FRAME COPY: %lu US", (unsigned long)ticks_to_us(copy));
    pine_demo_draw_text(screen, line, 16, 128, 2, 0xffffffffu);
    pine_demo_draw_text(screen, valid ? "NEON READBACK PASS" : "NEON READBACK FAIL",
                        16, 170, 2, valid ? 0xff80ff80u : 0xffff4040u);
    pine_demo_draw_text(screen, "A/B: MENU", 16, 244, 1, 0xffc0c0c0u);
    pine_demo_present(FB1_ADDR);
    pine_demo_wait();
    return 0;
}
