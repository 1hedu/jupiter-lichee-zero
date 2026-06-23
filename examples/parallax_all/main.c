/*
 * Jupiter SDK — SNES Parallax Checker Test
 *
 * Cycles SNES modes 0-5 (all multi-layer modes), ~7 seconds each.
 * Largest checkers on top layer (BG1), smallest on bottom — so you
 * can see through the big gaps down to every layer beneath.
 * 256-color modes (3,4): bordered tiles with rainbow gradient.
 * Offset modes (2,4): sine wave distortion on BG1.
 * VI1 PIP label in top-left shows mode number + BPP config.
 *
 * Build: make GAME=examples/parallax_all/main.c
 */
#include "jupiter.h"
#include "snes.h"

#define FRAMES_PER_MODE 420   /* ~7 seconds at 60fps */
#define NUM_MODES       6     /* modes 0-5 */

/* ================================================================
 *  TILES
 * ================================================================ */

static uint8_t tiles_2bpp[2 * 16];
static uint8_t tiles_4bpp[2 * 32];
static uint8_t tiles_8bpp[2 * 64];

static void init_tiles(void)
{
    for (int i = 0; i < 16; i++)
        tiles_2bpp[16 + i] = 0x55;

    for (int i = 0; i < 32; i++)
        tiles_4bpp[32 + i] = 0x11;

    uint8_t *p = tiles_8bpp + 64;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            if (x == 0 || x == 7 || y == 0 || y == 7)
                p[y*8+x] = 1;
            else
                p[y*8+x] = (uint8_t)(10 + y * 6 + x);
        }
}

/* ================================================================
 *  PALETTES
 * ================================================================ */

static const uint32_t pal2_red[4]    = { 0x00000000, 0xFFDD4444, 0xFFFF6666, 0xFFBB2222 };
static const uint32_t pal2_blue[4]   = { 0x00000000, 0xFF4444DD, 0xFF6666FF, 0xFF2222BB };
static const uint32_t pal2_green[4]  = { 0x00000000, 0xFF44DD44, 0xFF66FF66, 0xFF22BB22 };
static const uint32_t pal2_yellow[4] = { 0x00000000, 0xFFDDDD44, 0xFFFFFF66, 0xFFBBBB22 };

static const uint32_t pal4_red[16]   = { 0x00000000, 0xFFDD4444, [2 ... 15]=0xFF800000 };
static const uint32_t pal4_blue[16]  = { 0x00000000, 0xFF4444DD, [2 ... 15]=0xFF000080 };

static uint32_t pal_256[256];

static void init_pal_256(void)
{
    pal_256[0] = 0x00000000;
    pal_256[1] = 0xFF303030;
    for (int i = 2; i < 256; i++) {
        int h = (i * 4) % 256;
        int r, g, b;
        int seg = h / 43, f = (h - seg * 43) * 6;
        switch (seg) {
            case 0: r=255; g=f;     b=0;     break;
            case 1: r=255-f; g=255; b=0;     break;
            case 2: r=0;   g=255;   b=f;     break;
            case 3: r=0;   g=255-f; b=255;   break;
            case 4: r=f;   g=0;     b=255;   break;
            default: r=255; g=0;    b=255-f; break;
        }
        pal_256[i] = 0xFF000000 | ((uint32_t)r<<16) | ((uint32_t)g<<8) | (uint32_t)b;
    }
}

/* ================================================================
 *  MAPS + OFFSET + SINE
 * ================================================================ */

#define MW 48   /* divisible by 1,2,3,4 with even block counts — no seam */
#define MH 48
static uint16_t map_a[MH*MW], map_b[MH*MW], map_c[MH*MW], map_d[MH*MW];

static void make_checker(uint16_t *map, int block)
{
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            int bx = x / block, by = y / block;
            map[y*MW+x] = ((bx+by)&1) ? SNES_ENTRY(1,0,0,0,0) : 0;
        }
}

