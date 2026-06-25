/*
 * Jupiter SDK — Example Menu
 *
 * Combined build of all examples with a visual selection menu.
 * D-pad Up/Down to scroll, A to launch. Reset (power cycle) to pick
 * a different demo — menu is a boot picker, demos run forever.
 *
 * Build: make GAME=examples/menu/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>

/* ================================================================
 * 5x7 pixel font (column-encoded, from opn2_jupiter)
 * ================================================================ */
#define FNT_W 5
#define FNT_H 7
typedef struct { char ch; uint8_t cols[FNT_W]; } menu_glyph_t;

static const menu_glyph_t menu_font[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'A', {0x7E,0x09,0x09,0x09,0x7E}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},
    {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},
    {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x7F,0x20,0x18,0x20,0x7F}},
    {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}},
    {'Z', {0x61,0x51,0x49,0x45,0x43}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},
    {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},
    {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},
    {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {':', {0x00,0x24,0x00,0x00,0x00}},
    {'.', {0x00,0x40,0x00,0x00,0x00}},
    {'/', {0x20,0x10,0x08,0x04,0x02}},
    {'+', {0x08,0x08,0x3E,0x08,0x08}},
    {'_', {0x40,0x40,0x40,0x40,0x40}},
    {'(', {0x00,0x1C,0x22,0x41,0x00}},
    {')', {0x00,0x41,0x22,0x1C,0x00}},
};
#define MENU_FONT_N (sizeof(menu_font)/sizeof(menu_font[0]))

static const uint8_t *menu_glyph_for(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (uint32_t i = 0; i < MENU_FONT_N; i++)
        if (menu_font[i].ch == c) return menu_font[i].cols;
    return menu_font[0].cols;
}

static void menu_draw_text(uint32_t *fb, const char *s,
                           int x, int y, int size, uint32_t color)
{
    while (*s) {
        const uint8_t *g = menu_glyph_for(*s);
        for (int col = 0; col < FNT_W; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < FNT_H; row++) {
                if (!(bits & (1 << row))) continue;
                for (int dy = 0; dy < size; dy++) {
                    int py = y + row * size + dy;
                    if (py < 0 || py >= (int)LCD_H) continue;
                    uint32_t *line = fb + py * LCD_W;
                    for (int dx = 0; dx < size; dx++) {
                        int px = x + col * size + dx;
                        if (px < 0 || px >= (int)LCD_W) continue;
                        line[px] = color;
                    }
                }
            }
        }
        x += (FNT_W + 1) * size;
        s++;
    }
}

static void menu_fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color)
{
    for (int py = y; py < y + h && py < (int)LCD_H; py++) {
        if (py < 0) continue;
        uint32_t *row = fb + py * LCD_W;
        for (int px = x; px < x + w && px < (int)LCD_W; px++) {
            if (px < 0) continue;
            row[px] = color;
        }
    }
}

/* ================================================================
 * Example entry points (defined in ex_*.c wrapper files)
 * ================================================================ */

/* --- Video --- */
extern void colorbars_main(void);
extern void bouncing_sprite_main(void);
extern void jupiter_logo_main(void);
extern void sprites_main(void);
extern void parallax_main(void);
extern void fast_tiles_main(void);

/* --- SNES --- */
extern void mode1_main(void);
extern void mode2_main(void);
extern void snes_showcase_main(void);
extern void parallax_all_main(void);
extern void parallax_test_main(void);

/* --- Mode 7 --- */
extern void mode7_vortex_main(void);
extern void mode7_sprites_main(void);
extern void mode7_neon_main(void);

/* --- Genesis/Multi --- */
extern void genesis_vdp_main(void);
extern void trilayer_main(void);
extern void isometric_main(void);
extern void isometric_fullres_main(void);

/* --- CedarVE (H.264 hardware) --- */
extern void cedar_nes_main(void);
extern void cedar_gb_main(void);
extern void cedar_snes_main(void);
extern void cedar_genesis_main(void);
extern void cedar_jpeg_main(void);

/* --- Audio --- */
extern void jupiter_moon_main(void);
extern void av_demo_main(void);
extern void opn2_rt_main(void);
extern void opn2_jupiter_main(void);
extern void opn2_input_main(void);
extern void mt32_rt_main(void);
extern void sc55_warcraft_main(void);

/* --- YM3438 Hardware --- */
extern void ym3438_main(void);
extern void opn2_hw_live_main(void);
extern void opn2_hw_nes_main(void);
extern void opn2_hw_gb_main(void);
extern void opn2_hw_input_main(void);

/* --- Input --- */
extern void input_test_main(void);
extern void cpak_browser_main(void);

/* --- Benchmarks --- */
extern void benchmark_main(void);
extern void sram_bench_main(void);
extern void mmu_dcache_main(void);
extern void hdma_bench_main(void);
extern void hstimer_raster_main(void);
extern void scaler_probe_main(void);

/* ================================================================
 * Menu table
 * ================================================================ */

