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
 *   3. CSI     — the V3s MIPI receive chain (D-PHY RX → CSI-2
 *                controller → CSI0 DMA, mercury_csi_capture.h) takes
 *                the bridge's 640x480 stream; each finished frame is
 *                sampled back down to the Pico's 320x224 canvas into the
 *                VI1 window. CSI: counts real frame-done events.
 *                The second HUD row is the CSI-2 link itself:
 *                  FS/FE  frame start/end packets seen (lit = arriving)
 *                  ERR    ECC/checksum/sync errors (lit = bad link)
 *                  DT:xx  data type of the last packet (24 = RGB888,
 *                         1E = UYVY; 00/01 are frame start/end)
 *                  PKT:n  packets received
 *                The third row is each end's own view of the link:
 *                  IN:WxH  active size the bridge measures on HDMI
 *                  TX/HLT  bridge CSI transmitter sending / halted
 *                  RX      V3s receiver registers read back (DEAD =
 *                          block unclocked or held in reset)
 *                Button A restarts the link (receiver, then bridge
 *                stream); B swaps red/blue in case the bridge's RGB888
 *                byte order is the other way round.
 *
 * The screen itself is the README's Mercury layer stack, composited by
 * the DE2 blender in hardware:
 *   VI0 = V3s background (dusk scene, drawn once)
 *   VI1 = the Pico's 3D frame in a hardware-positioned 320x224 window
 *   UI0 = V3s HUD with per-pixel alpha on top
 * — the 32X arrangement: 2D console layers around a coprocessor's 3D.
 *
 * Wiring — docs/PIN_OVERLAYS.md Overlay 4, Pico 2 W + SpotPear HDMI:
 *   SPI:  V3s PC1=CLK PC2=CS PC3=MOSI  →  Pico GP6=SCK GP5=CSn GP4=RX
 *   I2C:  V3s PE21=SCL PE22=SDA (TWI1) →  bridge TC358743 (addr 0x0F)
 * (Other Pico DVI boards: see the board presets in mercury/README.md.)
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

