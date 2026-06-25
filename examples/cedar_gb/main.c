/*
 * GBC PPU Demo — Pokemon Crystal (Celebi #251)
 *
 * 12-frame animation, pre-converted 2bpp CHR, 3x metasprite,
 * GB PPU rendering at 160×144 authentic, vblank synced,
 * full sprite sheet on VI1.
 *
 * Build: make GAME=examples/cedar_gb/main.c
 */
#include "jupiter.h"
#include "gb.h"
#include "pmu.h"
#include <string.h>

#include "celebi_chr.h"
#include "celebi_thumb.h"

/* Source frame positions in original sheet (for VI1 red rect) */
static const int src_fx[CELEBI_FRAMES] = {
    2019,2076,2133,2190,2247,2304,2361,2418,2475,2532,2589,2646
};
static const int src_fy = 3163;
#define SRC_SHEET_W 2809
#define SRC_SHEET_H 3276

/* Native-size metasprite (no scaling — GB only has 40 sprites) */
#define META_TW  CELEBI_TW   /* 5 */
#define META_TH  CELEBI_TH   /* 7 */
#define META_TPF (META_TW * META_TH)  /* 35 — fits in 40 OAM */

/* BG tile data */
static uint8_t bg_chr[256 * 16];
static uint8_t bg_map[GB_MAP_SIZE];
static uint32_t bg_palette[32];  /* 8 palettes × 4 */
static uint32_t spr_palette[32]; /* 8 palettes × 4 */
static gb_oam_entry_t oam[GB_MAX_SPRITES];

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();

    uart_puts("\n========================================\n");
    uart_puts("  Celebi — GBC PPU 3x Metasprite\n");
    uart_puts("========================================\n\n");

    uart_puts("[main] "); uart_putdec(CELEBI_FRAMES);
    uart_puts(" frames × "); uart_putdec(META_TPF);
    uart_puts(" tiles (native size, 40 OAM max)\n");

    /* BG: grass tile + sky */
    memset(bg_chr, 0, sizeof(bg_chr));
    /* Tile 1: grass */
    for (int r=0;r<8;r++) {
        bg_chr[1*16+r]   = (r<3) ? 0xAA : 0xFF;
        bg_chr[1*16+r+8] = (r<3) ? 0x55 : 0x00;
    }
    /* Tile 2: dirt */
    for (int r=0;r<8;r++) { bg_chr[2*16+r]=0xFF; bg_chr[2*16+r+8]=0x00; }

    memset(bg_map, 0, sizeof(bg_map));
    for (int x=0; x<GB_MAP_W; x++) {
        bg_map[15*GB_MAP_W+x] = 1; /* grass surface */
        for (int y=16; y<GB_MAP_H; y++) bg_map[y*GB_MAP_W+x] = 2; /* dirt */
    }

    /* BG palette 0: greens */
    bg_palette[0] = 0xFF88CC88; /* light green sky */
    bg_palette[1] = 0xFF306830; /* dark green */
    bg_palette[2] = 0xFF58A858; /* medium green */
    bg_palette[3] = 0xFF98D898; /* light green */

    /* Sprite palette 0: Celebi colors */
    spr_palette[0] = 0x00000000; /* transparent */
    spr_palette[1] = celebi_pal[1];
    spr_palette[2] = celebi_pal[2];
    spr_palette[3] = celebi_pal[3];

    /* Display */
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

    /* VI1: full sprite sheet thumbnail */
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
        int spr_y = 15*8 - CELEBI_PH; /* feet on grass */

        /* Build OAM metasprite — tile indices 0..META_TPF per frame */
        int num_oam = 0;
        const uint8_t *frame_chr = &celebi_chr[frame * META_TPF * 16];
        for (int ty=0; ty<META_TH && num_oam<GB_MAX_SPRITES; ty++)
            for (int tx=0; tx<META_TW && num_oam<GB_MAX_SPRITES; tx++) {
                int ox = flip ? (META_TW-1-tx)*8 : tx*8;
                oam[num_oam++] = (gb_oam_entry_t){
                    .y = (uint8_t)(spr_y + ty*8 + 16), /* GB Y offset +16 */
                    .x = (uint8_t)(spr_x + ox + 8),    /* GB X offset +8 */
                    .tile = (uint8_t)(ty*META_TW+tx),
                    .attr = GB_SPR_PAL(0) | (flip?GB_SPR_HFLIP:0),
                };
            }

        /* VI1 update on frame change */
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
