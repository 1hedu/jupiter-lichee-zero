/*
 * Tri-Layer Demo — Four retro systems on three DE2 hardware layers
 *
 *   VI0 (opaque):  GB (left) + NES (right), side by side
 *   UI0 (alpha):   Genesis Plane A + sprites (floating overlay)
 *   VI1 (PIP):     SNES scene
 *
 * All sprites at native size. GB/NES/SNES use pre-converted CHR headers.
 * Genesis converts from embedded raw ARGB at boot (runtime pipeline).
 *
 * Build: make GAME=examples/trilayer/main.c
 */
#include "jupiter.h"
#include "nes.h"
#include "gb.h"
#include "genesis.h"
#include "snes.h"
#include "pmu.h"
#include <string.h>

/* Pre-converted tile data */
#include "../../examples/cedar_nes/vinci_chr.h"
#include "../../examples/cedar_gb/celebi_chr.h"
#include "../../examples/cedar_snes/soldier_chr.h"

/* Genesis: raw ARGB sprite sheet embedded via objcopy */
extern const uint8_t _binary_tools_pulseman_argb_start[];
extern const uint8_t _binary_tools_pulseman_argb_end[];

/* ================================================================== */
/* Layout constants                                                     */
/* ================================================================== */
#define GB_X    12
#define GB_Y    64   /* (272-144)/2 */
#define NES_X   188  /* 12 + 160 + 16 */
#define NES_Y   24   /* (272-224)/2 */
#define SNES_VW 160  /* VI1 viewport */
#define SNES_VH 144
#define SNES_X  312  /* bottom-right */
#define SNES_Y  120

/* ================================================================== */
/* Compositing temp buffers                                             */
/* ================================================================== */
static uint32_t gb_fb[GB_NATIVE_W * GB_NATIVE_H];
static uint32_t gb_ovl[GB_NATIVE_W * GB_NATIVE_H];
static uint32_t nes_fb[NES_NATIVE_W * NES_NATIVE_H];
static uint32_t nes_ovl[NES_NATIVE_W * NES_NATIVE_H];
static uint32_t snes_comp[SNES_VW * SNES_VH];
static uint32_t snes_tmp[SNES_VW * SNES_VH];

/* Composite overlay onto bg buffer (where overlay has alpha) */
static void composite(uint32_t *bg, const uint32_t *ovl, int count)
{
    for (int i = 0; i < count; i++)
        if (ovl[i] & 0xFF000000) bg[i] = ovl[i];
}

/* Blit rect from src (pitch=sw) to dst (pitch=LCD_W) at (dx,dy) */
static void blit(volatile uint32_t *dst, const uint32_t *src,
                 int sw, int sh, int dx, int dy)
{
    for (int y = 0; y < sh; y++)
        memcpy((void *)&dst[(dy + y) * LCD_W + dx], &src[y * sw], sw * 4);
}

/* ================================================================== */
/* Genesis runtime ARGB→4bpp conversion                                 */
/* ================================================================== */
#define GEN_SHEET_W  512
#define GEN_SHEET_H  480
#define GEN_SRC_PY   71
#define GEN_SRC_TH   7     /* 56px / 8 */
#define GEN_FRAMES   5     /* use frames 3-7 (32px wide) */

static const struct { int x, pw; } gen_src[GEN_FRAMES] = {
    {164,32}, {199,31}, {233,31}, {267,32}, {301,32},
};

/* Per-frame: top sprite 4×4=16 tiles + bottom sprite 4×3=12 tiles = 28 */
#define GEN_TPF  28
static uint8_t gen_tiles[GEN_FRAMES * GEN_TPF * 32];
static uint32_t gen_cram[64];
static uint32_t gen_bg_color;

static int gen_color_near(uint32_t a, uint32_t b)
{
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF)  - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF)          - (int)(b & 0xFF);
    return (dr * dr + dg * dg + db * db) < 40 * 40;
}