#define ENTRY_HEADER 0
#define ENTRY_DEMO   1

typedef struct {
    const char *name;
    void (*entry)(void);
    uint8_t type;
} menu_entry_t;

static const menu_entry_t entries[] = {
    {"--- VIDEO ---",           NULL,                    ENTRY_HEADER},
    {"Color Bars",              colorbars_main,          ENTRY_DEMO},
    {"Bouncing Sprite",         bouncing_sprite_main,    ENTRY_DEMO},
    {"Jupiter Logo",            jupiter_logo_main,       ENTRY_DEMO},
    {"Sprites",                 sprites_main,            ENTRY_DEMO},
    {"Parallax",                parallax_main,           ENTRY_DEMO},
    {"Fast Tiles",              fast_tiles_main,         ENTRY_DEMO},

    {"--- SNES MODES ---",      NULL,                    ENTRY_HEADER},
    {"Mode 1",                  mode1_main,              ENTRY_DEMO},
    {"Mode 2",                  mode2_main,              ENTRY_DEMO},
    {"SNES Showcase",           snes_showcase_main,      ENTRY_DEMO},
    {"Parallax All",            parallax_all_main,       ENTRY_DEMO},
    {"Parallax Test",           parallax_test_main,      ENTRY_DEMO},

    {"--- MODE 7 ---",          NULL,                    ENTRY_HEADER},
    {"Mode 7 Vortex",           mode7_vortex_main,       ENTRY_DEMO},
    {"Mode 7 Sprites",          mode7_sprites_main,      ENTRY_DEMO},
    {"Mode 7 NEON",             mode7_neon_main,         ENTRY_DEMO},

    {"--- GENESIS / MULTI ---", NULL,                    ENTRY_HEADER},
    {"Genesis VDP",             genesis_vdp_main,        ENTRY_DEMO},
    {"Tri-Layer",               trilayer_main,           ENTRY_DEMO},
    {"Isometric",               isometric_main,          ENTRY_DEMO},
    {"Isometric Fullres",       isometric_fullres_main,  ENTRY_DEMO},

    {"--- CEDARVE (H.264) ---", NULL,                    ENTRY_HEADER},
    {"Cedar NES (Vinci)",       cedar_nes_main,          ENTRY_DEMO},
    {"Cedar GB (Celebi)",       cedar_gb_main,           ENTRY_DEMO},
    {"Cedar SNES (FF6)",        cedar_snes_main,         ENTRY_DEMO},
    {"Cedar Genesis (Pulse)",   cedar_genesis_main,      ENTRY_DEMO},
    {"Cedar JPEG decode",       cedar_jpeg_main,         ENTRY_DEMO},

    {"--- AUDIO ---",           NULL,                    ENTRY_HEADER},
    {"Jupiter Moon",            jupiter_moon_main,       ENTRY_DEMO},
    {"AV Demo",                 av_demo_main,            ENTRY_DEMO},
    {"OPN2 Real-Time",          opn2_rt_main,            ENTRY_DEMO},
    {"OPN2 Jupiter",            opn2_jupiter_main,       ENTRY_DEMO},
    {"OPN2 Input",              opn2_input_main,         ENTRY_DEMO},
    {"MT-32 Real-Time",         mt32_rt_main,            ENTRY_DEMO},
    {"SC-55 Warcraft",          sc55_warcraft_main,      ENTRY_DEMO},

    {"--- YM3438 HARDWARE ---", NULL,                    ENTRY_HEADER},
    {"YM3438 VGM",              ym3438_main,             ENTRY_DEMO},
    {"HW FM + SN76489 PSG",     opn2_hw_live_main,       ENTRY_DEMO},
    {"HW FM + NES APU PSG",     opn2_hw_nes_main,        ENTRY_DEMO},
    {"HW FM + GB DMG PSG",      opn2_hw_gb_main,         ENTRY_DEMO},
    {"HW FM + Input",           opn2_hw_input_main,      ENTRY_DEMO},

    {"--- INPUT ---",           NULL,                    ENTRY_HEADER},
    {"Input Test",              input_test_main,         ENTRY_DEMO},
    {"CPak Browser",            cpak_browser_main,       ENTRY_DEMO},

    {"--- BENCHMARKS ---",      NULL,                    ENTRY_HEADER},
    {"Benchmark",               benchmark_main,          ENTRY_DEMO},
    {"SRAM Bench",              sram_bench_main,         ENTRY_DEMO},
    {"MMU D-Cache",             mmu_dcache_main,         ENTRY_DEMO},
    {"HDMA Bench",              hdma_bench_main,         ENTRY_DEMO},
    {"HSTimer Raster",          hstimer_raster_main,     ENTRY_DEMO},
    {"Scaler Probe",            scaler_probe_main,       ENTRY_DEMO},
};
#define NUM_ENTRIES (sizeof(entries)/sizeof(entries[0]))

/* ================================================================
 * Menu renderer
 * ================================================================ */

