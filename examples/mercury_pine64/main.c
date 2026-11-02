/* A64 host for the user's existing Mercury TV firmware. The Pico draws
 * every cube/bar pixel on its own HDMI output. This screen shows local
 * transmit status and the A64 capture limitation, not a simulated return. */
#include "jupiter.h"
#include "input.h"
#include "pine64_mercury.h"
#include "scenes.h"
#include "../../boards/pine64/demos/support.h"
#include <string.h>

static uint8_t display_list[512];
static pine64_mercury_bridge_t bridge;
static unsigned hud_index;
static int paused, pattern, dirty, bridge_probed, local_error;

static char *append(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}
static char *number(char *p, uint32_t n)
{
    char reversed[10];
    unsigned count = 0;
    do { reversed[count++] = (char)('0' + n % 10u); n /= 10u; } while (n);
    while (count) *p++ = reversed[--count];
    *p = 0;
    return p;
}
static char *hex_byte(char *p, uint8_t n)
{
    *p++ = "0123456789ABCDEF"[n >> 4];
    *p++ = "0123456789ABCDEF"[n & 15u];
    *p = 0;
    return p;
}
static void text(uint32_t *fb, int y, const char *s, uint32_t color)
{
    pine_demo_draw_text(fb, s, 18, y, 1, color);
}
static void background(void)
{
    uint32_t *fb = (uint32_t *)(uintptr_t)FB0_ADDR;
    static const uint32_t colors[8] = {
        0xff0b1026,0xff141a3a,0xff232558,0xff3a2e6e,
        0xff5c3b7e,0xff8a4a86,0xffc75b79,0xfff07a5a
    };
    for (unsigned y = 0; y < LCD_H; ++y)
        for (unsigned x = 0; x < LCD_W; ++x)
            fb[y * LCD_W + x] = colors[y * 8u / LCD_H];
    dcache_clean_fb(FB0_ADDR);
    video_swap(FB0_ADDR);
}
static void draw_hud(void)
{
    uint32_t address = (hud_index ^= 1u) ? OVL1_ADDR : OVL_ADDR;
    uint32_t *fb = (uint32_t *)(uintptr_t)address;
    memset(fb, 0, LCD_FB_BYTES);
    pine_demo_fill_rect(fb, 8, 7, LCD_W - 16, LCD_H - 14, 0xe0101420u);
    text(fb, 16, "MERCURY GPU - EXISTING PICO TV FIRMWARE", 0xfff2c14eu);
    text(fb, 32, "PINE64 SENDS DISPLAY LISTS TO THE PICO", 0xffcbd6e5u);
    text(fb, 56, pattern ? "PICO TV: BARS AND WHITE TRIANGLE" : "PICO TV: DUSK-COLOURED SPINNING CUBE", 0xff8ae7ffu);
    text(fb, 69, "CHECK THE TV FOR COLOUR / PAUSE / SCENE CHANGES", 0xffb5c8dcu);
    const pine64_mercury_stats_t *s = pine64_mercury_stats();
    char line[100], *p = append(line, "LOCAL LISTS SENT ");
    p = number(p, s->lists_sent); p = append(p, "  BYTES "); (void)number(p, s->bytes_sent);
    text(fb, 90, line, 0xffd5e4f5u);
    text(fb, 103, local_error ? pine64_mercury_error_string(local_error) :
         paused ? "PAUSED - LAST SCENE REMAINS ON PICO TV" : "SPI 1 MHZ MODE 0 - NO ACK / READBACK CHANNEL",
         local_error ? 0xffff8e80u : 0xffb6c7d9u);
    text(fb, 126, "PINE CAPTURE: CSI-2 RECEIVER NOT PRESENT", 0xffffb680u);
    text(fb, 139, "A64 CAMERA INPUT IS PARALLEL 8-BIT", 0xffc4b6d4u);
    text(fb, 152, "TC358743 CSI-2 CANNOT CONNECT DIRECTLY TO A64", 0xffc4b6d4u);
    if (!bridge_probed) {
        text(fb, 175, "V / C-UP: OPTIONAL BRIDGE I2C STATUS CHECK", 0xffa9bfd5u);
    } else if (bridge.error) {
        text(fb, 175, pine64_mercury_error_string(bridge.error), 0xffffaf87u);
        p = append(line, "PH2 SCL "); p = append(p, bridge.idle_lines & 2 ? "HIGH" : "LOW");
        p = append(p, "  PH3 SDA "); (void)append(p, bridge.idle_lines & 1 ? "HIGH" : "LOW");
        text(fb, 188, line, 0xffbbcbddu);
    } else {
        p = append(line, "BRIDGE ADDR "); p = hex_byte(p, bridge.address);
        p = append(p, "  ID "); p = hex_byte(p, (uint8_t)(bridge.chip_id >> 8));
        p = hex_byte(p, (uint8_t)bridge.chip_id);
        p = append(p, "  HDMI STATUS "); (void)hex_byte(p, bridge.status);
        text(fb, 175, line, 0xff8adbb5u);
        p = append(line, "TMDS "); p = append(p, bridge.status & 2 ? "YES" : "NO");
        p = append(p, "  SYNC "); p = append(p, bridge.status & 0x80 ? "YES" : "NO");
        p = append(p, "  SIZE "); p = number(p, bridge.width);
        p = append(p, "X"); (void)number(p, bridge.height);
        text(fb, 188, line, 0xffbbcbddu);
    }
    text(fb, 209, "SPOTPEAR: PC0 TO GP4  PC2 TO GP6  PC3 TO GP5", 0xffa9bfd5u);
    text(fb, 222, "PI-2 PINS 19 / 23 / 24 PLUS COMMON GROUND", 0xffa9bfd5u);
    text(fb, 241, "A/Z/ENTER PAUSE-RETRY  C/C-DOWN SCENE  B/X MENU", 0xff8ae7ffu);
    text(fb, 254, "V/C-UP BRIDGE READ  -  KEEP YOUR CURRENT PICO UF2", 0xffa9bfd5u);
    dcache_clean_fb(address);
    video_wait_vblank();
    video_set_overlay(address);
}