#define MAX_COLS 64
static int16_t offset_table[MAX_COLS];

#define SIN_N 64
static int16_t sin_tbl[SIN_N];

static void init_sin(void)
{
    static const int8_t q[16] = {0,1,2,3,4,5,5,6,6,6,6,6,5,5,4,3};
    for (int i = 0; i < 16; i++) {
        sin_tbl[i]    =  q[i];
        sin_tbl[32-i] =  q[i];
        sin_tbl[32+i] = -q[i];
        if (64-i < 64) sin_tbl[64-i] = -q[i];
    }
    sin_tbl[0] = 0;
}

/* ================================================================
 *  BG STATE + MODE SETUP
 *
 *  KEY: biggest checkers on top (BG1), smallest on bottom.
 *  BG1 is highest priority — big blocks with big gaps let you
 *  see through to every layer beneath.
 * ================================================================ */

static snes_bg_t bg[4];
static snes_tile_offset_t ofs;

static void setup_mode(int mode)
{
    for (int i = 0; i < 4; i++) {
        bg[i].enabled  = 0;
        bg[i].scroll_x = 0;
        bg[i].scroll_y = 0;
    }

    switch (mode) {
    case 0: /* 4 BGs, all 2bpp — top=8x8 red, 4x4 blue, 2x2 green, bottom=1x1 yellow */
        make_checker(map_a, 8);
        make_checker(map_b, 4);
        make_checker(map_c, 2);
        make_checker(map_d, 1);
        bg[0] = (snes_bg_t){ tiles_2bpp, map_a, pal2_red,    0,0, MW,MH, 2, 1 };
        bg[1] = (snes_bg_t){ tiles_2bpp, map_b, pal2_blue,   0,0, MW,MH, 2, 1 };
        bg[2] = (snes_bg_t){ tiles_2bpp, map_c, pal2_green,  0,0, MW,MH, 2, 1 };
        bg[3] = (snes_bg_t){ tiles_2bpp, map_d, pal2_yellow, 0,0, MW,MH, 2, 1 };
        break;

    case 1: /* 3 BGs: 4/4/2 — top=4x4 red, 2x2 blue, bottom=1x1 green */
        make_checker(map_a, 4);
        make_checker(map_b, 2);
        make_checker(map_c, 1);
        bg[0] = (snes_bg_t){ tiles_4bpp, map_a, pal4_red,   0,0, MW,MH, 4, 1 };
        bg[1] = (snes_bg_t){ tiles_4bpp, map_b, pal4_blue,  0,0, MW,MH, 4, 1 };
        bg[2] = (snes_bg_t){ tiles_2bpp, map_c, pal2_green, 0,0, MW,MH, 2, 1 };
        break;

    case 2: /* 2 BGs: 4/4 + offset — top=4x4 red (sine), bottom=2x2 blue */
        make_checker(map_a, 4);
        make_checker(map_b, 2);
        bg[0] = (snes_bg_t){ tiles_4bpp, map_a, pal4_red,  0,0, MW,MH, 4, 1 };
        bg[1] = (snes_bg_t){ tiles_4bpp, map_b, pal4_blue, 0,0, MW,MH, 4, 1 };
        ofs.col_offset = offset_table;
        ofs.vertical = 1;
        break;

    case 3: /* 2 BGs: 8/4 — top=4x4 bordered rainbow, bottom=2x2 blue */
        make_checker(map_a, 4);
        make_checker(map_b, 2);
        bg[0] = (snes_bg_t){ tiles_8bpp, map_a, pal_256,   0,0, MW,MH, 8, 1 };
        bg[1] = (snes_bg_t){ tiles_4bpp, map_b, pal4_blue, 0,0, MW,MH, 4, 1 };
        break;

    case 4: /* 2 BGs: 8/2 + offset — top=4x4 bordered rainbow (sine), bottom=2x2 green */
        make_checker(map_a, 4);
        make_checker(map_b, 2);
        bg[0] = (snes_bg_t){ tiles_8bpp, map_a, pal_256,    0,0, MW,MH, 8, 1 };
        bg[1] = (snes_bg_t){ tiles_2bpp, map_b, pal2_green, 0,0, MW,MH, 2, 1 };
        ofs.col_offset = offset_table;
        ofs.vertical = 1;
        break;

    case 5: /* 2 BGs: 4/2 — top=4x4 red, bottom=2x2 blue */
        make_checker(map_a, 4);
        make_checker(map_b, 2);
        bg[0] = (snes_bg_t){ tiles_4bpp, map_a, pal4_red,  0,0, MW,MH, 4, 1 };
        bg[1] = (snes_bg_t){ tiles_2bpp, map_b, pal2_blue, 0,0, MW,MH, 2, 1 };
        break;
    }
}