#define TEXT_SIZE     2
#define LINE_H       (FNT_H * TEXT_SIZE + 3)  /* 17px per line */
#define TITLE_Y      6
#define LIST_Y       30
#define LIST_X       16
#define VISIBLE      ((int)((LCD_H - LIST_Y - 20) / LINE_H))

#define COL_BG       0xFF0A0A1A
#define COL_TITLE    0xFF40C0FF
#define COL_HEADER   0xFF808080
#define COL_NORMAL   0xFFCCCCCC
#define COL_HILITE   0xFFFFFFFF
#define COL_BAR      0xFF1A3050
#define COL_FOOTER   0xFF606060

static void menu_render(uint32_t *fb, int sel, int scroll)
{
    /* Clear */
    memset32_neon((uint32_t)fb, COL_BG, LCD_W * LCD_H * 4);

    /* Title */
    menu_draw_text(fb, "JUPITER SDK", 160, TITLE_Y, 3, COL_TITLE);

    /* List */
    for (int i = 0; i < VISIBLE && (scroll + i) < (int)NUM_ENTRIES; i++) {
        int idx = scroll + i;
        int y = LIST_Y + i * LINE_H;

        if (idx == sel) {
            menu_fill_rect(fb, 0, y - 1, LCD_W, LINE_H, COL_BAR);
        }

        uint32_t color;
        if (entries[idx].type == ENTRY_HEADER)
            color = COL_HEADER;
        else if (idx == sel)
            color = COL_HILITE;
        else
            color = COL_NORMAL;

        menu_draw_text(fb, entries[idx].name, LIST_X, y, TEXT_SIZE, color);
    }

    /* Footer */
    menu_draw_text(fb, "A:SELECT  UP/DOWN:SCROLL  RESET:BACK", 24, LCD_H - 16, 1, COL_FOOTER);

    dcache_clean_fb((uint32_t)fb);
}

/* Skip to next selectable entry in direction dir (+1 or -1) */
static int next_selectable(int from, int dir)
{
    int idx = from + dir;
    while (idx >= 0 && idx < (int)NUM_ENTRIES) {
        if (entries[idx].type == ENTRY_DEMO) return idx;
        idx += dir;
    }
    return from;  /* stay put if nothing found */
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    video_init();

    uart_puts("\n\n========================================\n");
    uart_puts("  JUPITER SDK — Example Menu\n");
    uart_puts("========================================\n");
    uart_puts("  N64: Up/Down to scroll, A to select\n");
    uart_puts("       Power-cycle to pick a different demo\n\n");

    input_init(INPUT_N64);

    uint32_t *fb = (uint32_t *)FB0_ADDR;
    int sel = next_selectable(-1, +1);  /* first selectable */
    int scroll = 0;

    /* Clear overlay layer (UI0) — otherwise random DRAM garbage blends over
     * VI0 and washes out the menu colors. Fully transparent = show VI0 only. */
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);

    /* Clear VI1 sprite layer too */
    memset32_neon(SPR_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(SPR_ADDR);

    menu_render(fb, sel, scroll);

    while (1) {
        uint32_t t0 = timer_read();

        input_poll();
        uint16_t pressed = input_pressed();
        int changed = 0;

        if (pressed & BTN_DOWN) {
            int next = next_selectable(sel, +1);
            if (next != sel) { sel = next; changed = 1; }
        }
        if (pressed & BTN_UP) {
            int next = next_selectable(sel, -1);
            if (next != sel) { sel = next; changed = 1; }
        }

        /* Keep selection visible */
        if (changed) {
            if (sel < scroll) scroll = sel;
            if (sel >= scroll + VISIBLE) scroll = sel - VISIBLE + 1;
            if (scroll < 0) scroll = 0;
            menu_render(fb, sel, scroll);
        }

        if (pressed & BTN_A) {
            if (entries[sel].entry) {
                uart_puts("[menu] launching: ");
                uart_puts(entries[sel].name);
                uart_puts("\n");
                /* Clear the layers the menu paints to before handing off
                 * to the demo. Demos that only touch VI0 (colorbars,
                 * jupiter_logo, fast_tiles) repaint every pixel of FB0
                 * each frame so they're unaffected, but demos that paint
                 * to the UI0 overlay (sprites, bouncing_sprite,
                 * parallax's sprite layer) inherit the menu's text on
                 * top — visible as "trails" / "garbled sprites" / the
                 * colorbars demo's "blip then halt" (menu text stayed
                 * painted on OVL above the colorbars). Clear OVL +
                 * OVL1 (UI0 buffers) + SPR (VI1 sprite layer) to
                 * transparent so the demo starts on a clean overlay.
                 * VI0 framebuffers are left alone — demos own them. */
                memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
                memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
                memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
                dcache_clean_fb(OVL_ADDR);
                dcache_clean_fb(OVL1_ADDR);
                dcache_clean_fb(SPR_ADDR);
                entries[sel].entry();
                /* never returns — menu is a boot picker; hard-reset to
                 * pick another demo. */
            }
        }

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667);
    }
    return 0;
}