static void gen_extract_palette(const uint32_t *sheet)
{
    gen_bg_color = sheet[0] | 0xFF000000;
    memset(gen_cram, 0, sizeof(gen_cram));
    gen_cram[0] = 0x00000000;
    int count = 0;
    for (int f = 0; f < GEN_FRAMES && count < 15; f++) {
        int fx = gen_src[f].x, pw = gen_src[f].pw;
        for (int y = GEN_SRC_PY; y < GEN_SRC_PY + 56 && count < 15; y++)
            for (int x = fx; x < fx + pw && count < 15; x++) {
                uint32_t c = sheet[y * GEN_SHEET_W + x] | 0xFF000000;
                if (gen_color_near(c, gen_bg_color)) continue;
                if ((c & 0x00FFFFFF) < 0x080808) continue;
                int dup = 0;
                for (int i = 0; i <= count; i++)
                    if (gen_color_near(c, gen_cram[i])) { dup = 1; break; }
                if (!dup) gen_cram[++count] = c;
            }
    }
}

static int gen_nearest(uint32_t c, const uint32_t *sheet)
{
    if (gen_color_near(c | 0xFF000000, gen_bg_color)) return 0;
    if ((c & 0x00FFFFFF) < 0x080808) return 0;
    c |= 0xFF000000;
    int best = 0, bd = 999999;
    for (int i = 1; i < 16; i++) {
        if (!gen_cram[i]) continue;
        int dr = (int)((c >> 16) & 0xFF) - (int)((gen_cram[i] >> 16) & 0xFF);
        int dg = (int)((c >> 8) & 0xFF)  - (int)((gen_cram[i] >> 8) & 0xFF);
        int db = (int)(c & 0xFF)          - (int)(gen_cram[i] & 0xFF);
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void gen_convert_tile(uint8_t *out, const uint32_t *sheet,
                              int fx, int tile_px, int tile_py, int pw)
{
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++) {
            int px0 = tile_px + c * 2;
            int px1 = px0 + 1;
            int py  = tile_py + r;
            uint8_t hi = 0, lo = 0;
            if (px0 < pw && py < 56)
                hi = gen_nearest(sheet[(GEN_SRC_PY + py) * GEN_SHEET_W + fx + px0], sheet);
            if (px1 < pw && py < 56)
                lo = gen_nearest(sheet[(GEN_SRC_PY + py) * GEN_SHEET_W + fx + px1], sheet);
            out[r * 4 + c] = (hi << 4) | lo;
        }
}

static void gen_convert_frames(const uint32_t *sheet)
{
    for (int f = 0; f < GEN_FRAMES; f++) {
        int fx = gen_src[f].x;
        int pw = gen_src[f].pw;
        uint8_t *base = &gen_tiles[f * GEN_TPF * 32];

        /* Top sprite: 4×4 tiles (column-major with h=4) */
        for (int tx = 0; tx < 4; tx++)
            for (int ty = 0; ty < 4; ty++)
                gen_convert_tile(&base[(tx * 4 + ty) * 32],
                                 sheet, fx, tx * 8, ty * 8, pw);

        /* Bottom sprite: 4×3 tiles (column-major with h=3) */
        for (int tx = 0; tx < 4; tx++)
            for (int ty = 0; ty < 3; ty++)
                gen_convert_tile(&base[(16 + tx * 3 + ty) * 32],
                                 sheet, fx, tx * 8, (4 + ty) * 8, pw);
    }
}

/* ================================================================== */
/* GB state                                                             */
/* ================================================================== */
static uint8_t gb_chr[256 * 16];
static uint8_t gb_map[GB_MAP_SIZE];
static uint32_t gb_palette[32];
static uint32_t gb_spr_pal[32];
static gb_oam_entry_t gb_oam[GB_MAX_SPRITES];

/* ================================================================== */
/* NES state                                                            */
/* ================================================================== */
static uint8_t nes_chr[256 * 16];
static uint8_t nes_palette_ram[32];
static uint8_t nes_nametable[NES_NT_TILES];
static uint8_t nes_attribute[NES_NT_ATTRS];
static nes_oam_entry_t nes_oam[NES_MAX_SPRITES];

/* ================================================================== */
/* Genesis state                                                        */
/* ================================================================== */
static uint16_t gen_bg_map[64 * 32];
static genesis_sprite_t gen_sprites[2];

/* ================================================================== */
/* SNES state                                                           */
/* ================================================================== */
static uint8_t snes_bg_chr[256 * 32];
static uint16_t snes_bg_map[32 * 32];
static uint32_t snes_bg_pal[128];
static snes_sprite_t snes_sprites[1];

/* ================================================================== */
/* Main                                                                 */
/* ================================================================== */
int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();

    uart_puts("\n========================================\n");
    uart_puts("  Tri-Layer: GB+NES | Genesis | SNES\n");
    uart_puts("========================================\n\n");

    /* ---- Genesis: runtime ARGB→4bpp conversion ---- */
    uint32_t *gen_sheet = (uint32_t *)0x43400000;
    uint32_t gen_sz = (uint32_t)(_binary_tools_pulseman_argb_end -
                                  _binary_tools_pulseman_argb_start);
    memcpy(gen_sheet, _binary_tools_pulseman_argb_start, gen_sz);
    uart_puts("[gen] ARGB: "); uart_putdec(gen_sz / 1024); uart_puts("KB\n");

    gen_extract_palette(gen_sheet);
    gen_convert_frames(gen_sheet);
    uart_puts("[gen] converted "); uart_putdec(GEN_FRAMES);
    uart_puts(" frames ("); uart_putdec(GEN_TPF);
    uart_puts(" tiles each)\n");

    /* ---- GB setup ---- */
    memset(gb_chr, 0, sizeof(gb_chr));
    for (int r = 0; r < 8; r++) {
        gb_chr[1 * 16 + r]     = (r < 3) ? 0xAA : 0xFF;
        gb_chr[1 * 16 + r + 8] = (r < 3) ? 0x55 : 0x00;
    }
    for (int r = 0; r < 8; r++) {
        gb_chr[2 * 16 + r] = 0xFF; gb_chr[2 * 16 + r + 8] = 0x00;
    }
    memset(gb_map, 0, sizeof(gb_map));
    for (int x = 0; x < GB_MAP_W; x++) {
        gb_map[15 * GB_MAP_W + x] = 1;
        for (int y = 16; y < GB_MAP_H; y++) gb_map[y * GB_MAP_W + x] = 2;
    }
    gb_palette[0] = 0xFF88CC88; gb_palette[1] = 0xFF306830;
    gb_palette[2] = 0xFF58A858; gb_palette[3] = 0xFF98D898;
    gb_spr_pal[0] = 0x00000000;
    gb_spr_pal[1] = celebi_pal[1];
    gb_spr_pal[2] = celebi_pal[2];
    gb_spr_pal[3] = celebi_pal[3];

    /* ---- NES setup ---- */
    memset(nes_chr, 0, sizeof(nes_chr));
    for (int r = 0; r < 8; r++) {
        nes_chr[1 * 16 + r] = 0xFF; nes_chr[1 * 16 + r + 8] = 0x00;
    }
    for (int r = 0; r < 8; r++) {
        if (r < 1)      { nes_chr[2*16+r]=0xFF; nes_chr[2*16+r+8]=0xFF; }
        else if (r < 3) { nes_chr[2*16+r]=(r&1)?0xAA:0x55; nes_chr[2*16+r+8]=0xFF; }
        else             { nes_chr[2*16+r]=0xFF; nes_chr[2*16+r+8]=0x00; }
    }
    for (int r = 0; r < 8; r++) {
        if (r==0||r==4) { nes_chr[3*16+r]=0x00; nes_chr[3*16+r+8]=0xFF; }
        else if (r<4)   { nes_chr[3*16+r]=0xEF; nes_chr[3*16+r+8]=0x00; }
        else            { nes_chr[3*16+r]=0xFE; nes_chr[3*16+r+8]=0x00; }
    }
    memset(nes_nametable, 0, sizeof(nes_nametable));
    for (int x = 0; x < NES_NT_W; x++) {
        nes_nametable[25 * NES_NT_W + x] = 2;
        for (int y = 26; y < NES_NT_H; y++) nes_nametable[y * NES_NT_W + x] = 3;
    }
    memset(nes_attribute, 0, sizeof(nes_attribute));
    for (int ax = 0; ax < 8; ax++) {
        nes_attribute[6 * 8 + ax] = NES_ATTR(0, 0, 1, 1);
        nes_attribute[7 * 8 + ax] = NES_ATTR(1, 1, 1, 1);
    }
    memset(nes_palette_ram, 0x0D, sizeof(nes_palette_ram));
    nes_palette_ram[0] = vinci_bg_color;
    nes_palette_ram[1] = 0x02; nes_palette_ram[2] = 0x12; nes_palette_ram[3] = 0x21;
    nes_palette_ram[5] = 0x17; nes_palette_ram[6] = 0x27; nes_palette_ram[7] = 0x37;
    nes_palette_ram[17] = vinci_spr_palette[1];
    nes_palette_ram[18] = vinci_spr_palette[2];
    nes_palette_ram[19] = vinci_spr_palette[3];

    /* ---- Genesis BG (simple floor) ---- */
    {
        uint8_t floor_tile[32];
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 4; c++)
                floor_tile[r * 4 + c] = (r < 2) ? 0x11 : 0x00;
        /* Append floor tile after sprite tiles */
        int ft_idx = GEN_FRAMES * GEN_TPF;
        memcpy(&gen_tiles[ft_idx * 32], floor_tile, 32);

        gen_cram[16] = 0xFF1A1A3A; /* bg palette 1 */
        gen_cram[17] = 0xFF2A2A5A;

        memset(gen_bg_map, 0, sizeof(gen_bg_map));
        for (int y = 0; y < 28; y++)
            for (int x = 0; x < 40; x++)
                if (y >= 22)
                    gen_bg_map[y * 64 + x] = GEN_ENTRY(ft_idx, 1, 0, 0);
    }

    genesis_plane_t gen_plane_b = {
        .tiles = gen_tiles, .map = gen_bg_map, .cram = gen_cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = NULL,
        .map_w = 64, .map_h = 32, .enabled = 1,
    };
    static uint16_t gen_empty_map[64 * 32];
    memset(gen_empty_map, 0, sizeof(gen_empty_map));
    genesis_plane_t gen_plane_a = {
        .tiles = gen_tiles, .map = gen_empty_map, .cram = gen_cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = NULL,
        .map_w = 64, .map_h = 32, .enabled = 1,
    };

    /* ---- SNES BG ---- */
    memset(snes_bg_chr, 0, sizeof(snes_bg_chr));
    /* Tile 1: solid ground */
    for (int r = 0; r < 8; r++) {
        snes_bg_chr[32 + r*2] = 0xFF; snes_bg_chr[32 + r*2+1] = 0;
        snes_bg_chr[32 + r*2+16] = 0; snes_bg_chr[32 + r*2+17] = 0;
    }
    /* Tile 2: surface */
    for (int r = 0; r < 8; r++) {
        if (r < 2) {
            snes_bg_chr[64 + r*2] = 0xFF; snes_bg_chr[64 + r*2+1] = 0xFF;
            snes_bg_chr[64 + r*2+16] = 0; snes_bg_chr[64 + r*2+17] = 0;
        } else {
            snes_bg_chr[64 + r*2] = 0xFF; snes_bg_chr[64 + r*2+1] = 0;
            snes_bg_chr[64 + r*2+16] = 0; snes_bg_chr[64 + r*2+17] = 0;
        }
    }
    memset(snes_bg_map, 0, sizeof(snes_bg_map));
    for (int x = 0; x < 32; x++) {
        snes_bg_map[15 * 32 + x] = SNES_ENTRY(2, 0, 0, 0, 0);
        for (int y = 16; y < 32; y++)
            snes_bg_map[y * 32 + x] = SNES_ENTRY(1, 0, 0, 0, 0);
    }
    memset(snes_bg_pal, 0, sizeof(snes_bg_pal));
    snes_bg_pal[0] = 0xFF2040A0;
    snes_bg_pal[1] = 0xFF604020;
    snes_bg_pal[2] = 0xFF806040;
    snes_bg_pal[3] = 0xFF40A040;

    snes_bg_t snes_bg1 = {
        .tiles = snes_bg_chr, .map = snes_bg_map, .palette = snes_bg_pal,
        .scroll_x = 0, .scroll_y = 0, .map_w = 32, .map_h = 32,
        .bpp = 4, .enabled = 1,
    };

    /* ---- Display init ---- */
    video_init();

    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++) {
        fb0[i] = 0xFF000000; fb1[i] = 0xFF000000;
    }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    /* VI1: SNES PIP */
    video_vi1_init(SNES_X, SNES_Y, SNES_VW, SNES_VH);

    /* NES BG descriptor */
    nes_bg_t nes_bg = {
        .chr = nes_chr, .nametable = nes_nametable, .attribute = nes_attribute,
        .palette_ram = nes_palette_ram, .scroll_x = 0, .scroll_y = 0,
        .line_scroll_x = NULL, .enabled = 1,
    };

    /* GB BG descriptor */
    gb_bg_t gb_bg = {
        .chr = gb_chr, .map = gb_map, .map_attr = NULL,
        .palette = gb_palette, .scroll_x = 0, .scroll_y = 0, .enabled = 1,
    };

    uart_puts("[main] go!\n");

    /* ---- Animation state ---- */
    int buf = 0;
    int gb_frame = 0, gb_x = 40, gb_dx = 1, gb_flip = 0;
    int nes_frame = 0, nes_x = 80, nes_dx = 1, nes_flip = 0;
    int gen_frame = 0, gen_x = 100, gen_dx = 1, gen_flip = 0;
    int snes_frame = 0, snes_sx = 60, snes_dx = 1, snes_flip = 0;
    uint32_t gb_t = timer_read(), nes_t = gb_t, gen_t = gb_t, snes_t = gb_t;

    while (1) {
        volatile uint32_t *fb  = buf ? fb1 : fb0;
        volatile uint32_t *ovl = (volatile uint32_t *)(buf ? OVL1_ADDR : OVL_ADDR);
        uint32_t fb_addr  = buf ? FB1_ADDR : FB0_ADDR;
        uint32_t ovl_addr = buf ? OVL1_ADDR : OVL_ADDR;

        uint32_t now = timer_read();

        /* Advance animations */
        if (ticks_to_ms(timer_elapsed(gb_t, now)) > 100) {
            gb_frame = (gb_frame + 1) % CELEBI_FRAMES;
            gb_t = now;
        }
        if (ticks_to_ms(timer_elapsed(nes_t, now)) > 100) {
            nes_frame = (nes_frame + 1) % 8;
            nes_t = now;
        }
        if (ticks_to_ms(timer_elapsed(gen_t, now)) > 120) {
            gen_frame = (gen_frame + 1) % GEN_FRAMES;
            gen_t = now;
        }
        if (ticks_to_ms(timer_elapsed(snes_t, now)) > 150) {
            snes_frame = (snes_frame + 1) % SOLDIER_FRAMES;
            snes_t = now;
        }

        /* Move sprites */
        gb_x += gb_dx;
        if (gb_x > GB_NATIVE_W - CELEBI_PW - 4) { gb_dx = -1; gb_flip = 1; }
        if (gb_x < 4) { gb_dx = 1; gb_flip = 0; }

        nes_x += nes_dx;
        if (nes_x > NES_NATIVE_W - 16 - 4) { nes_dx = -1; nes_flip = 1; }
        if (nes_x < 4) { nes_dx = 1; nes_flip = 0; }

        gen_x += gen_dx;
        if (gen_x > GEN_NATIVE_W - 32 - 4) { gen_dx = -2; gen_flip = 1; }
        if (gen_x < 4) { gen_dx = 2; gen_flip = 0; }

        snes_sx += snes_dx;
        if (snes_sx > SNES_VW - SOLDIER_PW - 4) { snes_dx = -1; snes_flip = 1; }
        if (snes_sx < 4) { snes_dx = 1; snes_flip = 0; }

        /* ============================================================ */
        /* 1) Genesis → VI0 (Plane B backdrop) + UI0 (sprites)          */
        /* ============================================================ */
        int gen_y = 22 * 8 - 56; /* feet on floor row 22 */
        gen_sprites[0] = (genesis_sprite_t){
            .x = gen_x, .y = gen_y,
            .tile = gen_frame * GEN_TPF,
            .w = 4, .h = 4, .pal = 0,
            .fliph = gen_flip, .flipv = 0, .enabled = 1,
        };
        gen_sprites[1] = (genesis_sprite_t){
            .x = gen_x, .y = gen_y + 32,
            .tile = gen_frame * GEN_TPF + 16,
            .w = 4, .h = 3, .pal = 0,
            .fliph = gen_flip, .flipv = 0, .enabled = 1,
        };

        memset((void *)ovl, 0, LCD_FB_BYTES);
        genesis_render((uint32_t *)fb, (uint32_t *)ovl, LCD_W, LCD_H,
                       0xFF1A1A3A, &gen_plane_a, &gen_plane_b, NULL,
                       gen_sprites, 2, 1);

        /* ============================================================ */
        /* 2) GB → temp buffers → composite → blit to VI0 left          */
        /* ============================================================ */
        {
            int spr_y = 15 * 8 - CELEBI_PH;
            int num_oam = 0;
            const uint8_t *chr = &celebi_chr[gb_frame * CELEBI_TPF * 16];
            for (int ty = 0; ty < CELEBI_TH && num_oam < GB_MAX_SPRITES; ty++)
                for (int tx = 0; tx < CELEBI_TW && num_oam < GB_MAX_SPRITES; tx++) {
                    int ox = gb_flip ? (CELEBI_TW - 1 - tx) * 8 : tx * 8;
                    gb_oam[num_oam++] = (gb_oam_entry_t){
                        .y = (uint8_t)(spr_y + ty * 8 + 16),
                        .x = (uint8_t)(gb_x + ox + 8),
                        .tile = (uint8_t)(ty * CELEBI_TW + tx),
                        .attr = GB_SPR_PAL(0) | (gb_flip ? GB_SPR_HFLIP : 0),
                    };
                }

            memset(gb_ovl, 0, sizeof(gb_ovl));
            gb_render(gb_fb, gb_ovl, GB_NATIVE_W, GB_NATIVE_H,
                      &gb_bg, chr, gb_spr_pal, gb_oam, num_oam, 0);
            composite(gb_fb, gb_ovl, GB_NATIVE_W * GB_NATIVE_H);
            blit(fb, gb_fb, GB_NATIVE_W, GB_NATIVE_H, GB_X, GB_Y);
        }

        /* ============================================================ */
        /* 3) NES → temp buffers → composite → blit to VI0 right        */
        /* ============================================================ */
        {
            int spr_y = 25 * 8 - 24; /* feet at row 25 */
            int num_oam = 0;
            const uint8_t *chr = &vinci_spr_chr[nes_frame * 6 * 16];
            for (int ty = 0; ty < 3 && num_oam < NES_MAX_SPRITES; ty++)
                for (int tx = 0; tx < 2 && num_oam < NES_MAX_SPRITES; tx++) {
                    int ox = nes_flip ? (1 - tx) * 8 : tx * 8;
                    nes_oam[num_oam++] = (nes_oam_entry_t){
                        .y = (uint8_t)(spr_y + ty * 8 - 1),
                        .tile = (uint8_t)(ty * 2 + tx),
                        .attr = NES_SPR_PAL(0) | (nes_flip ? NES_SPR_HFLIP : 0),
                        .x = (uint8_t)(nes_x + ox),
                    };
                }

            memset(nes_ovl, 0, sizeof(nes_ovl));
            nes_render(nes_fb, nes_ovl, NES_NATIVE_W, NES_NATIVE_H,
                       &nes_bg, chr, nes_oam, num_oam, 0);
            composite(nes_fb, nes_ovl, NES_NATIVE_W * NES_NATIVE_H);
            blit(fb, nes_fb, NES_NATIVE_W, NES_NATIVE_H, NES_X, NES_Y);
        }

        /* ============================================================ */
        /* 4) SNES → VI1 PIP                                            */
        /* ============================================================ */
        {
            memset(snes_tmp, 0, sizeof(snes_tmp));
            snes_mode1_render(snes_comp, snes_tmp, SNES_VW, SNES_VH,
                              snes_bg_pal[0], &snes_bg1, NULL, NULL, 0);
            snes_sprites[0] = (snes_sprite_t){
                .x = snes_sx, .y = 15 * 8 - SOLDIER_PH,
                .tile = snes_frame * SOLDIER_TPF,
                .w = SOLDIER_TW, .h = SOLDIER_TH,
                .pal = 0, .priority = 0,
                .fliph = snes_flip, .flipv = 0, .enabled = 1,
            };
            snes_render_sprites(snes_tmp, SNES_VW, SNES_VH,
                                soldier_chr, soldier_pal,
                                snes_sprites, 1, 0);
            composite(snes_comp, snes_tmp, SNES_VW * SNES_VH);
            memcpy((void *)SPR_ADDR, snes_comp, SNES_VW * SNES_VH * 4);
            dcache_clean_range(SPR_ADDR, SNES_VW * SNES_VH * 4);
        }

        /* ============================================================ */
        /* Flush + sync                                                  */
        /* ============================================================ */
        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        video_wait_vblank();
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf = !buf;
    }
    return 0;
}
