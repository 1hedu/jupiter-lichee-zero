/*
 * Mercury GPU bring-up — the V3s side of the Pico polygon coprocessor
 *
 * Drives all three legs of the Mercury link and reports each one's
 * health live on the LCD + UART:
 *
 *   1. BRIDGE  — TC358743 (HDMI→CSI-2) init over TWI0, then a 1 Hz
 *                SYS_STATUS poll decoded as TMDS / PLL / SCDT / SYNC.
 *                Works with just the bridge board wired to I2C.
 *   2. SPI     — streams a spinning-cube display list to the Pico at
 *                ~60 Hz (SET_RES + CLEAR + 12 tris + SCENE_END per
 *                frame). BRING-UP TIP: plug the Pico's DVI output into
 *                any HDMI monitor — the moment this example runs, the
 *                Pico's self-test cube should be REPLACED by our cube
 *                (same shape, different colors/spin). That proves the
 *                whole V3s→SPI→Pico→raster path with no bridge at all.
 *   3. CSI     — arms the V3s capture unit and counts frames. The MIPI
 *                PHY setup is still TODO (needs the Linux register
 *                dump), so expect 0 here until that lands — the HUD
 *                says so honestly.
 *
 * Wiring (see mercury/README.md):
 *   SPI:  V3s PC1=CLK PC2=CS PC3=MOSI  →  Pico GP10=SCK GP9=CSn GP8=RX
 *   I2C:  V3s PB6=SCL PB7=SDA          →  bridge TC358743 (addr 0x0F)
 *
 * Build: make GAME=examples/mercury_gpu/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"

#include "../../mercury/v3s_side/tc358743_init.h"
#include "../../mercury/v3s_side/mercury_spi_master.h"
#include "../../mercury/v3s_side/mercury_csi_capture.h"

/* ================================================================
 * 5x7 font for the HUD (audited: every glyph used by the strings
 * below exists — scripts/check the T!)
 * ================================================================ */
typedef struct { char ch; const char *rows[7]; } glyph_t;

static const glyph_t font[] = {
    {'A',{".###.","#...#","#...#","#####","#...#","#...#","#...#"}},
    {'B',{"####.","#...#","#...#","####.","#...#","#...#","####."}},
    {'C',{".####","#....","#....","#....","#....","#....",".####"}},
    {'D',{"####.","#...#","#...#","#...#","#...#","#...#","####."}},
    {'E',{"#####","#....","#....","####.","#....","#....","#####"}},
    {'F',{"#####","#....","#....","####.","#....","#....","#...."}},
    {'G',{".###.","#....","#....","#.###","#...#","#...#",".###."}},
    {'H',{"#...#","#...#","#...#","#####","#...#","#...#","#...#"}},
    {'I',{"#####","..#..","..#..","..#..","..#..","..#..","#####"}},
    {'K',{"#...#","#..#.","#.#..","##...","#.#..","#..#.","#...#"}},
    {'L',{"#....","#....","#....","#....","#....","#....","#####"}},
    {'M',{"#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#"}},
    {'N',{"#...#","##..#","#.#.#","#..##","#...#","#...#","#...#"}},
    {'O',{".###.","#...#","#...#","#...#","#...#","#...#",".###."}},
    {'P',{"####.","#...#","#...#","####.","#....","#....","#...."}},
    {'R',{"####.","#...#","#...#","####.","#.#..","#..#.","#...#"}},
    {'S',{".####","#....","#....",".###.","....#","....#","####."}},
    {'T',{"#####","..#..","..#..","..#..","..#..","..#..","..#.."}},
    {'U',{"#...#","#...#","#...#","#...#","#...#","#...#",".###."}},
    {'V',{"#...#","#...#","#...#","#...#",".#.#.",".#.#.","..#.."}},
    {'W',{"#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},
    {'X',{"#...#",".#.#.","..#..","..#..","..#..",".#.#.","#...#"}},
    {'Y',{"#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."}},
    {'0',{".###.","#...#","#..##","#.#.#","##..#","#...#",".###."}},
    {'1',{"..#..",".##..","..#..","..#..","..#..","..#..","#####"}},
    {'2',{".###.","#...#","....#","..##.",".#...","#....","#####"}},
    {'3',{"####.","....#","....#",".###.","....#","....#","####."}},
    {'4',{"#...#","#...#","#...#","#####","....#","....#","....#"}},
    {'5',{"#####","#....","#....","####.","....#","....#","####."}},
    {'6',{".###.","#....","#....","####.","#...#","#...#",".###."}},
    {'7',{"#####","....#","...#.","..#..",".#...",".#...",".#..."}},
    {'8',{".###.","#...#","#...#",".###.","#...#","#...#",".###."}},
    {'9',{".###.","#...#","#...#",".####","....#","....#",".###."}},
    {'-',{".....",".....",".....","#####",".....",".....","....."}},
    {':',{".....","..#..",".....",".....","..#..",".....","....."}},
};

static void draw_text(volatile uint32_t *buf, uint32_t pitch,
                      const char *s, int x, int y, int scale, uint32_t color)
{
    for (; *s; s++, x += 6 * scale) {
        if (*s == ' ') continue;
        const glyph_t *g = 0;
        for (unsigned i = 0; i < sizeof(font) / sizeof(font[0]); i++)
            if (font[i].ch == *s) { g = &font[i]; break; }
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g->rows[r][c] == '#')
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++)
                            buf[(y + r * scale + sy) * pitch +
                                (x + c * scale + sx)] = color;
    }
}