/* ================================================================
 *  VI1 LABEL (bitmap font, top-left PIP)
 *  Supports: 0-9 M O D E F S + space
 * ================================================================ */

static const uint8_t font_5x7[19][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /*  0: 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /*  1: 1 */
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, /*  2: 2 */
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, /*  3: 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /*  4: 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /*  5: 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /*  6: 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /*  7: 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /*  8: 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /*  9: 9 */
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x11}, /* 10: M */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* 11: O */
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, /* 12: D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* 13: E */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 14: space */
    {0x00,0x00,0x04,0x0E,0x04,0x00,0x00}, /* 15: + */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* 16: F */
    {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}, /* 17: S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* 18: T */
};

static int font_index(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    switch (c) {
        case 'M': return 10;  case 'O': return 11;
        case 'D': return 12;  case 'E': return 13;
        case '+': return 15;  case 'F': return 16;
        case 'S': return 17;  case 'T': return 18;
    }
    return 14; /* space / unknown */
}

#define VI1_W 240
#define VI1_H 24

static void vi1_draw_label(const char *str)
{
    uint32_t *vb = (uint32_t *)SPR_ADDR;
    for (int i = 0; i < VI1_W * VI1_H; i++)
        vb[i] = 0xFF102040;
    for (int x = 0; x < VI1_W; x++)
        vb[(VI1_H-1) * VI1_W + x] = 0xFF4080C0;

    int cx = 6;
    while (*str) {
        const uint8_t *g = font_5x7[font_index(*str)];
        for (int r = 0; r < 7; r++) {
            uint8_t row = g[r];
            for (int c = 0; c < 5; c++) {
                if (row & (0x10 >> c)) {
                    int sx = cx + c*2, sy = 4 + r*2;
                    if (sx+1 < VI1_W && sy+1 < VI1_H) {
                        vb[sy*VI1_W + sx]         = 0xFFFFFFFF;
                        vb[sy*VI1_W + sx + 1]     = 0xFFFFFFFF;
                        vb[(sy+1)*VI1_W + sx]     = 0xFFFFFFFF;
                        vb[(sy+1)*VI1_W + sx + 1] = 0xFFFFFFFF;
                    }
                }
            }
        }
        cx += 12;
        str++;
    }
    dcache_clean_range(SPR_ADDR, VI1_W * VI1_H * 4);
}

