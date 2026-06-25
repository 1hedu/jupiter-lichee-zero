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

/* ----- Nearest-palette match (decoded ARGB → 0-3 color index) ----- */
static int nearest_pal_idx(uint32_t c)
{
    int best = 0, bd = 999999;
    int cr = (c >> 16) & 0xFF, cg = (c >> 8) & 0xFF, cb = c & 0xFF;
    for (int i = 0; i < 4; i++) {
        uint32_t p = celebi_pal[i];
        int dr = cr - (int)((p>>16)&0xFF);
        int dg = cg - (int)((p>>8)&0xFF);
        int db = cb - (int)(p&0xFF);
        int d = dr*dr + dg*dg + db*db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* ----- Cut decoded atlas → GB 2bpp tiles per frame ---------------- */
static void cut_atlas_to_tiles(void)
{
    for (int f = 0; f < CELEBI_FRAMES; f++) {
        int frame_x0 = f * CELEBI_PW;
        static uint8_t ci_grid[CELEBI_PW * CELEBI_PH];

        for (int y = 0; y < CELEBI_PH; y++)
            for (int x = 0; x < CELEBI_PW; x++)
                ci_grid[y * CELEBI_PW + x] =
                    nearest_pal_idx(atlas[y * ATLAS_W + (frame_x0 + x)]);

        uint8_t *out = &cedar_chr[f * META_TPF * 16];
        for (int ty = 0; ty < META_TH; ty++) {
            for (int tx = 0; tx < META_TW; tx++) {
                uint8_t *tile = out + (ty * META_TW + tx) * 16;
                for (int r = 0; r < 8; r++) {
                    uint8_t bp0 = 0, bp1 = 0;
                    for (int c = 0; c < 8; c++) {
                        int px = tx * 8 + c;
                        int py = ty * 8 + r;
                        uint8_t ci = (px < CELEBI_PW && py < CELEBI_PH)
                                     ? ci_grid[py * CELEBI_PW + px] : 0;
                        bp0 |= ((ci & 1) << (7 - c));
                        bp1 |= (((ci >> 1) & 1) << (7 - c));
                    }
                    tile[r * 2]     = bp0;
                    tile[r * 2 + 1] = bp1;
                }
            }
        }
    }
}

/* GB state */
static uint8_t bg_chr[256 * 16];
static uint8_t bg_map[GB_MAP_SIZE];
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
    int enc_sz = cedar_h264_encode(ATLAS_W, ATLAS_H, 10, NULL, 0);
    int cedar_ok = 0;
    if (enc_sz > 0) {
        uart_puts("[main] encoded: "); uart_putdec(enc_sz); uart_puts("B\n");
        int rc = cedar_h264_decode((const uint8_t *)0x43700000, enc_sz,
                                   ATLAS_W, ATLAS_H, 36, 10, 0, 0, 1);
        if (rc == 0) {
            uint32_t dsz = ATLAS_W * ATLAS_H;
            dcache_invalidate_range(0x43100000, dsz);
            dcache_invalidate_range(0x43200000, dsz / 2);
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

    /* BG: grass + dirt */
    memset(bg_chr, 0, sizeof(bg_chr));
    for (int r=0;r<8;r++) {
        bg_chr[1*16+r]   = (r<3) ? 0xAA : 0xFF;
        bg_chr[1*16+r+8] = (r<3) ? 0x55 : 0x00;
    }
    for (int r=0;r<8;r++) { bg_chr[2*16+r]=0xFF; bg_chr[2*16+r+8]=0x00; }
    memset(bg_map, 0, sizeof(bg_map));
    for (int x=0; x<GB_MAP_W; x++) {
        bg_map[15*GB_MAP_W+x] = 1;
        for (int y=16; y<GB_MAP_H; y++) bg_map[y*GB_MAP_W+x] = 2;
    }
    bg_palette[0] = 0xFF88CC88;
    bg_palette[1] = 0xFF306830;
    bg_palette[2] = 0xFF58A858;
    bg_palette[3] = 0xFF98D898;

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
        .chr=bg_chr, .map=bg_map, .map_attr=NULL,
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