static void draw_hex2(volatile uint32_t *buf, uint32_t pitch,
                      uint32_t v, int x, int y, uint32_t color)
{
    static const char hx[] = "0123456789ABCDEF";
    char out[3] = { hx[(v >> 4) & 15], hx[v & 15], 0 };
    draw_text(buf, pitch, out, x, y, 1, color);
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

/* ---- The Mercury layer stack, for real (the 32X pitch) ----
 *   VI0 (FB0)      V3s background — dusk scene, drawn once
 *   VI1 (SPR)      the Pico's 3D frame, a hardware-positioned window
 *   UI0 (OVL)      V3s HUD, per-pixel alpha on top
 * The DE2 blender composites all three during scanout — the only CPU
 * pixel work after boot is sampling each captured CSI frame down into
 * the VI1 buffer. */

/* VI1 scans SPR_ADDR (ARGB, pitch = window width). The raw CSI frames
 * land in the two capture megabytes mercury_csi_capture.h owns. */
#define WINDOW_BUF   SPR_ADDR
/* Pico-frame window position on the LCD */
#define CAP_X  ((LCD_W - MERC_W) / 2)
#define CAP_Y  14

static uint32_t hash2(uint32_t x, uint32_t y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* VI0: the V3s background layer — dusk bands, stars, ridge line.
 * Drawn once; the blender does the rest forever. */
static void draw_background(volatile uint32_t *fb)
{
    static const uint32_t bands[8] = {
        0xFF0B1026, 0xFF141A3A, 0xFF232558, 0xFF3A2E6E,
        0xFF5C3B7E, 0xFF8A4A86, 0xFFC75B79, 0xFFF07A5A,
    };
    for (uint32_t y = 0; y < LCD_H; y++) {
        uint32_t c = bands[y * 8 / LCD_H];
        for (uint32_t x = 0; x < LCD_W; x++)
            fb[y * LCD_W + x] = c;
    }
    for (uint32_t y = 8; y < LCD_H / 2; y += 8)
        for (uint32_t x = 4; x < LCD_W - 4; x += 8)
            if ((hash2(x, y) % 37) == 0)
                fb[y * LCD_W + x] = 0xFFE8ECFF;
    for (uint32_t x = 0; x < LCD_W; x++) {
        uint32_t h = 10 + (hash2(x >> 4, 7) % 14);
        for (uint32_t y = LCD_H - h; y < LCD_H; y++)
            fb[y * LCD_W + x] = 0xFF191230;
    }
    /* frame around where the VI1 window sits */
    for (int x = CAP_X - 1; x <= CAP_X + MERC_W; x++) {
        fb[(CAP_Y - 1) * LCD_W + x]      = COL_HEAD;
        fb[(CAP_Y + MERC_H) * LCD_W + x] = COL_HEAD;
    }
    for (int y = CAP_Y - 1; y <= CAP_Y + MERC_H; y++) {
        fb[y * LCD_W + CAP_X - 1]      = COL_HEAD;
        fb[y * LCD_W + CAP_X + MERC_W] = COL_HEAD;
    }
}

/* VI1: placeholder content until CSI frames arrive (dark panel +
 * faint grid so the hardware window is visibly its own layer). */
static void draw_window_placeholder(volatile uint32_t *win)
{
    for (int y = 0; y < MERC_H; y++)
        for (int x = 0; x < MERC_W; x++) {
            uint32_t c = COL_BG;
            if ((x & 31) == 0 || (y & 31) == 0) c = 0xFF1A2130;
            win[y * MERC_W + x] = c;
        }
    draw_text(win, MERC_W, "WAITING FOR PICO FRAME VIA CSI",
              MERC_W / 2 - 90, MERC_H / 2 - 4, 1, COL_DIM);
}

/* UI0: title + status strip, per-pixel alpha over everything. The
 * strip's three rows sit just below the window (CAP_Y + MERC_H). */
#define HUD_Y  (LCD_H - 32)
static void hud_title(volatile uint32_t *ovl)
{
    draw_text(ovl, LCD_W, "MERCURY GPU BRING-UP", 8, 4, 1, COL_HEAD);
}

/* bridge_state: 0 = answering, TC_ERR_* = init failed, 1 = answered at
 * init but this second's status read wasn't ACKed */
#define BRIDGE_I2C_LOST 1
static void hud_status(volatile uint32_t *ovl, int bridge_state, uint8_t st,
                       uint32_t spi_sent, uint32_t spi_errs,
                       uint32_t csi_frames, const csi_mipi_status_t *ms,
                       uint32_t mipi_seen, const tc358743_diag_t *td)
{
    /* translucent status strip along the bottom, redrawn every second */
    int y = HUD_Y + 1;
    for (uint32_t i = (uint32_t)HUD_Y * LCD_W; i < LCD_W * LCD_H; i++)
        ovl[i] = 0xC010141E;

    draw_text(ovl, LCD_W, "BRIDGE:", 8, y, 1, COL_LABEL);
    if (bridge_state == TC_ERR_NOACK) {
        draw_text(ovl, LCD_W, "NO ACK", 56, y, 1, COL_BAD);
    } else if (bridge_state == TC_ERR_ID) {
        draw_text(ovl, LCD_W, "BAD ID", 56, y, 1, COL_BAD);
    } else if (bridge_state == TC_ERR_VERIFY) {
        draw_text(ovl, LCD_W, "I2C BAD", 56, y, 1, COL_BAD);
    } else if (bridge_state == BRIDGE_I2C_LOST) {
        draw_text(ovl, LCD_W, "I2C LOST", 56, y, 1, COL_BAD);
    } else {
        draw_text(ovl, LCD_W, "TMDS", 56, y, 1,
                  (st & TC_MASK_S_TMDS)     ? COL_OK : COL_DIM);
        draw_text(ovl, LCD_W, "PLL", 88, y, 1,
                  (st & TC_MASK_S_PHY_PLL)  ? COL_OK : COL_DIM);
        draw_text(ovl, LCD_W, "SCDT", 112, y, 1,
                  (st & TC_MASK_S_PHY_SCDT) ? COL_OK : COL_DIM);
        draw_text(ovl, LCD_W, "SYNC", 144, y, 1,
                  (st & TC_MASK_S_SYNC)     ? COL_OK : COL_DIM);
    }

    draw_text(ovl, LCD_W, "SPI:", 184, y, 1, COL_LABEL);
    draw_dec(ovl, LCD_W, spi_sent, 212, y, 1, spi_errs ? COL_BAD : COL_OK);
    if (spi_errs) {
        draw_text(ovl, LCD_W, "E", 268, y, 1, COL_BAD);
        draw_dec(ovl, LCD_W, spi_errs, 276, y, 1, COL_BAD);
    }

    draw_text(ovl, LCD_W, "CSI:", 316, y, 1, COL_LABEL);
    draw_dec(ovl, LCD_W, csi_frames, 344, y, 1,
             csi_frames ? COL_OK : COL_DIM);

    /* CSI-2 link row: what the controller saw in the last second */
    y += 10;
    draw_text(ovl, LCD_W, "MIPI:", 8, y, 1, COL_LABEL);
    draw_text(ovl, LCD_W, "FS", 44, y, 1,
              (mipi_seen & MCSI_PD_FRAME_START) ? COL_OK : COL_DIM);
    draw_text(ovl, LCD_W, "FE", 62, y, 1,
              (mipi_seen & MCSI_PD_FRAME_END) ? COL_OK : COL_DIM);
    draw_text(ovl, LCD_W, "ERR", 80, y, 1,
              (mipi_seen & MCSI_PD_ERRORS) ? COL_BAD : COL_DIM);
    draw_text(ovl, LCD_W, "DT:", 110, y, 1, COL_LABEL);
    draw_hex2(ovl, LCD_W, ms->pkt_hdr & 0x3F, 128, y,
              ms->pkt_count ? COL_OK : COL_DIM);
    draw_text(ovl, LCD_W, "PKT:", 152, y, 1, COL_LABEL);
    draw_dec(ovl, LCD_W, ms->pkt_count, 176, y, 1,
             ms->pkt_count ? COL_OK : COL_DIM);
    draw_text(ovl, LCD_W, "OVF:", 260, y, 1, COL_LABEL);
    draw_dec(ovl, LCD_W, csi_overflows, 284, y, 1,
             csi_overflows ? COL_BAD : COL_DIM);

    /* Link row: both ends' own view. IN = active size the bridge
     * measured on its HDMI input, TX = bridge's CSI transmitter
     * sending, HLT = it halted on an error, RX = the V3s receiver's
     * enables read back (DEAD = block unclocked / in reset). */
    y += 10;
    draw_text(ovl, LCD_W, "LINK:", 8, y, 1, COL_LABEL);
    draw_text(ovl, LCD_W, "IN:", 44, y, 1, COL_LABEL);
    uint32_t in_ok = td->width == 640 && td->height == 480;
    draw_dec(ovl, LCD_W, td->width, 62, y, 1, in_ok ? COL_OK : COL_BAD);
    draw_text(ovl, LCD_W, "X", 86, y, 1, COL_LABEL);
    draw_dec(ovl, LCD_W, td->height, 92, y, 1, in_ok ? COL_OK : COL_BAD);
    draw_text(ovl, LCD_W, "TX", 122, y, 1,
              (td->csi_status & TC_MASK_S_TXACT) ? COL_OK : COL_DIM);
    draw_text(ovl, LCD_W, "HLT", 140, y, 1,
              (td->csi_status & TC_MASK_S_HLT) ? COL_BAD : COL_DIM);
    draw_text(ovl, LCD_W, "RX:", 170, y, 1, COL_LABEL);
    if (csi_rx_alive())
        draw_text(ovl, LCD_W, "OK", 188, y, 1, COL_OK);
    else
        draw_text(ovl, LCD_W, "DEAD", 188, y, 1, COL_BAD);
    draw_text(ovl, LCD_W, "A:RELINK  B:SWAP R-B", 236, y, 1, COL_DIM);
}

/* ================================================================
 * Main
 * ================================================================ */
static uint8_t dl_buf[512];

/* (Re)start the CSI-2 link in Linux's order: receiver configured
 * first, then the bridge's LP-11 -> HS stream start. */
static void link_start(int bridge_ok)
{
    csi_capture_init();
    if (bridge_ok) tc358743_stream_on();
}

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    video_init();
    input_init(INPUT_N64);
    init_sin();             /* cube rotation table — without it every
                             * vertex projects to one point (zero-area
                             * tris: the Pico would draw nothing) */

    uart_puts("\n=== Mercury GPU bring-up (V3s side) ===\n");

    /* Build the three hardware layers (see the layer-stack comment):
     * background on VI0, Pico window on VI1, HUD on UI0. */
    volatile uint32_t *fb  = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *win = (volatile uint32_t *)WINDOW_BUF;
    volatile uint32_t *ovl = (volatile uint32_t *)OVL_ADDR;

    draw_background(fb);
    draw_window_placeholder(win);
    memset32_neon(OVL_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    hud_title(ovl);
    dcache_clean_range(WINDOW_BUF, MERC_W * MERC_H * 4);
    dcache_clean_fb(OVL_ADDR);
    video_vi1_init(CAP_X, CAP_Y, MERC_W, MERC_H);

    /* --- 1. Bridge: configured, video muted until the receiver is up --- */
    int bridge_rc = tc358743_init();
    int bridge_ok = (bridge_rc == 0);
    switch (bridge_rc) {
    case 0:
        uart_puts("[mercury] TC358743 init OK at I2C 0x");
        uart_puthex(tc_i2c_addr); uart_puts("\n");
        break;
    case TC_ERR_NOACK:
        uart_puts("[mercury] TC358743: no ACK at 0x0F or 0x1F "
                  "(SDA/SCL, power, pull-ups?)\n");
        break;
    case TC_ERR_ID:
        uart_puts("[mercury] TC358743: something answered but CHIPID "
                  "isn't 0x00xx\n");
        break;
    default:
        uart_puts("[mercury] TC358743: register write didn't read back "
                  "(I2C unreliable)\n");
        break;
    }

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

    /* --- 3. MIPI receive chain, then start the bridge's stream (the
     *        D-PHY must see the lanes go LP-11 -> HS) --- */
    csi_gpio_init();
    link_start(bridge_ok);
    uart_puts("[mercury] MIPI RX up, CSI-2 version=");
    uart_puthex(csi_mipi_version);
    uart_puts(bridge_ok ? ", bridge streaming\n" : ", no bridge\n");

    dcache_clean_fb(FB0_ADDR);
    video_swap(FB0_ADDR);

    uint8_t  angle = 0;
    uint32_t spi_sent = 0, spi_errs = 0, csi_frames = 0;
    uint32_t frame = 0, mipi_seen = 0;
    uint8_t  st = 0;
    int      swap_rb = 0;
    csi_mipi_status_t ms = csi_mipi_status();
    tc358743_diag_t td = {0};

    while (1) {
        /* stream this frame's display list */
        uint32_t len = build_cube_dl(dl_buf, angle);
        if (mercury_spi_send_displaylist(dl_buf, len) == 0) spi_sent++;
        else spi_errs++;
        angle++;

        input_poll();
        if (input_pressed() & BTN_B) swap_rb ^= 1;
        if (input_pressed() & BTN_A) {
            uart_puts("[mercury] relink\n");
            link_start(bridge_ok);
        }

        /* Wait for the next captured frame (this also paces the loop
         * to the Pico's 60 Hz while frames flow; with no stream it
         * times out after ~one frame). The finished buffer is ours
         * until the next call — convert it into the VI1 window and
         * the blender composites it in hardware. */
        uint32_t cap = csi_wait_frame(20000);
        if (cap) {
            csi_frames++;
            dcache_invalidate_range(cap, MERCURY_CSI_FRAME_BYTES);
            csi_frame_to_argb((uint32_t *)WINDOW_BUF, (const uint8_t *)cap,
                              MERC_W, MERC_H, swap_rb);
            dcache_clean_range(WINDOW_BUF, MERC_W * MERC_H * 4);
        }

        /* 1 Hz status refresh on the UI0 overlay */
        if ((frame++ & 63) == 0) {
            int bridge_state = bridge_rc;
            if (bridge_ok) {
                if (tc358743_status_rd(&st)) {
                    st = 0;
                    bridge_state = BRIDGE_I2C_LOST;
                }
                td = tc358743_diag();
            }
            ms = csi_mipi_status();
            mipi_seen = ms.pending;
            hud_status(ovl, bridge_state, st, spi_sent, spi_errs,
                       csi_frames, &ms, mipi_seen, &td);
            uart_puts("[mercury] status=");  uart_puthex(st);
            if (bridge_state == BRIDGE_I2C_LOST) uart_puts("(I2C LOST)");
            uart_puts(" spi=");   uart_putdec(spi_sent);
            uart_puts(" errs=");  uart_putdec(spi_errs);
            uart_puts(" csi=");   uart_putdec(csi_frames);
            uart_puts(" ovf=");   uart_putdec(csi_overflows);
            uart_puts("\n          mipi pd="); uart_puthex(ms.pending);
            uart_puts(" ph=");    uart_puthex(ms.pkt_hdr);
            uart_puts(" pkts=");  uart_putdec(ms.pkt_count);
            uart_puts(" frm=");   uart_putdec(ms.frame_num);
            uart_puts(" line=");  uart_putdec(ms.line_num);
            uart_puts(" csi_sta="); uart_puthex(ms.csi_sta);
            uart_puts(swap_rb ? " rb=swapped\n" : "\n");
            uart_puts("          bridge in=");  uart_putdec(td.width);
            uart_puts("x");      uart_putdec(td.height);
            uart_puts(" csi_status="); uart_puthex(td.csi_status);
            uart_puts(" csi_err=");    uart_puthex(td.csi_err);
            uart_puts(" confctl=");    uart_puthex(td.confctl);
            uart_puts("\n          rx ctl=");   uart_puthex(MCSI_R(MCSI_CTL_REG));
            uart_puts(" cfg=");     uart_puthex(MCSI_R(MCSI_CFG_REG));
            uart_puts(" vcdt=");    uart_puthex(MCSI_R(MCSI_VCDT_RX_REG));
            uart_puts(" dphy gctl="); uart_puthex(DPHY_R(DPHY_GCTL_REG));
            uart_puts(" rxctl=");   uart_puthex(DPHY_R(DPHY_RX_CTL_REG));
            uart_puts(" dbg=");     uart_puthex(DPHY_R(0xF0));
            uart_puts(",");         uart_puthex(DPHY_R(0xF4));
            uart_puts(",");         uart_puthex(DPHY_R(0xF8));
            uart_puts(",");         uart_puthex(DPHY_R(0xFC));
            uart_puts(" csi en=");  uart_puthex(CSI_R(CSI_EN_REG));
            uart_puts(" ccu134=");  uart_puthex(CCU_R(0x134));
            uart_puts(" ccu16c=");  uart_puthex(CCU_R(0x16C));
            uart_puts(" pll_isp="); uart_puthex(CCU_R(0x02C));
            uart_puts("\n");
            dcache_clean_range(OVL_ADDR + HUD_Y * LCD_W * 4,
                               (LCD_H - HUD_Y) * LCD_W * 4);
        }

    }
    return 0;
}
