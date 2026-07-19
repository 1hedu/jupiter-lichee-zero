/*
 * GBC PPU + CedarVE Demo — Pokemon Crystal (Celebi #251)
 *
 * Full cedar pipeline in the sprite render path (modeled on cedar_genesis):
 *   1. Construct an ARGB sprite atlas from the source CHR + GB palette
 *      (12 frames × 40×54 px laid out horizontally → 480×54, padded to
 *      480×64 for H.264 macroblock alignment).
 *   2. CedarVE H.264 round-trip on the atlas: ARGB → NV12 (sw)
 *      → cedar_h264_encode (hw) → cedar_h264_decode (hw)
 *      → cedar_nv12_to_argb (sw). On real silicon this is clean; in
 *      QEMU there will be visible H.264 artifacts on the sprite.
 *   3. Cut decoded ARGB per-frame back to GB 2bpp CHR tiles via
 *      nearest-palette match against the 4-color GB sprite palette.
 *   4. Render via the authentic GB PPU pipeline at 160×144.
 *
 * VI1 shows the raw (non-cedar) thumbnail sheet with the red frame
 * indicator as a pristine reference.
 *
 * Build: make GAME=examples/cedar_gb/main.c
 */
#include "jupiter.h"
#include "gb.h"
#include "sheet.h"
#include "pmu.h"
#include <string.h>

#include "celebi_chr.h"
#include "celebi_thumb.h"

#define META_TW  CELEBI_TW   /* 5 */
#define META_TH  CELEBI_TH   /* 7 */
#define META_TPF (META_TW * META_TH)  /* 35 — fits in 40 OAM */

/* Atlas: 12 frames × 40 px wide = 480, 54 tall, padded to 64 for
 * 16-row alignment. */
#define ATLAS_W       480
#define ATLAS_H       64
#define ATLAS_ADDR    0x43400000
static uint32_t * const atlas = (uint32_t *)ATLAS_ADDR;

/* Per-frame CHR (cedar-derived) */
static uint8_t cedar_chr[CELEBI_FRAMES * META_TPF * 16];

static const int src_fx[CELEBI_FRAMES] = {
    2019,2076,2133,2190,2247,2304,2361,2418,2475,2532,2589,2646
};
static const int src_fy = 3163;
#define SRC_SHEET_W 2809
#define SRC_SHEET_H 3276

/* ----- Build ARGB atlas from source celebi_chr + celebi_pal ------- */
static void build_atlas(void)
{
    /* Background: deep purple to make cedar artifacts on transparent
     * regions obvious vs the GB grass backdrop. */
    for (uint32_t i = 0; i < ATLAS_W * ATLAS_H; i++)
        atlas[i] = 0xFF200030;

    for (int f = 0; f < CELEBI_FRAMES; f++) {
        int frame_x0 = f * CELEBI_PW;
        const uint8_t *fchr = &celebi_chr[f * META_TPF * 16];
        for (int ty = 0; ty < META_TH; ty++) {
            for (int tx = 0; tx < META_TW; tx++) {
                const uint8_t *tile = &fchr[(ty * META_TW + tx) * 16];
                for (int r = 0; r < 8; r++) {
                    for (int c = 0; c < 8; c++) {
                        uint8_t bp0 = (tile[r * 2]     >> (7 - c)) & 1;
                        uint8_t bp1 = (tile[r * 2 + 1] >> (7 - c)) & 1;
                        int ci = (bp1 << 1) | bp0;
                        if (ci == 0) continue;
                        int px = frame_x0 + tx * 8 + c;
                        int py = ty * 8 + r;
                        if (py < ATLAS_H && px < ATLAS_W)
                            atlas[py * ATLAS_W + px] = celebi_pal[ci] | 0xFF000000;
                    }
                }
            }
        }
    }
}

