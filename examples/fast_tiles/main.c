/*
 * Jupiter SDK — Fast Tile Renderer Test
 *
 * Benchmarks old tiles_render() vs new tiles_render_fast()
 * on the same parallax scene, then runs the game loop.
 *
 * Three bottlenecks killed:
 *   1. Function pointer per pixel → color LUT (indexed load)
 *   2. Non-power-of-2 modulo     → AND mask (map_w = 64)
 *   3. Per-pixel tile lookup      → 8-pixel tile runs
 *
 * Target: 68ms → under 16.6ms (60fps)
 */
#include "jupiter.h"

/* ---- Map config ---- */
#define MAP_W       64      /* POWER OF 2 — critical for fast path */
#define MAP_H_FAR   17
#define MAP_H_NEAR  17
#define HORIZON     (LCD_H / 2)

static uint8_t sky_map[MAP_H_FAR * MAP_W];
static uint8_t ground_map[MAP_H_NEAR * MAP_W];

/* ---- Color LUT (replaces function pointer + switch) ---- */
static const uint32_t sky_lut[256] = {
    [0] = 0x00102040,  /* deep sky */
    [1] = 0x00183058,  /* sky with star */
    [2] = 0x00203860,  /* lighter sky */
    [3] = 0x00284068,  /* lightest sky */
};

static const uint32_t ground_lut[256] = {
    [4] = 0x00206020,  /* dark grass */
    [5] = 0x00308030,  /* grass */
    [6] = 0x0040A040,  /* light grass */
    [7] = 0x00604020,  /* dirt/path */
};

/* ---- Old-style color functions (for baseline comparison) ---- */
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

/* ---- Map builder ---- */
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

/* ---- Render helpers ---- */
static void render_old(volatile uint32_t *fb, int sf, int sn)
{
    tiles_render(fb, LCD_W, sky_map, MAP_W,
                 0, MAP_H_FAR, sf, sky_color);
    tiles_render(fb, LCD_W, ground_map, MAP_W,
                 HORIZON, MAP_H_NEAR, sn, ground_color);
}

static void render_fast(volatile uint32_t *fb, int sf, int sn)
{
    tiles_render_fast(fb, LCD_W, sky_map, MAP_W, sky_lut,
                      0, MAP_H_FAR, sf);
    tiles_render_fast(fb, LCD_W, ground_map, MAP_W, ground_lut,
                      HORIZON, MAP_H_NEAR, sn);
}

/* ==== ENTRY ==== */
void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — Fast Tile Renderer ===\n");
    timer_init();
    build_maps();

    memset32_neon(FB0_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);

    video_init();
    uart_puts("Display live.\n\n");

    /* ---- Enable MMU + D-cache first ---- */
    mmu_init();
    uart_puts("MMU + D-cache active.\n\n");

    /* ---- BENCHMARK: old renderer ---- */
    uart_puts("--- OLD (function ptr, mod 64) ---\n");
    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;

    uint32_t t0 = timer_read();
    render_old(fb0, 0, 0);
    uint32_t t1 = timer_read();
    dcache_clean_fb(FB0_ADDR);
    uint32_t us_old = ticks_to_us(timer_elapsed(t0, t1));

    uart_puts("  Render: ");
    uart_putdec(us_old);
    uart_puts(" us\n");
    video_swap(FB0_ADDR);

    /* ---- BENCHMARK: new renderer ---- */
    uart_puts("--- NEW (LUT, AND mask, tile runs) ---\n");
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;

    t0 = timer_read();
    render_fast(fb1, 0, 0);
    t1 = timer_read();
    uint32_t t2 = timer_read();
    dcache_clean_fb(FB1_ADDR);
    uint32_t t3 = timer_read();
    uint32_t us_new = ticks_to_us(timer_elapsed(t0, t1));
    uint32_t us_flush = ticks_to_us(timer_elapsed(t2, t3));

    uart_puts("  Render: ");
    uart_putdec(us_new);
    uart_puts(" us\n");
    uart_puts("  Flush:  ");
    uart_putdec(us_flush);
    uart_puts(" us\n");
    uart_puts("  Total:  ");
    uart_putdec(us_new + us_flush);
    uart_puts(" us\n\n");

    /* ---- RESULT ---- */
    uart_puts("  ");
    uart_putdec(us_old);
    uart_puts(" -> ");
    uart_putdec(us_new + us_flush);
    uart_puts(" us  (");
    if ((us_new + us_flush) > 0)
        uart_putdec(us_old / (us_new + us_flush));
    else
        uart_puts("INF");
    uart_puts("x faster)\n");

    if (us_new + us_flush < 16667)
        uart_puts("  ** 60fps capable **\n");
    else if (us_new + us_flush < 33333)
        uart_puts("  ** 30fps capable **\n");
    uart_puts("\n");

    video_swap(FB1_ADDR);

    /* ---- GAME LOOP ---- */
    uart_puts("Entering game loop...\n");

    uint32_t back_fb  = FB0_ADDR;
    uint32_t front_fb = FB1_ADDR;
    int scroll_far = 0, scroll_near = 0;
    uint32_t frame = 0;

    while (1) {
        t0 = timer_read();

        volatile uint32_t *fb = (volatile uint32_t *)back_fb;
        render_fast(fb, scroll_far, scroll_near);

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

        /* Pace to 60fps — we should be fast enough now */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            ;
    }
}
