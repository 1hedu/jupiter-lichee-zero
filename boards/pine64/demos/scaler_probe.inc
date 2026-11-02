#include "support.h"
#include "pine64_video.h"

int main(void)
{
    timer_init(); mmu_init(); video_init();
    uart_puts("\n=== PINE64 VI0 VSU: 240x136 -> 480x272 ===\n");
    uart_puts("Hardware scaling, 20 fractional bits, all 32 coefficient phases loaded.\n");
    uint32_t *small = (uint32_t *)FB1_ADDR;
    static const uint32_t colors[] = {
        0xffff2020, 0xffffa020, 0xffffff20, 0xff20ff20,
        0xff20ffff, 0xff4040ff, 0xffff40ff, 0xffffffff
    };
    for (uint32_t y = 0; y < 136; ++y)
        for (uint32_t x = 0; x < 240; ++x) {
            uint32_t c = colors[x / 30u];
            if (y > 92u) c = ((x ^ y) & 1u) ? 0xffffffffu : 0xff000000u;
            if (!x || !y || x == 239u || y == 135u) c = 0xffff8000u;
            small[y * 240u + x] = c;
        }
    memset32_neon(OVL_ADDR, 0, LCD_FB_BYTES);
    uint32_t *overlay = (uint32_t *)OVL_ADDR;
    pine_demo_fill_rect(overlay, 12, 12, 456, 47, 0xd0101830u);
    pine_demo_draw_text(overlay, "A64 HARDWARE SCALER", 20, 20, 2, 0xffffffffu);
    pine_demo_draw_text(overlay, "240X136 TO 480X272 - 2X PIXELS BELOW", 20, 44, 1, 0xffffffffu);
    pine_demo_draw_text(overlay, "A/B: MENU", 20, 244, 1, 0xffff6060u);
    dcache_clean_range(OVL_ADDR, LCD_FB_BYTES);
    video_set_overlay(OVL_ADDR);
    int accepted = pine64_video_scale_rgb(FB1_ADDR, 240, 136);
    if (!accepted) {
        uart_puts("[scaler] configuration not accepted; reset display layout\n");
        video_init();
        memset32_neon(FB0_ADDR, 0xff401010u, LCD_FB_BYTES);
        pine_demo_draw_text((uint32_t *)FB0_ADDR, "SCALER SETUP FAILED", 20, 100, 2, 0xffffffffu);
        pine_demo_present(FB0_ADDR);
    } else {
        uart_puts("[scaler] register readback OK; confirm 2x checkerboard and orange border\n");
        uart_puts("Readback confirms programmed registers; the monitor verifies output.\n");
    }
    pine_demo_wait();
    return 0;
}