static void draw_dec(volatile uint32_t *buf, uint32_t pitch,
                     uint32_t v, int x, int y, int scale, uint32_t color)
{
    char tmp[12]; int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 11);
    char out[12]; int m = 0;
    while (n > 0) out[m++] = tmp[--n];
    out[m] = 0;
    draw_text(buf, pitch, out, x, y, scale, color);
}

/* ================================================================
 * Display-list build — same wire format as mercury_displaylist.c
 * ================================================================ */
#define CMD_CLEAR      0x01
#define CMD_TRI        0x02
#define CMD_SET_RES    0x20
#define CMD_SCENE_END  0xFF
#define RGB332(r, g, b) \
    ((uint8_t)(((r) & 0xE0) | (((g) >> 3) & 0x1C) | (((b) >> 6) & 0x03)))

#define MERC_W  320
#define MERC_H  224

static uint32_t dl_put_tri(uint8_t *dl, uint32_t pos, uint8_t color,
                           int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2)
{
    dl[pos++] = CMD_TRI;
    dl[pos++] = color;
    int16_t v[6] = { x0, y0, x1, y1, x2, y2 };
    for (int i = 0; i < 6; i++) {
        dl[pos++] = (uint8_t)(v[i] & 0xFF);
        dl[pos++] = (uint8_t)((uint16_t)v[i] >> 8);
    }
    return pos;
}

/* Cube data + integer rotate/project — mirrors the Pico's self-test
 * math so the on-monitor result is directly comparable. */
static const int16_t cube_verts[8][3] = {
    {-64, -64, -64}, { 64, -64, -64}, { 64,  64, -64}, {-64,  64, -64},
    {-64, -64,  64}, { 64, -64,  64}, { 64,  64,  64}, {-64,  64,  64},
};
static const uint8_t cube_tris[12][3] = {
    {0,1,2}, {0,2,3}, {4,6,5}, {4,7,6}, {0,4,5}, {0,5,1},
    {2,6,7}, {2,7,3}, {0,3,7}, {0,7,4}, {1,5,6}, {1,6,2},
};
/* V3s cube gets its own palette so it's obvious whose cube is showing:
 * Jupiter dusk tones, not the Pico self-test primaries. */
static const uint8_t face_colors[6] = {
    RGB332(0xF0, 0x7A, 0x5A),  /* front  — dusk orange   */
    RGB332(0x23, 0x25, 0x58),  /* back   — deep indigo   */
    RGB332(0x8A, 0x4A, 0x86),  /* bottom — mauve         */
    RGB332(0xF2, 0xC1, 0x4E),  /* top    — gold          */
    RGB332(0xC7, 0x5B, 0x79),  /* left   — rose          */
    RGB332(0x48, 0xA0, 0x90),  /* right  — teal          */
};

static int16_t sinq8[256];
static void init_sin(void)
{
    for (int i = 0; i < 128; i++) {
        int v = (4 * i * (128 - i) * 256) / (128 * 128);
        sinq8[i] = (int16_t)v;
        sinq8[128 + i] = (int16_t)-v;
    }
}
#define ISIN(a) sinq8[(a) & 255]
#define ICOS(a) sinq8[((a) + 64) & 255]

