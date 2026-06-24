/*
 * NES PPU Sprite Demo — Mendel Palace (Vinci)
 *
 * Pre-converted 2bpp CHR tiles + NES palette, rendered through the
 * authentic NES PPU pipeline at 256×224 with vblank sync.
 * VI1 shows the full sprite sheet with red frame indicator.
 *
 * Build: make GAME=examples/nes_ppu/main.c
 */
#include "jupiter.h"
#include "nes.h"
#include "pmu.h"
#include <string.h>

#include "vinci_chr.h"
#include "vinci_thumb.h"

/* Source frame layout */
#define ANIM_FRAMES   8
#define SRC_TW        2    /* source tiles wide */
#define SRC_TH        3    /* source tiles tall */
#define SRC_PW        16
#define SRC_PH        24
#define SRC_TILES     (SRC_TW * SRC_TH)

/* Scaled metasprite: 3x = 48x72px = 6x9 tiles */
#define SCALE         3
#define META_PW       (SRC_PW * SCALE)   /* 48 */
#define META_PH       (SRC_PH * SCALE)   /* 72 */
#define META_TW       (META_PW / 8)      /* 6 */
#define META_TH       ((META_PH + 7) / 8) /* 9 */
#define META_TILES    (META_TW * META_TH) /* 54 */

/* Scaled CHR storage: 8 frames × 54 tiles × 16 bytes */
static uint8_t meta_chr[ANIM_FRAMES * META_TILES * 16];

/* Source frame positions in the original sheet (for VI1 red rect) */
static const int frame_px[ANIM_FRAMES] = { 16, 64, 112, 160, 208, 256, 304, 352 };
static const int frame_py = 9;
#define SRC_SHEET_W 384
#define SRC_SHEET_H 960

/* ================================================================== */
/* Step 1+2: Render CHR→ARGB, scale 3x, convert back to 2bpp CHR       */
/* ================================================================== */
static void process_meta_frame(int f)
{
    /* Step 1: Render source CHR tiles to small ARGB buffer, scale 3x */
    static uint8_t scaled_ci[META_PW * META_PH]; /* color indices 0-3 */
    int base = f * SRC_TILES;

    for (int y = 0; y < META_PH; y++) {
        int sy = y / SCALE;
        int src_ty = sy / 8;
        int src_fy = sy & 7;
        for (int x = 0; x < META_PW; x++) {
            int sx = x / SCALE;
            int src_tx = sx / 8;
            int src_fx = sx & 7;

            if (src_ty >= SRC_TH || src_tx >= SRC_TW) {
                scaled_ci[y * META_PW + x] = 0;
                continue;
            }

            const uint8_t *tile = &vinci_spr_chr[(base + src_ty * SRC_TW + src_tx) * 16];
            uint8_t bp0 = (tile[src_fy] >> (7 - src_fx)) & 1;
            uint8_t bp1 = (tile[src_fy + 8] >> (7 - src_fx)) & 1;
            scaled_ci[y * META_PW + x] = (bp1 << 1) | bp0;
        }
    }

    /* Step 2: Pack scaled color indices back into NES 2bpp CHR tiles */
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
                                 ? scaled_ci[py * META_PW + px] : 0;
                    bp0 |= ((ci & 1) << (7 - col));
                    bp1 |= (((ci >> 1) & 1) << (7 - col));
                }
                tile[row] = bp0;
                tile[row + 8] = bp1;
            }
        }
    }
}

