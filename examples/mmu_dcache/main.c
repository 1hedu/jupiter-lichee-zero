/*
 * Jupiter SDK — MMU + D-Cache Test (Option B)
 *
 * Proves: Cached framebuffer writes + explicit D-cache flush for DE2 coherency
 *
 * Memory map:
 *   0x00-0x01F  Device      Peripherals
 *   0x400-0x41F Cached WBWA Code, data, stack, tile maps
 *   0x420-0x43F Cached WBWA Framebuffers (flushed before swap)
 *
 * Benchmark sequence:
 *   1. Baseline render (I-cache only, no MMU)
 *   2. Enable MMU — ALL DRAM cached
 *   3. Render + flush — shows both times separately
 */
#include "jupiter.h"

/* ---- Tile config (same as parallax example) ---- */
#define MAP_W       80
#define MAP_H_FAR   17
#define MAP_H_NEAR  17
#define HORIZON     (LCD_H / 2)

static uint8_t sky_map[MAP_H_FAR * MAP_W];
static uint8_t ground_map[MAP_H_NEAR * MAP_W];

static uint32_t sky_color(uint8_t id, uint32_t px, uint32_t py)
{
    (void)px; (void)py;
    switch (id) {
    case 0: return 0x00102040;
    case 1: return 0x00183058;
    case 2: return 0x00203860;
    case 3: return 0x00284068;
    default: return 0x00000000;
    }
}

static uint32_t ground_color(uint8_t id, uint32_t px, uint32_t py)
{
    (void)px; (void)py;
    switch (id) {
    case 4: return 0x00206020;
    case 5: return 0x00308030;
    case 6: return 0x0040A040;
    case 7: return 0x00604020;
    default: return 0x00000000;
    }
}

static void build_maps(void)
{
    for (uint32_t y = 0; y < MAP_H_FAR; y++)
        for (uint32_t x = 0; x < MAP_W; x++) {
            uint8_t base = (y < 5) ? 0 : (y < 10) ? 1 : (y < 14) ? 2 : 3;
            if (((x * 7 + y * 13) % 23) == 0) base = 1;
            sky_map[y * MAP_W + x] = base;
        }

    for (uint32_t y = 0; y < MAP_H_NEAR; y++)
        for (uint32_t x = 0; x < MAP_W; x++) {
            uint8_t base = 5;
            if (y == 0) base = 6;
            if (y > 12) base = 4;
            if (((x + y * 3) % 11) == 0) base = 7;
            ground_map[y * MAP_W + x] = base;
        }
}

static void render_frame(volatile uint32_t *fb, int scroll_far, int scroll_near)
{
    tiles_render(fb, LCD_W, sky_map, MAP_W,
                 0, MAP_H_FAR, scroll_far, sky_color);
    tiles_render(fb, LCD_W, ground_map, MAP_W,
                 HORIZON, MAP_H_NEAR, scroll_near, ground_color);
}

/* ==== ENTRY ==== */
void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — MMU + D-Cache Test (Option B) ===\n");
    timer_init();

    build_maps();

    /* Clear all buffers (before MMU — writes go uncached) */
    memset32_neon(FB0_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    uart_puts("Display live.\n\n");

    /* ---- BENCHMARK: before D-cache ---- */
    uart_puts("--- BASELINE (I-cache only) ---\n");
    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;

    uint32_t t0 = timer_read();
    render_frame(fb0, 0, 0);
    uint32_t t1 = timer_read();
    uint32_t us_baseline = ticks_to_us(timer_elapsed(t0, t1));

    uart_puts("  Render: ");
    uart_putdec(us_baseline);
    uart_puts(" us\n");
    video_swap(FB0_ADDR);

    /* ---- ENABLE MMU + D-CACHE (everything cached) ---- */
    uart_puts("\nEnabling MMU + D-cache (all DRAM cached)...\n");
    mmu_init();
    uart_puts("MMU active.\n\n");

    /* ---- BENCHMARK: render + flush ---- */
    uart_puts("--- WITH D-CACHE (FB cached + flush) ---\n");
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;

    t0 = timer_read();
    render_frame(fb1, 0, 0);
    t1 = timer_read();
    uint32_t us_render = ticks_to_us(timer_elapsed(t0, t1));

    uint32_t t2 = timer_read();
    dcache_clean_fb(FB1_ADDR);
    uint32_t t3 = timer_read();
    uint32_t us_flush = ticks_to_us(timer_elapsed(t2, t3));

    uint32_t us_total = us_render + us_flush;

    uart_puts("  Render: ");
    uart_putdec(us_render);
    uart_puts(" us\n");
    uart_puts("  Flush:  ");
    uart_putdec(us_flush);
    uart_puts(" us\n");
    uart_puts("  Total:  ");
    uart_putdec(us_total);
    uart_puts(" us\n\n");

    /* ---- RESULT ---- */
    uart_puts("  ");
    uart_putdec(us_baseline);
    uart_puts(" us -> ");
    uart_putdec(us_total);
    uart_puts(" us  (");
    if (us_total > 0) uart_putdec(us_baseline / us_total);
    else uart_puts("?");
    uart_puts("x speedup)\n\n");

    video_swap(FB1_ADDR);

    /* ---- GAME LOOP ---- */
    uart_puts("Entering main loop...\n");

    uint32_t back_fb  = FB0_ADDR;
    uint32_t front_fb = FB1_ADDR;
    int scroll_far = 0, scroll_near = 0;
    uint32_t frame = 0;

    while (1) {
        t0 = timer_read();

        volatile uint32_t *fb = (volatile uint32_t *)back_fb;
        render_frame(fb, scroll_far, scroll_near);

        t1 = timer_read();
        dcache_clean_fb(back_fb);
        t2 = timer_read();

        video_swap(back_fb);
        uint32_t tmp = back_fb;
        back_fb = front_fb;
        front_fb = tmp;

        scroll_far  += 1;
        scroll_near += 3;

        uint32_t r = ticks_to_us(timer_elapsed(t0, t1));
        uint32_t f = ticks_to_us(timer_elapsed(t1, t2));

        if ((frame % 120) == 0) {
            uart_puts("f=");
            uart_putdec(frame);
            uart_puts(" r=");
            uart_putdec(r);
            uart_puts(" f=");
            uart_putdec(f);
            uart_puts(" t=");
            uart_putdec(r + f);
            uart_puts("us\n");
        }
        frame++;

        /* Pace to 30fps */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 33333)
            ;
    }
}