void main(void)
{
    timer_init();
    mmu_init();
    pine64_audio_stop();
    video_init();
    input_init(INPUT_N64);
    input_settle();
    pine64_demo_active(1);
    hud_index = 0;
    paused = pattern = bridge_probed = 0;
    dirty = 1;
    memset(&bridge, 0, sizeof(bridge));
    background();
    uart_puts("\n=== Mercury GPU: PINE64 host / existing Pico firmware ===\n");
    uart_puts("[mercury] Pico HDMI output is unchanged; dusk cube commands distinguish host control\n");
    uart_puts("[mercury] A64 has parallel CSI, not MIPI CSI-2 RX: TC358743 capture is unavailable\n");
    local_error = pine64_mercury_init();
    uint8_t angle = 0;
    for (;;) {
        (void)input_poll();
        uint32_t keys = input_pressed();
        if (keys & BTN_B) break;
        if (keys & BTN_A) {
            if (local_error) {
                pine64_mercury_deinit();
                local_error = pine64_mercury_init();
                dirty = 1;
            } else paused = !paused;
        }
        if (keys & (BTN_X | BTN_C_DOWN)) { pattern = !pattern; dirty = 1; }
        if (keys & (BTN_Y | BTN_C_UP)) {
            bridge_probed = 1;
            (void)pine64_mercury_probe_bridge(&bridge);
            uart_puts("[mercury/bridge] read-only check: ");
            if (bridge.error) uart_puts(pine64_mercury_error_string(bridge.error));
            else {
                uart_puts("ID="); uart_puthex(bridge.chip_id);
                uart_puts(" HDMI status="); uart_puthex(bridge.status);
                uart_puts(" size="); uart_putdec(bridge.width); uart_puts("x"); uart_putdec(bridge.height);
            }
            uart_puts("; no capture claimed\n");
        }
        if (!local_error && (!paused || dirty)) {
            size_t length = pattern ? mercury_scene_bars(display_list, sizeof(display_list)) :
                                     mercury_scene_cube(display_list, sizeof(display_list), angle);
            local_error = pine64_mercury_send_displaylist(display_list, length);
            if (local_error) pine64_mercury_dump_diagnostics();
            else {
                dirty = 0;
                if (!paused) angle = (uint8_t)(angle + 2u);
            }
        }
        draw_hud();
    }
    pine64_mercury_deinit();
    memset((void *)(uintptr_t)OVL_ADDR, 0, LCD_FB_BYTES);
    dcache_clean_fb(OVL_ADDR);
    video_set_overlay(OVL_ADDR);
    /* Parent menu reinitializes display and input after normal return. */
}
