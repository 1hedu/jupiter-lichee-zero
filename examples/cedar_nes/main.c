/*
 * NES PPU + CedarVE Demo — Mendel Palace (Vinci)
 *
 * Full cedar pipeline in the sprite render path (modeled on cedar_snes):
 *   1. Construct an ARGB sprite atlas from the source CHR + NES palette
 *      (8 frames × 48×72 pixels at 3× scale, laid out horizontally →
 *      384×72, padded to 384×80 for H.264 macroblock alignment).
 *   2. CedarVE H.264 round-trip on the atlas: ARGB → NV12 (sw)
 *      → cedar_h264_encode (hw) → cedar_h264_decode (hw)
 *      → cedar_nv12_to_argb (sw). On real silicon this is clean; in
 *      QEMU there will be visible H.264 artifacts on the sprite.
 *   3. Cut the decoded ARGB per-frame back to NES 2bpp CHR tiles via
 *      nearest-palette match against the small NES sprite palette.
 *   4. Render the cedar-derived tiles via the authentic NES PPU
 *      pipeline at 256×224 with vblank sync.
 *
 * VI1 shows the raw (non-cedar) thumbnail with the red frame indicator
 * — a side-by-side reference of pristine source vs cedar-fed render.
 *
 * Build: make GAME=examples/cedar_nes/main.c
 */
#include "jupiter.h"
#include "nes.h"
#include "pmu.h"
#include <string.h>

#include "vinci_chr.h"
#include "vinci_thumb.h"

#define ANIM_FRAMES   8
#define SRC_TW        2
#define SRC_TH        3
#define SRC_PW        16
#define SRC_PH        24
#define SRC_TILES     (SRC_TW * SRC_TH)

#define SCALE         3
#define META_PW       (SRC_PW * SCALE)
#define META_PH       (SRC_PH * SCALE)
#define META_TW       (META_PW / 8)
#define META_TH       ((META_PH + 7) / 8)
#define META_TILES    (META_TW * META_TH)

/* Per-frame CHR storage — populated from the cedar-round-tripped atlas */
static uint8_t meta_chr[ANIM_FRAMES * META_TILES * 16];

/* Atlas: 8 frames × 48 px wide horizontally = 384, 72 tall (3× scaled),
 * padded to 80 for H.264 16-row alignment. */
#define ATLAS_W       384
#define ATLAS_H       80
#define ATLAS_ADDR    0x43400000
static uint32_t * const atlas = (uint32_t *)ATLAS_ADDR;

/* NES sprite palette as ARGB. vinci_spr_palette = { 0x0D, 0x0D, 0x35, 0x36 }
 *   0x0D = pure black, 0x35 = pink, 0x36 = pale pink (FCEUX) */
static const uint32_t vinci_pal_rgb[4] = {
    0x00000000,  /* index 0 — transparent */
    0xFF000000,  /* index 1 — 0x0D black */
    0xFFFCA8BC,  /* index 2 — 0x35 pink */
    0xFFFCC4C4,  /* index 3 — 0x36 pale pink */
};

static const int frame_px[ANIM_FRAMES] = { 16, 64, 112, 160, 208, 256, 304, 352 };
static const int frame_py = 9;
#define SRC_SHEET_W 384
#define SRC_SHEET_H 960

/* ----- Build the ARGB atlas from source CHR + palette ------------- */
static void build_atlas(void)
{
    /* Background: deep blue so cedar artifacts on transparent regions
     * are obvious vs the NES backdrop. */
    for (uint32_t i = 0; i < ATLAS_W * ATLAS_H; i++)
        atlas[i] = 0xFF000080;

    for (int f = 0; f < ANIM_FRAMES; f++) {
        int frame_x0 = f * META_PW;
        int base = f * SRC_TILES;
        for (int y = 0; y < META_PH; y++) {
            int sy = y / SCALE;
            int src_ty = sy / 8;
            int src_fy = sy & 7;
            for (int x = 0; x < META_PW; x++) {
                int sx = x / SCALE;
                int src_tx = sx / 8;
                int src_fx = sx & 7;
                if (src_ty >= SRC_TH || src_tx >= SRC_TW) continue;
                const uint8_t *tile = &vinci_spr_chr[
                    (base + src_ty * SRC_TW + src_tx) * 16];
                uint8_t bp0 = (tile[src_fy] >> (7 - src_fx)) & 1;
                uint8_t bp1 = (tile[src_fy + 8] >> (7 - src_fx)) & 1;
                int ci = (bp1 << 1) | bp0;
                if (ci != 0)
                    atlas[y * ATLAS_W + (frame_x0 + x)] =
                        vinci_pal_rgb[ci] | 0xFF000000;
            }
        }
    }
}