static void rotate_project(const int16_t in[3], uint8_t ax, uint8_t ay,
                           int16_t *sx, int16_t *sy, int32_t *z_out)
{
    int32_t x = in[0], y = in[1], z = in[2];
    int32_t ca = ICOS(ay), sa = ISIN(ay);
    int32_t nx = (x * ca - z * sa) >> 8;
    int32_t nz = (x * sa + z * ca) >> 8;
    x = nx; z = nz;
    ca = ICOS(ax); sa = ISIN(ax);
    int32_t ny = (y * ca - z * sa) >> 8;
    nz = (y * sa + z * ca) >> 8;
    y = ny; z = nz;
    z += 256;
    if (z < 16) z = 16;
    *sx = (int16_t)(MERC_W / 2 + (x * 200) / z);
    *sy = (int16_t)(MERC_H / 2 + (y * 200) / z);
    *z_out = z;
}

static uint32_t build_cube_dl(uint8_t *dl, uint8_t angle)
{
    uint32_t pos = 0;
    dl[pos++] = CMD_CLEAR;
    dl[pos++] = RGB332(0x0B, 0x10, 0x26);       /* dusk-night backdrop */

    int16_t pv[8][2];
    int32_t pz[8];
    for (int i = 0; i < 8; i++)
        rotate_project(cube_verts[i], (uint8_t)(angle / 2), angle,
                       &pv[i][0], &pv[i][1], &pz[i]);

    /* Painter's sort: far faces first */
    int32_t face_z[6];
    int order[6] = {0, 1, 2, 3, 4, 5};
    for (int f = 0; f < 6; f++) {
        int t0 = f * 2;
        face_z[f] = (pz[cube_tris[t0][0]] + pz[cube_tris[t0][1]] +
                     pz[cube_tris[t0][2]] + pz[cube_tris[t0 + 1][2]]) / 4;
    }
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 6; j++)
            if (face_z[order[i]] < face_z[order[j]]) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }

    for (int fi = 0; fi < 6; fi++) {
        int f = order[fi];
        for (int t = 0; t < 2; t++) {
            int ti = f * 2 + t;
            pos = dl_put_tri(dl, pos, face_colors[f],
                pv[cube_tris[ti][0]][0], pv[cube_tris[ti][0]][1],
                pv[cube_tris[ti][1]][0], pv[cube_tris[ti][1]][1],
                pv[cube_tris[ti][2]][0], pv[cube_tris[ti][2]][1]);
        }
    }
    dl[pos++] = CMD_SCENE_END;
    return pos;
}

/* ================================================================
 * HUD
 * ================================================================ */
#define COL_BG     0xFF10141E
#define COL_HEAD   0xFFF2C14E
#define COL_LABEL  0xFF8899AA
#define COL_OK     0xFF48D090
#define COL_BAD    0xFFE05555
#define COL_DIM    0xFF555F6A

/* CSI capture buffer: the VI1 sprite slot — this example never uses
 * VI1, and 320x224 R3G3B2 is 70 KB of its 1 MB. */
#define CAPTURE_BUF  SPR_ADDR
/* Captured-frame window position on the LCD */
#define CAP_X  ((LCD_W - MERC_W) / 2)
#define CAP_Y  20

static void hud_static(volatile uint32_t *fb)
{
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++)
        fb[i] = COL_BG;
    draw_text(fb, LCD_W, "MERCURY GPU BRING-UP", 8, 4, 1, COL_HEAD);
    /* capture window frame */
    for (int x = CAP_X - 1; x <= CAP_X + MERC_W; x++) {
        fb[(CAP_Y - 1) * LCD_W + x]      = COL_DIM;
        fb[(CAP_Y + MERC_H) * LCD_W + x] = COL_DIM;
    }
    for (int y = CAP_Y - 1; y <= CAP_Y + MERC_H; y++) {
        fb[y * LCD_W + CAP_X - 1]      = COL_DIM;
        fb[y * LCD_W + CAP_X + MERC_W] = COL_DIM;
    }
    draw_text(fb, LCD_W, "PICO FRAME VIA CSI - MIPI PHY TODO",
              CAP_X + 8, CAP_Y + MERC_H / 2 - 4, 1, COL_DIM);
}