/* ================================================================
 *  MAIN LOOP
 * ================================================================ */

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — SNES Parallax Checker Test ===\n");
    uart_puts("Cycles modes 0-5 (multi-layer), 7 seconds each.\n");
    uart_puts("Big checkers on top, small on bottom.\n\n");

    timer_init();
    mmu_init();
    init_tiles();
    init_pal_256();
    init_sin();
    video_init();
    video_vi1_init(8, 8, VI1_W, VI1_H);

    static const char *labels[6] = {
        "MODE 0 2+2+2+2",
        "MODE 1 4+4+2",
        "MODE 2 4+4 OFFSET",
        "MODE 3 8+4",
        "MODE 4 8+2 OFFSET",
        "MODE 5 4+2",
    };
    vi1_draw_label(labels[0]);

    uint32_t back = FB1_ADDR, front = FB0_ADDR;
    uint32_t frame = 0;
    int cur_mode = -1;

    while (1) {
        uint32_t t0 = timer_read();
        int mode = (int)((frame / FRAMES_PER_MODE) % NUM_MODES);
        uint32_t mf = frame % FRAMES_PER_MODE;

        if (mode != cur_mode) {
            setup_mode(mode);
            cur_mode = mode;
            vi1_draw_label(labels[mode]);
            uart_puts("\n>> "); uart_puts(labels[mode]); uart_puts("\n");
        }

        uint32_t *fb = (uint32_t *)back;
        uint32_t *ovl = (uint32_t *)OVL_ADDR;
        uint32_t backdrop = 0xFF1A1A2A;

        switch (mode) {
        case 0: /* 4 layers — fastest on top */
            bg[0].scroll_x = (int32_t)(mf);
            bg[1].scroll_x = (int32_t)(mf * 3 / 4);
            bg[2].scroll_x = (int32_t)(mf / 2);
            bg[3].scroll_x = (int32_t)(mf / 4);
            snes_mode0_render(fb, ovl, LCD_W, LCD_H, backdrop,
                              &bg[0], &bg[1], &bg[2], &bg[3], 0);
            break;

        case 1: /* 3 layers */
            bg[0].scroll_x = (int32_t)(mf);
            bg[1].scroll_x = (int32_t)(mf / 2);
            bg[2].scroll_x = (int32_t)(mf / 4);
            snes_mode1_render(fb, ovl, LCD_W, LCD_H, backdrop,
                              &bg[0], &bg[1], &bg[2], 0);
            break;

        case 2: /* 2 layers + sine offset on BG1 */
            bg[0].scroll_x = (int32_t)(mf);
            bg[1].scroll_x = (int32_t)(mf / 2);
            for (int c = 0; c < MAX_COLS; c++) {
                int phase = (int)(mf + c * 2) & (SIN_N - 1);
                offset_table[c] = sin_tbl[phase];
            }
            snes_mode2_render(fb, ovl, LCD_W, LCD_H, backdrop,
                              &bg[0], &bg[1], &ofs, 0);
            break;

        case 3: /* 8bpp bordered + 4bpp */
            bg[0].scroll_x = (int32_t)(mf);
            bg[1].scroll_x = (int32_t)(mf / 2);
            snes_mode3_render(fb, ovl, LCD_W, LCD_H, backdrop,
                              &bg[0], &bg[1], 0);
            break;

        case 4: /* 8bpp bordered + 2bpp + sine offset */
            bg[0].scroll_x = (int32_t)(mf);
            bg[1].scroll_x = (int32_t)(mf / 2);
            for (int c = 0; c < MAX_COLS; c++) {
                int phase = (int)(mf * 2 + c * 3) & (SIN_N - 1);
                offset_table[c] = sin_tbl[phase];
            }
            snes_mode4_render(fb, ovl, LCD_W, LCD_H, backdrop,
                              &bg[0], &bg[1], &ofs, 0);
            break;

        case 5: /* 4bpp + 2bpp */
            bg[0].scroll_x = (int32_t)(mf);
            bg[1].scroll_x = (int32_t)(mf / 2);
            snes_mode5_render(fb, ovl, LCD_W, LCD_H, backdrop,
                              &bg[0], &bg[1], 0);
            break;
        }

        dcache_clean_fb(back);
        dcache_clean_fb(OVL_ADDR);
        uint32_t t1 = timer_read();
        video_swap(back);
        uint32_t tmp = back; back = front; front = tmp;
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;

        if ((frame % 120) == 0) {
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" m="); uart_putdec(mode);
            uart_puts(" render="); uart_putdec(ticks_to_us(timer_elapsed(t0, t1)));
            uart_puts("us\n");
        }
        frame++;
    }
}