/* NES state */
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

    uart_puts("\n========================================\n");
    uart_puts("  Vinci — NES PPU 3x Metasprite\n");
    uart_puts("========================================\n\n");

    /* Pre-process: scale all frames 3x and re-tile */
    for (int f = 0; f < ANIM_FRAMES; f++)
        process_meta_frame(f);
    uart_puts("[main] "); uart_putdec(ANIM_FRAMES); uart_puts(" frames × ");
    uart_putdec(META_TILES); uart_puts(" tiles = ");
    uart_putdec(ANIM_FRAMES * META_TILES * 16); uart_puts("B CHR\n");

    /* ---- Palette RAM ---- */
    memset(palette_ram, 0x0D, sizeof(palette_ram));
    palette_ram[0] = vinci_bg_color;
    palette_ram[1] = 0x02; palette_ram[2] = 0x12; palette_ram[3] = 0x21;
    palette_ram[5] = 0x17; palette_ram[6] = 0x27; palette_ram[7] = 0x37;
    palette_ram[17] = vinci_spr_palette[1];
    palette_ram[18] = vinci_spr_palette[2];
    palette_ram[19] = vinci_spr_palette[3];

    /* ---- BG CHR ---- */
    memset(bg_chr, 0, sizeof(bg_chr));
    for (int r = 0; r < 8; r++) {
        bg_chr[1*16+r] = 0xFF; bg_chr[1*16+r+8] = 0x00;
    }
    for (int r = 0; r < 8; r++) {
        if (r < 1) { bg_chr[2*16+r]=0xFF; bg_chr[2*16+r+8]=0xFF; }
        else if (r < 3) { bg_chr[2*16+r]=(r&1)?0xAA:0x55; bg_chr[2*16+r+8]=0xFF; }
        else { bg_chr[2*16+r]=0xFF; bg_chr[2*16+r+8]=0x00; }
    }
    for (int r = 0; r < 8; r++) {
        if (r==0||r==4) { bg_chr[3*16+r]=0x00; bg_chr[3*16+r+8]=0xFF; }
        else if (r<4) { bg_chr[3*16+r]=0xEF; bg_chr[3*16+r+8]=0x00; }
        else { bg_chr[3*16+r]=0xFE; bg_chr[3*16+r+8]=0x00; }
    }

    /* ---- Nametable ---- */
    memset(nametable, 0, sizeof(nametable));
    for (int x = 0; x < NES_NT_W; x++) {
        nametable[25*NES_NT_W+x] = 2;
        for (int y = 26; y < NES_NT_H; y++) nametable[y*NES_NT_W+x] = 3;
    }
    memset(attribute, 0, sizeof(attribute));
    for (int ax = 0; ax < 8; ax++) {
        attribute[6*8+ax] = NES_ATTR(0,0,1,1);
        attribute[7*8+ax] = NES_ATTR(1,1,1,1);
    }

    /* ---- Display ---- */
    video_init();
    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i = 0; i < LCD_W*LCD_H; i++) { fb0[i]=0xFF000000; fb1[i]=0xFF000000; }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    /* ---- VI1: full sprite sheet thumbnail + red rect ---- */
    #define VIS_B 2
    video_vi1_init(LCD_W - VINCI_THUMB_W - VIS_B*2 - 4, 4,
                   VINCI_THUMB_W + VIS_B*2, VINCI_THUMB_H + VIS_B*2);
    volatile uint32_t *vi1 = (volatile uint32_t *)SPR_ADDR;
    int vi1_p = VINCI_THUMB_W + VIS_B*2;

    /* Draw border + sheet once */
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

        /* OAM: META_TW × META_TH grid (6×9 = 54 sprites)
         * Tile indices start at 0 per frame — we pass per-frame CHR pointer */
        int num_oam = 0;
        for (int ty = 0; ty < META_TH && num_oam < NES_MAX_SPRITES; ty++)
            for (int tx = 0; tx < META_TW && num_oam < NES_MAX_SPRITES; tx++) {
                int ox = flip ? (META_TW-1-tx)*8 : tx*8;
                oam[num_oam++] = (nes_oam_entry_t){
                    .y = (uint8_t)(spr_y + ty*8 - 1),
                    .tile = (uint8_t)(ty*META_TW + tx),  /* 0-53 per frame */
                    .attr = NES_SPR_PAL(0) | (flip ? NES_SPR_HFLIP : 0),
                    .x = (uint8_t)(spr_x + ox),
                };
            }
        /* Per-frame CHR pointer */
        const uint8_t *frame_chr = &meta_chr[frame * META_TILES * 16];

        /* VI1 red rect update on frame change */
        {
            static int pf = -1;
            if (frame != pf) {
                /* Restore sheet */
                for (int y=0; y<VINCI_THUMB_H; y++)
                    for (int x=0; x<VINCI_THUMB_W; x++)
                        vi1[(y+VIS_B)*vi1_p+(x+VIS_B)] = vinci_thumb[y*VINCI_THUMB_W+x];
                /* Red rect */
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