static void hud_status(volatile uint32_t *fb, int bridge_ok, uint8_t st,
                       uint32_t spi_sent, uint32_t spi_errs, uint32_t csi_frames)
{
    /* status strip along the bottom, redrawn every second */
    int y = LCD_H - 24;
    for (uint32_t i = (uint32_t)y * LCD_W; i < LCD_W * LCD_H; i++)
        fb[i] = COL_BG;

    draw_text(fb, LCD_W, "BRIDGE:", 8, y, 1, COL_LABEL);
    if (!bridge_ok) {
        draw_text(fb, LCD_W, "NO ACK", 56, y, 1, COL_BAD);
    } else {
        draw_text(fb, LCD_W, "TMDS", 56, y, 1,
                  (st & TC_MASK_S_TMDS)     ? COL_OK : COL_DIM);
        draw_text(fb, LCD_W, "PLL", 88, y, 1,
                  (st & TC_MASK_S_PHY_PLL)  ? COL_OK : COL_DIM);
        draw_text(fb, LCD_W, "SCDT", 112, y, 1,
                  (st & TC_MASK_S_PHY_SCDT) ? COL_OK : COL_DIM);
        draw_text(fb, LCD_W, "SYNC", 144, y, 1,
                  (st & TC_MASK_S_SYNC)     ? COL_OK : COL_DIM);
    }

    draw_text(fb, LCD_W, "SPI:", 184, y, 1, COL_LABEL);
    draw_dec(fb, LCD_W, spi_sent, 212, y, 1, spi_errs ? COL_BAD : COL_OK);
    if (spi_errs) {
        draw_text(fb, LCD_W, "E", 268, y, 1, COL_BAD);
        draw_dec(fb, LCD_W, spi_errs, 276, y, 1, COL_BAD);
    }

    draw_text(fb, LCD_W, "CSI:", 316, y, 1, COL_LABEL);
    draw_dec(fb, LCD_W, csi_frames, 344, y, 1,
             csi_frames ? COL_OK : COL_DIM);

    y += 12;
    draw_text(fb, LCD_W, "HDMI MONITOR ON PICO SHOWS THE CUBE - DUSK COLORS: V3S DRIVING",
              8, y, 1, COL_DIM);
}

/* ================================================================
 * Main
 * ================================================================ */
static uint8_t dl_buf[512];

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    video_init();
    input_init(INPUT_N64);

    uart_puts("\n=== Mercury GPU bring-up (V3s side) ===\n");

    volatile uint32_t *fb = (volatile uint32_t *)FB0_ADDR;
    hud_static(fb);

    /* --- 1. Bridge --- */
    int bridge_ok = (tc358743_init() == 0);
    uart_puts(bridge_ok ? "[mercury] TC358743 init OK\n"
                        : "[mercury] TC358743 NO ACK / bad chip id\n");

    /* --- 2. SPI link --- */
    mercury_spi_init();
    /* one-time resolution command (Pico default is already 320x224;
     * sending it also proves multi-command parsing) */
    {
        uint8_t sr[5] = { CMD_SET_RES,
                          MERC_W & 0xFF, MERC_W >> 8,
                          MERC_H & 0xFF, MERC_H >> 8 };
        mercury_spi_send_displaylist(sr, sizeof(sr));
    }

    /* --- 3. CSI capture (MIPI PHY still TODO — counts stay 0 until
     *        that lands, and the HUD says so) --- */
    csi_clocks_init();
    csi_gpio_init();
    csi_capture_init(CAPTURE_BUF, MERCURY_RES_GENESIS);

    dcache_clean_fb(FB0_ADDR);
    video_swap(FB0_ADDR);

    uint8_t  angle = 0;
    uint32_t spi_sent = 0, spi_errs = 0, csi_frames = 0;
    uint32_t frame = 0;
    uint8_t  st = 0;

    while (1) {
        /* stream this frame's display list */
        uint32_t len = build_cube_dl(dl_buf, angle);
        if (mercury_spi_send_displaylist(dl_buf, len) == 0) spi_sent++;
        else spi_errs++;
        angle++;

        /* captured frame? convert into the LCD window */
        if (csi_frame_ready()) {
            csi_frames++;
            dcache_invalidate_range(CAPTURE_BUF, MERC_W * MERC_H);
            const uint8_t *src = (const uint8_t *)CAPTURE_BUF;
            for (int y = 0; y < MERC_H; y++) {
                volatile uint32_t *dst = fb + (CAP_Y + y) * LCD_W + CAP_X;
                mercury_r3g3b2_to_argb((uint32_t *)dst,
                                       src + y * MERC_W, MERC_W, 1);
            }
        }

        /* 1 Hz status refresh */
        if ((frame++ & 63) == 0) {
            if (bridge_ok) st = tc358743_status();
            hud_status(fb, bridge_ok, st, spi_sent, spi_errs, csi_frames);
            uart_puts("[mercury] status=");  uart_puthex(st);
            uart_puts(" spi=");   uart_putdec(spi_sent);
            uart_puts(" errs=");  uart_putdec(spi_errs);
            uart_puts(" csi=");   uart_putdec(csi_frames);
            uart_puts("\n");
            dcache_clean_fb(FB0_ADDR);
        } else if (csi_frames) {
            dcache_clean_fb(FB0_ADDR);
        }

        video_wait_vblank();
    }
    return 0;
}