/* ----- Nearest-palette match (decoded ARGB → 0-3 color index) ----- */
static int nearest_pal_idx(uint32_t c)
{
    int best = 0, bd = 999999;
    int cr = (c >> 16) & 0xFF, cg = (c >> 8) & 0xFF, cb = c & 0xFF;
    for (int i = 0; i < 4; i++) {
        uint32_t p = vinci_pal_rgb[i];
        int dr = cr - (int)((p>>16)&0xFF);
        int dg = cg - (int)((p>>8)&0xFF);
        int db = cb - (int)(p&0xFF);
        int d = dr*dr + dg*dg + db*db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* ----- Cut decoded atlas → NES 2bpp tiles per frame --------------- */
static void cut_atlas_to_tiles(void)
{
    for (int f = 0; f < ANIM_FRAMES; f++) {
        int frame_x0 = f * META_PW;
        static uint8_t ci_grid[META_PW * META_PH];

        for (int y = 0; y < META_PH; y++)
            for (int x = 0; x < META_PW; x++)
                ci_grid[y * META_PW + x] =
                    nearest_pal_idx(atlas[y * ATLAS_W + (frame_x0 + x)]);

        uint8_t *out = &meta_chr[f * META_TILES * 16];
        for (int ty = 0; ty < META_TH; ty++) {
            for (int tx = 0; tx < META_TW; tx++) {
                uint8_t *tile = out + (ty * META_TW + tx) * 16;
                for (int row = 0; row < 8; row++) {
                    uint8_t bp0 = 0, bp1 = 0;
                    for (int col = 0; col < 8; col++) {
                        int px = tx * 8 + col;
                        int py = ty * 8 + row;
                        uint8_t ci = (px < META_PW && py < META_PH)
                                     ? ci_grid[py * META_PW + px] : 0;
                        bp0 |= ((ci & 1) << (7 - col));
                        bp1 |= (((ci >> 1) & 1) << (7 - col));
                    }
                    tile[row] = bp0;
                    tile[row + 8] = bp1;
                }
            }
        }
    }
}

static uint8_t bg_chr[256 * 16];
static uint8_t palette_ram[32];
static uint8_t nametable[NES_NT_TILES];
static uint8_t attribute[NES_NT_ATTRS];
static nes_oam_entry_t oam[NES_MAX_SPRITES];

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    cedar_init();

    uart_puts("\n========================================\n");
    uart_puts("  Vinci — NES PPU + CedarVE H.264\n");
    uart_puts("========================================\n\n");

    build_atlas();
    uart_puts("[main] atlas built ("); uart_putdec(ATLAS_W);
    uart_puts("x"); uart_putdec(ATLAS_H); uart_puts(")\n");

    cedar_argb_to_nv12(atlas, ATLAS_W, ATLAS_W, ATLAS_H);
    int enc_sz = cedar_h264_encode(ATLAS_W, ATLAS_H, 10);
    int cedar_ok = 0;
    if (enc_sz > 0) {
        uart_puts("[main] encoded: "); uart_putdec(enc_sz); uart_puts("B\n");
        int rc = cedar_h264_decode((const uint8_t *)cedar_enc_stream_addr(),
                                   enc_sz, ATLAS_W, ATLAS_H, 36, 10, 0, 0, 1);
        if (rc == 0) {
            /* decode invalidates its output buffers internally */
            cedar_nv12_to_argb(atlas, ATLAS_W, ATLAS_W, ATLAS_H);
            cedar_ok = 1;
            uart_puts("[main] cedar round-trip OK\n");
        } else {
            uart_puts("[main] decode fail — falling back to raw\n");
        }
    } else {
        uart_puts("[main] encode fail — falling back to raw\n");
    }
    if (!cedar_ok) build_atlas();

    cut_atlas_to_tiles();
    uart_puts("[main] cut to "); uart_putdec(ANIM_FRAMES);
    uart_puts(" frames × "); uart_putdec(META_TILES); uart_puts(" tiles\n");

    /* BG palettes (NES master-palette indices):
     *   pal0 sky:    navy / white stars / pale-yellow moon shade
     *   pal1 ground: dark green / mid green / bright glints
     *   pal2 glow:   navy / dusk purple-blue horizon
     *   pal3 city:   purple-blue gaps / lit windows / black buildings
     * universal backdrop 0x0F = black night zenith. */
    memset(palette_ram, 0x0D, sizeof(palette_ram));
    palette_ram[0]  = 0x0F;
    palette_ram[1]  = 0x01; palette_ram[2]  = 0x30; palette_ram[3]  = 0x38;
    palette_ram[5]  = 0x09; palette_ram[6]  = 0x1A; palette_ram[7]  = 0x2A;
    palette_ram[9]  = 0x01; palette_ram[10] = 0x03; palette_ram[11] = 0x0F;
    palette_ram[13] = 0x03; palette_ram[14] = 0x28; palette_ram[15] = 0x0F;
    palette_ram[17] = vinci_spr_palette[1];
    palette_ram[18] = vinci_spr_palette[2];
    palette_ram[19] = vinci_spr_palette[3];

    /* BG tiles as ASCII art, '0'..'3' = color index in the block's
     * attribute-selected palette ('0' is always the backdrop). */
    enum {
        T_VOID,                          /* backdrop only          */
        T_NAVY, T_DBN,                   /* sky + zenith dither    */
        T_STAR, T_WARM, T_SPARK,         /* sparse stars           */
        T_MOON_00, T_MOON_01, T_MOON_10, T_MOON_11,
        T_DNB, T_GLOW,                   /* horizon glow band      */
        T_PEAK_L, T_PEAK_R,              /* tower tops in the glow */
        T_TALL_TL, T_TALL_TR,            /* skyline silhouette:    */
        T_MID_TL,  T_MID_TR,             /* 2-tile-wide buildings, */
        T_LOW_TL,  T_LOW_TR,             /* three roof heights     */
        T_BODY_L,  T_BODY_R,
        T_FRINGE, T_GBASE, T_GRASS,      /* moonlit walk band      */
        T_GLINT, T_GDARK,                /* foreground fade        */
    };
    static const char bg_art[][8][9] = {
        [T_VOID] = {"00000000","00000000","00000000","00000000",
                    "00000000","00000000","00000000","00000000"},
        [T_NAVY] = {"11111111","11111111","11111111","11111111",
                    "11111111","11111111","11111111","11111111"},
        [T_DBN]  = {"00001111","00001111","00001111","00001111",
                    "11110000","11110000","11110000","11110000"},
        [T_STAR] = {"11111111","11111111","11121111","11111111",
                    "11111111","11111111","11111111","11111111"},
        [T_WARM] = {"11111111","11111111","11111111","11111111",
                    "11111131","11111111","11111111","11111111"},
        [T_SPARK]= {"11111111","11121111","11222111","11121111",
                    "11111111","11111111","11111111","11111111"},
        [T_MOON_00]={"11111112","11112222","11122222","11222222",
                    "12233222","12233222","12222222","22222222"},
        [T_MOON_01]={"21111111","22221111","22222111","22222211",
                    "23322221","23322221","22222221","22222222"},
        [T_MOON_10]={"22222222","12223332","12223332","12223332",
                    "11222222","11122222","11112222","11111112"},
        [T_MOON_11]={"22222222","22222221","22332221","22332221",
                    "22332211","22222111","22221111","21111111"},
        [T_DNB]  = {"11112222","11112222","11112222","11112222",
                    "22221111","22221111","22221111","22221111"},
        [T_GLOW] = {"22222222","22222222","22222222","22222222",
                    "22222222","22222222","22222222","22222222"},
        [T_PEAK_L]= {"22222222","22222222","22222222","22222222",
                    "23333333","23333333","23333333","23333333"},
        [T_PEAK_R]= {"22222222","22232222","22232222","22232222",
                    "33333332","33333332","33333332","33333332"},
        [T_TALL_TL]={"13333333","13333333","13333333","13333333",
                    "13333333","13333333","13333333","13333333"},
        [T_TALL_TR]={"33333331","33333331","33333331","33333331",
                    "33333331","33333331","33333331","33333331"},
        [T_MID_TL]= {"11111111","11111111","11111111","11111111",
                    "13333333","13333333","13333333","13333333"},
        [T_MID_TR]= {"11111111","11111111","11111111","11111111",
                    "33333331","33333331","33333331","33333331"},
        [T_LOW_TL]= {"11111111","11111111","13333333","13333333",
                    "13333333","13333333","13333333","13333333"},
        [T_LOW_TR]= {"11111111","11111111","33333331","33333331",
                    "33333331","33333331","33333331","33333331"},
        [T_BODY_L]= {"13333333","13233233","13333333","13333333",
                    "13333333","13333333","13333333","13333333"},
        [T_BODY_R]= {"33333331","33333331","33333331","33333331",
                    "33233231","33333331","33333331","33333331"},
        [T_FRINGE]= {"22112211","12211221","11111111","11111111",
                    "11111111","11211111","11111111","11111121"},
        [T_GBASE]= {"11111111","11111111","11111111","11111111",
                    "11111111","11111111","11111111","11111111"},
        [T_GRASS]= {"11111111","11211111","12121111","11111111",
                    "11111211","11112121","11111111","11111111"},
        [T_GLINT]= {"11111111","11111111","11113111","11111111",
                    "12111111","11111111","11111111","11111111"},
        [T_GDARK]= {"11110000","11110000","11110000","11110000",
                    "00001111","00001111","00001111","00001111"},
    };
    memset(bg_chr, 0, sizeof(bg_chr));
    for (unsigned t = 0; t < sizeof(bg_art)/sizeof(bg_art[0]); t++)
        for (int r = 0; r < 8; r++) {
            uint8_t bp0 = 0, bp1 = 0;
            for (int c = 0; c < 8; c++) {
                uint8_t ci = (uint8_t)(bg_art[t][r][c] - '0');
                bp0 |= (ci & 1) << (7 - c);
                bp1 |= ((ci >> 1) & 1) << (7 - c);
            }
            bg_chr[t*16 + r]     = bp0;
            bg_chr[t*16 + r + 8] = bp1;
        }

    /* Night scene: black zenith -> navy starfield -> purple horizon
     * glow -> city silhouette -> moonlit grass where Vinci walks. */
    memset(nametable, 0, sizeof(nametable));
    for (int x = 0; x < NES_NT_W; x++) {
        nametable[0*NES_NT_W+x] = T_VOID;
        nametable[1*NES_NT_W+x] = T_DBN;
        for (int y = 2; y < 20; y++) {              /* starfield      */
            uint8_t t = T_NAVY;
            if (((x*13 + y*7)  & 31) == 3)  t = T_STAR;
            if (((x*17 + y*11) & 63) == 20) t = T_WARM;
            if (((x*11 + y*17) & 63) == 5)  t = T_SPARK;
            nametable[y*NES_NT_W+x] = t;
        }
        {                                           /* city skyline   */
            /* roofline: 16 two-tile buildings, 0=tall 1=mid 2=low */
            static const uint8_t roof[16] =
                { 0,2,1,0, 2,1,0,1, 2,0,1,2, 0,1,2,1 };
            int v = roof[(x >> 1) & 15], right = x & 1;
            nametable[20*NES_NT_W+x] = T_DNB;       /* horizon glow   */
            nametable[21*NES_NT_W+x] =
                (v==0) ? (right ? T_PEAK_R : T_PEAK_L) : T_GLOW;
            nametable[22*NES_NT_W+x] =
                (v==0) ? (right ? T_TALL_TR : T_TALL_TL)
              : (v==1) ? (right ? T_MID_TR  : T_MID_TL)
              :          T_NAVY;                    /* sky over low   */
            nametable[23*NES_NT_W+x] =
                (v==2) ? (right ? T_LOW_TR : T_LOW_TL)
              :          (right ? T_BODY_R : T_BODY_L);
        }
        nametable[24*NES_NT_W+x] = T_FRINGE;        /* walk band      */
        for (int y = 25; y < 27; y++) {
            int h = (x*5 + y*13) & 15;
            nametable[y*NES_NT_W+x] = (h == 2) ? T_GRASS
                                    : (h == 9) ? T_GLINT : T_GBASE;
        }
        nametable[27*NES_NT_W+x] = T_GDARK;         /* fade to black  */
        for (int y = 28; y < NES_NT_H; y++)
            nametable[y*NES_NT_W+x] = T_VOID;
    }
    /* big blocky moon, 2x2 tiles, left of center */
    nametable[3*NES_NT_W+6] = T_MOON_00; nametable[3*NES_NT_W+7] = T_MOON_01;
    nametable[4*NES_NT_W+6] = T_MOON_10; nametable[4*NES_NT_W+7] = T_MOON_11;

    memset(attribute, 0, sizeof(attribute));        /* sky = pal0     */
    for (int ax = 0; ax < 8; ax++) {
        attribute[5*8+ax] = NES_ATTR(2,2,3,3);      /* glow, city     */
        attribute[6*8+ax] = NES_ATTR(1,1,1,1);      /* ground         */
        attribute[7*8+ax] = NES_ATTR(1,1,1,1);
    }

    video_init();
    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i = 0; i < LCD_W*LCD_H; i++) { fb0[i]=0xFF000000; fb1[i]=0xFF000000; }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    #define VIS_B 2
    video_vi1_init(LCD_W - VINCI_THUMB_W - VIS_B*2 - 4, 4,
                   VINCI_THUMB_W + VIS_B*2, VINCI_THUMB_H + VIS_B*2);
    volatile uint32_t *vi1 = (volatile uint32_t *)SPR_ADDR;
    int vi1_p = VINCI_THUMB_W + VIS_B*2;

    for (int y = 0; y < VINCI_THUMB_H + VIS_B*2; y++)
        for (int x = 0; x < vi1_p; x++)
            vi1[y*vi1_p+x] = (x<VIS_B||x>=VINCI_THUMB_W+VIS_B||
                               y<VIS_B||y>=VINCI_THUMB_H+VIS_B)
                              ? 0xFF00FF00 : vinci_thumb[((y-VIS_B)*VINCI_THUMB_W)+(x-VIS_B)];
    dcache_clean_range(SPR_ADDR, vi1_p*(VINCI_THUMB_H+VIS_B*2)*4);

    nes_bg_t bg = {
        .chr=bg_chr, .nametable=nametable, .attribute=attribute,
        .palette_ram=palette_ram, .scroll_x=0, .scroll_y=0,
        .line_scroll_x=NULL, .enabled=1,
    };

    uart_puts("[main] go!\n");

    int buf=0, frame=0, spr_x=80, spr_dx=1, flip=0;
    uint32_t last_t = timer_read();

    while (1) {
        volatile uint32_t *fb = buf?fb1:fb0;
        volatile uint32_t *ovl = (volatile uint32_t*)(buf?OVL1_ADDR:OVL_ADDR);
        uint32_t fb_addr = buf?FB1_ADDR:FB0_ADDR;
        uint32_t ovl_addr = buf?OVL1_ADDR:OVL_ADDR;

        uint32_t now = timer_read();
        if (ticks_to_ms(timer_elapsed(last_t, now)) > 100) {
            frame = (frame+1) % ANIM_FRAMES;
            last_t = now;
        }

        spr_x += spr_dx;
        if (spr_x > NES_NATIVE_W - META_PW - 4) { spr_dx=-1; flip=1; }
        if (spr_x < 4) { spr_dx=1; flip=0; }
        int spr_y = 25*8 - META_PH;

        int num_oam = 0;
        for (int ty = 0; ty < META_TH && num_oam < NES_MAX_SPRITES; ty++)
            for (int tx = 0; tx < META_TW && num_oam < NES_MAX_SPRITES; tx++) {
                int ox = flip ? (META_TW-1-tx)*8 : tx*8;
                oam[num_oam++] = (nes_oam_entry_t){
                    .y = (uint8_t)(spr_y + ty*8 - 1),
                    .tile = (uint8_t)(ty*META_TW + tx),
                    .attr = NES_SPR_PAL(0) | (flip ? NES_SPR_HFLIP : 0),
                    .x = (uint8_t)(spr_x + ox),
                };
            }
        const uint8_t *frame_chr = &meta_chr[frame * META_TILES * 16];

        {
            static int pf = -1;
            if (frame != pf) {
                for (int y=0; y<VINCI_THUMB_H; y++)
                    for (int x=0; x<VINCI_THUMB_W; x++)
                        vi1[(y+VIS_B)*vi1_p+(x+VIS_B)] = vinci_thumb[y*VINCI_THUMB_W+x];
                int rx = frame_px[frame] * VINCI_THUMB_W / SRC_SHEET_W;
                int ry = frame_py * VINCI_THUMB_H / SRC_SHEET_H;
                int rw = SRC_PW * VINCI_THUMB_W / SRC_SHEET_W;
                int rh = SRC_PH * VINCI_THUMB_H / SRC_SHEET_H;
                if (rw<2) rw=2; if (rh<2) rh=2;
                for (int x=rx; x<rx+rw && x<VINCI_THUMB_W; x++) {
                    vi1[(ry+VIS_B)*vi1_p+x+VIS_B] = 0xFFFF0000;
                    vi1[(ry+rh-1+VIS_B)*vi1_p+x+VIS_B] = 0xFFFF0000;
                }
                for (int y=ry; y<ry+rh && y<VINCI_THUMB_H; y++) {
                    vi1[(y+VIS_B)*vi1_p+rx+VIS_B] = 0xFFFF0000;
                    vi1[(y+VIS_B)*vi1_p+rx+rw-1+VIS_B] = 0xFFFF0000;
                }
                dcache_clean_range(SPR_ADDR, vi1_p*(VINCI_THUMB_H+VIS_B*2)*4);
                pf = frame;
            }
        }

        memset((void*)ovl, 0, LCD_FB_BYTES);
        nes_render((uint32_t*)fb, (uint32_t*)ovl, LCD_W, LCD_H,
                   &bg, frame_chr, oam, num_oam, 1);

        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        video_wait_vblank();
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf = !buf;
    }
    return 0;
}