/* ----- Cut decoded atlas → GB 2bpp tiles per frame (lib/sheet.c) --
 * bg_thresh = 0: every atlas pixel goes through the pure nearest-
 * palette match against the 4-color GB palette, exactly like the old
 * local nearest_pal_idx() path. */
static void cut_atlas_to_tiles(void)
{
    sheet_t sh;
    sheet_init(&sh, atlas, ATLAS_W, ATLAS_H);
    sh.bg_thresh = 0;
    for (int f = 0; f < CELEBI_FRAMES; f++)
        sheet_cut(&sh, f * CELEBI_PW, 0, CELEBI_PW, CELEBI_PH, 1,
                  SHEET_FMT_GB_2BPP, 0, celebi_pal, 0, 4,
                  &cedar_chr[f * META_TPF * 16], META_TPF);
}

/* GB state */
static uint8_t bg_chr[256 * 16];
static uint8_t bg_map[GB_MAP_SIZE];
static uint8_t bg_attr[GB_MAP_SIZE];   /* GBC per-tile palette select */
static uint32_t bg_palette[32];
static uint32_t spr_palette[32];
static gb_oam_entry_t oam[GB_MAX_SPRITES];

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    cedar_init();

    uart_puts("\n========================================\n");
    uart_puts("  Celebi — GBC PPU + CedarVE H.264\n");
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
    uart_puts("[main] cut to "); uart_putdec(CELEBI_FRAMES);
    uart_puts(" frames × "); uart_putdec(META_TPF); uart_puts(" tiles\n");

    /* BG: CGB afternoon pastoral — banded sky with chunky dither, a low sun,
     * pine treeline silhouette, and a quiet grass field for the walk
     * path.  Tiles are ASCII art, '0' = lightest shade .. '3' = darkest. */
    enum {
        T_L0, T_L1, T_L2, T_L3,          /* solid shades          */
        T_D32, T_D21, T_D10,             /* 4x4-block sky dither  */
        T_SUN_00, T_SUN_01, T_SUN_02,    /* 3x3-tile low sun      */
        T_SUN_10, T_SUN_11, T_SUN_12,
        T_SUN_20, T_SUN_21, T_SUN_22,
        T_BIRD,                          /* distant birds         */
        T_TREE_A, T_TREE_B,              /* treeline silhouette   */
        T_TRANS,                         /* hill -> grass steps   */
        T_GRASS,                         /* grass tuft accent     */
        T_FRINGE,                        /* field -> ground edge  */
        T_GROUND, T_FLOWER, T_TALL,      /* dark foreground band  */
    };
    static const char bg_art[][8][9] = {
        [T_L0]  = {"00000000","00000000","00000000","00000000",
                   "00000000","00000000","00000000","00000000"},
        [T_L1]  = {"11111111","11111111","11111111","11111111",
                   "11111111","11111111","11111111","11111111"},
        [T_L2]  = {"22222222","22222222","22222222","22222222",
                   "22222222","22222222","22222222","22222222"},
        [T_L3]  = {"33333333","33333333","33333333","33333333",
                   "33333333","33333333","33333333","33333333"},
        [T_D32] = {"33332222","33332222","33332222","33332222",
                   "22223333","22223333","22223333","22223333"},
        [T_D21] = {"22221111","22221111","22221111","22221111",
                   "11112222","11112222","11112222","11112222"},
        [T_D10] = {"11110000","11110000","11110000","11110000",
                   "00001111","00001111","00001111","00001111"},
        [T_SUN_00]={"11111111","11111111","11111112","11111122",
                   "11111220","11112200","11122000","11220000"},
        [T_SUN_01]={"11111111","11122111","22222222","20000002",
                   "00000000","00000000","00000000","00000000"},
        [T_SUN_02]={"11111111","11111111","21111111","22111111",
                   "02211111","00221111","00022111","00002211"},
        [T_SUN_10]={"11220000","11200000","11200000","12200000",
                   "12200000","11200000","11200000","11220000"},
        [T_SUN_11]={"00000000","00000000","00000000","00000000",
                   "00000000","00000000","00000000","00000000"},
        [T_SUN_12]={"00002211","00000211","00000211","00000221",
                   "00000221","00000211","00000211","00002211"},
        [T_SUN_20]={"11220000","11122000","11112200","11111220",
                   "11111122","11111112","11111111","11111111"},
        [T_SUN_21]={"00000000","00000000","00000000","00000000",
                   "20000002","22222222","11122111","11111111"},
        [T_SUN_22]={"00002211","00022111","00221111","02211111",
                   "22111111","21111111","11111111","11111111"},
        [T_BIRD] = {"11111111","13131111","11311111","11111111",
                   "11111313","11111131","11111111","11111111"},
        [T_TREE_A]={"00030000","00333000","00333000","03333300",
                   "03333300","33333333","33333333","33333333"},
        [T_TREE_B]={"00000300","00003330","00003330","30033333",
                   "33333333","33333333","33333333","33333333"},
        [T_TRANS]= {"33333333","33333333","33223322","22332233",
                   "22222222","22112211","11221122","11111111"},
        [T_GRASS]= {"11111111","11111111","11211121","12121212",
                   "11111111","11111111","11111111","11111111"},
        [T_FRINGE]={"11111111","11111111","21122112","22222222",
                   "22222222","22222222","22222222","22222222"},
        [T_GROUND]={"22222222","22232222","22222222","22222223",
                   "22222222","23222222","22222222","22222322"},
        [T_FLOWER]={"22222222","22200222","22000022","22000022",
                   "22200222","22222222","22222222","22222222"},
        [T_TALL] = {"22222222","23232322","23232322","23232322",
                   "22222222","22222222","22222222","22222222"},
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

    memset(bg_map, 0, sizeof(bg_map));
    for (int x = 0; x < GB_MAP_W; x++) {
        bg_map[0*GB_MAP_W+x] = T_D32;             /* dark dusk zenith   */
        bg_map[1*GB_MAP_W+x] = T_L2;
        bg_map[2*GB_MAP_W+x] = T_D21;             /* dither step        */
        bg_map[3*GB_MAP_W+x] = T_L1;
        bg_map[4*GB_MAP_W+x] = T_L1;
        bg_map[5*GB_MAP_W+x] = T_D10;             /* horizon glow       */
        bg_map[6*GB_MAP_W+x] = (x & 1) ? T_TREE_B : T_TREE_A;
        bg_map[7*GB_MAP_W+x] = T_L3;              /* treeline mass      */
        bg_map[8*GB_MAP_W+x] = T_TRANS;
        for (int y = 9; y < 15; y++)              /* calm walk field    */
            bg_map[y*GB_MAP_W+x] =
                (((x*7 + y*13) & 15) == 1) ? T_GRASS : T_L1;
        bg_map[15*GB_MAP_W+x] = T_FRINGE;
        for (int y = 16; y < GB_MAP_H; y++) {     /* dark foreground    */
            int h = (x*5 + y*11) & 15;
            bg_map[y*GB_MAP_W+x] = (h == 2) ? T_FLOWER
                                 : (h == 9) ? T_TALL : T_GROUND;
        }
    }
    /* low sun, 3x3 tiles, resting on the treeline left of center */
    for (int ty = 0; ty < 3; ty++)
        for (int tx = 0; tx < 3; tx++)
            bg_map[(3+ty)*GB_MAP_W + 4+tx] = T_SUN_00 + ty*3 + tx;
    /* a few distant birds */
    bg_map[3*GB_MAP_W+13] = T_BIRD;
    bg_map[4*GB_MAP_W+10] = T_BIRD;

    /* GBC palette assignment — Celebi is a Game Boy COLOR sheet, so
     * the scene uses real CGB colors: per-tile palette attributes pick
     * one of five 4-color palettes (tile art shades 0=light..3=dark). */
    for (int x = 0; x < GB_MAP_W; x++) {
        for (int y = 0; y < 6; y++)  bg_attr[y*GB_MAP_W+x] = 0;  /* sky   */
        bg_attr[6*GB_MAP_W+x] = 2;                               /* trees */
        bg_attr[7*GB_MAP_W+x] = 2;
        for (int y = 8; y < 16; y++) bg_attr[y*GB_MAP_W+x] = 3;  /* field */
        for (int y = 16; y < GB_MAP_H; y++)
            bg_attr[y*GB_MAP_W+x] = 4;                           /* fore  */
    }
    for (int ty = 0; ty < 3; ty++)                               /* sun   */
        for (int tx = 0; tx < 3; tx++)
            bg_attr[(3+ty)*GB_MAP_W + 4+tx] = 1;

    /* pal0 sky: afternoon blues, deep zenith -> pale horizon */
    bg_palette[0]  = 0xFF8AC8F0; bg_palette[1]  = 0xFF5EA8E0;
    bg_palette[2]  = 0xFF3678C0; bg_palette[3]  = 0xFF1E4E94;
    /* pal1 sun: pale gold disc, warm halo */
    bg_palette[4]  = 0xFFFFF6C8; bg_palette[5]  = 0xFFFFD24E;
    bg_palette[6]  = 0xFFF0A030; bg_palette[7]  = 0xFFC87018;
    /* pal2 treeline: sky behind, forest green silhouette */
    bg_palette[8]  = 0xFF8AC8F0; bg_palette[9]  = 0xFF4E9A50;
    bg_palette[10] = 0xFF2E7038; bg_palette[11] = 0xFF16421E;
    /* pal3 walk field: lush greens */
    bg_palette[12] = 0xFFB0E080; bg_palette[13] = 0xFF88C860;
    bg_palette[14] = 0xFF58A048; bg_palette[15] = 0xFF2E6E30;
    /* pal4 foreground: pink blooms over deep grass */
    bg_palette[16] = 0xFFF26A8A; bg_palette[17] = 0xFF88C860;
    bg_palette[18] = 0xFF3E7C36; bg_palette[19] = 0xFF234E24;

    /* (legacy DMG ramp kept in pal7 for reference/experiments) */
    bg_palette[28] = 0xFF9BBC0F;
    bg_palette[29] = 0xFF8BAC0F;
    bg_palette[30] = 0xFF306230;
    bg_palette[31] = 0xFF0F380F;

    spr_palette[0] = 0x00000000;
    spr_palette[1] = celebi_pal[1];
    spr_palette[2] = celebi_pal[2];
    spr_palette[3] = celebi_pal[3];

    video_init();
    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i=0; i<LCD_W*LCD_H; i++) { fb0[i]=0xFF000000; fb1[i]=0xFF000000; }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    gb_bg_t bg = {
        .chr=bg_chr, .map=bg_map, .map_attr=bg_attr,
        .palette=bg_palette, .scroll_x=0, .scroll_y=0, .enabled=1,
    };

    #define VIS_B 2
    video_vi1_init(LCD_W-CELEBI_THUMB_W-VIS_B*2-4, 4,
                   CELEBI_THUMB_W+VIS_B*2, CELEBI_THUMB_H+VIS_B*2);
    volatile uint32_t *vi1 = (volatile uint32_t *)SPR_ADDR;
    int vi1_p = CELEBI_THUMB_W + VIS_B*2;
    for (int y=0; y<CELEBI_THUMB_H+VIS_B*2; y++)
        for (int x=0; x<vi1_p; x++)
            vi1[y*vi1_p+x] = (x<VIS_B||x>=CELEBI_THUMB_W+VIS_B||
                               y<VIS_B||y>=CELEBI_THUMB_H+VIS_B)
                              ? 0xFF00FF00 : celebi_thumb[((y-VIS_B)*CELEBI_THUMB_W)+(x-VIS_B)];
    dcache_clean_range(SPR_ADDR, vi1_p*(CELEBI_THUMB_H+VIS_B*2)*4);

    uart_puts("[main] go!\n");

    int buf=0, frame=0, spr_x=40, spr_dx=1, flip=0;
    uint32_t last_t = timer_read();

    while (1) {
        volatile uint32_t *fb = buf?fb1:fb0;
        volatile uint32_t *ovl = (volatile uint32_t*)(buf?OVL1_ADDR:OVL_ADDR);
        uint32_t fb_addr = buf?FB1_ADDR:FB0_ADDR;
        uint32_t ovl_addr = buf?OVL1_ADDR:OVL_ADDR;

        uint32_t now = timer_read();
        if (ticks_to_ms(timer_elapsed(last_t, now)) > 100) {
            frame = (frame+1) % CELEBI_FRAMES;
            last_t = now;
        }

        spr_x += spr_dx;
        if (spr_x > GB_NATIVE_W - CELEBI_PW - 4) { spr_dx=-1; flip=1; }
        if (spr_x < 4) { spr_dx=1; flip=0; }
        int spr_y = 15*8 - CELEBI_PH;

        int num_oam = 0;
        const uint8_t *frame_chr = &cedar_chr[frame * META_TPF * 16];
        for (int ty=0; ty<META_TH && num_oam<GB_MAX_SPRITES; ty++)
            for (int tx=0; tx<META_TW && num_oam<GB_MAX_SPRITES; tx++) {
                int ox = flip ? (META_TW-1-tx)*8 : tx*8;
                oam[num_oam++] = (gb_oam_entry_t){
                    .y = (uint8_t)(spr_y + ty*8 + 16),
                    .x = (uint8_t)(spr_x + ox + 8),
                    .tile = (uint8_t)(ty*META_TW+tx),
                    .attr = GB_SPR_PAL(0) | (flip?GB_SPR_HFLIP:0),
                };
            }

        {
            static int pf = -1;
            if (frame != pf) {
                for (int y=0;y<CELEBI_THUMB_H;y++)
                    for (int x=0;x<CELEBI_THUMB_W;x++)
                        vi1[(y+VIS_B)*vi1_p+(x+VIS_B)] = celebi_thumb[y*CELEBI_THUMB_W+x];
                int rx = src_fx[frame] * CELEBI_THUMB_W / SRC_SHEET_W;
                int ry = src_fy * CELEBI_THUMB_H / SRC_SHEET_H;
                int rw = CELEBI_PW * CELEBI_THUMB_W / SRC_SHEET_W;
                int rh = CELEBI_PH * CELEBI_THUMB_H / SRC_SHEET_H;
                if (rw<2) rw=2; if (rh<2) rh=2;
                for (int x=rx;x<rx+rw&&x<CELEBI_THUMB_W;x++) {
                    vi1[(ry+VIS_B)*vi1_p+x+VIS_B]=0xFFFF0000;
                    vi1[(ry+rh-1+VIS_B)*vi1_p+x+VIS_B]=0xFFFF0000;
                }
                for (int y=ry;y<ry+rh&&y<CELEBI_THUMB_H;y++) {
                    vi1[(y+VIS_B)*vi1_p+rx+VIS_B]=0xFFFF0000;
                    vi1[(y+VIS_B)*vi1_p+rx+rw-1+VIS_B]=0xFFFF0000;
                }
                dcache_clean_range(SPR_ADDR, vi1_p*(CELEBI_THUMB_H+VIS_B*2)*4);
                pf = frame;
            }
        }

        memset((void*)ovl, 0, LCD_FB_BYTES);
        gb_render((uint32_t*)fb, (uint32_t*)ovl, LCD_W, LCD_H,
                  &bg, frame_chr, spr_palette, oam, num_oam, 1);

        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        video_wait_vblank();
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf = !buf;
    }
    return 0;
}
