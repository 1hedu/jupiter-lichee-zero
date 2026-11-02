/* Exercise the actual changed renderers under ASan/UBSan. No MMIO. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "nes.h"
#include "gb.h"
#include "snes.h"
#include "genesis.h"
enum { W = 512, H = 300 }; /* Larger than the 480x272 opacity arena. */
static uint32_t fb[W * H], ovl[W * H];
static uint8_t chr2[3 * 16], nes_map[960], attr[64], nes_pal[32], gb_map[1024];
static uint32_t palette[128];

static void test_nes(void)
{
    memset(nes_map, 1, sizeof(nes_map));
    memset(nes_map + 29 * 32, 2, 32);
    nes_pal[1] = 1; nes_pal[2] = 2; nes_pal[17] = 3;
    nes_bg_t bg = {.chr = chr2, .nametable = nes_map, .attribute = attr,
                   .palette_ram = nes_pal, .scroll_y = -1, .enabled = 1};
    nes_oam_entry_t sprites[64] = {{0}};
    for (unsigned i = 0; i < 64; ++i) sprites[i].y = 255;
    for (unsigned i = 0; i < 9; ++i)
        sprites[i] = (nes_oam_entry_t){.y = 10, .x = (uint8_t)(i * 8), .tile = 1};
    nes_render(fb, ovl, W, H, &bg, chr2, sprites, 65, 0);
    assert(fb[0] == nes_master_palette[2]); /* -1 wraps to row 239. */
    assert(ovl[11 * W] == nes_master_palette[3] && !ovl[11 * W + 64]);
    sprites[0].attr = NES_SPR_BEHIND;
    memset(ovl, 0, sizeof(ovl));
    nes_render(fb, ovl, W, H, &bg, chr2, sprites, 1, 0);
    assert(!ovl[11 * W]);
    bg.enabled = 0;
    nes_render(fb, ovl, W, H, &bg, chr2, sprites, 1, 0);
    assert(ovl[11 * W] == nes_master_palette[3]); /* No stale opacity. */
    memset(ovl, 0, sizeof(ovl));
    nes_render(fb, ovl, W, H, NULL, chr2, sprites, 65, 0);
    assert(!ovl[11 * W]);
}

static void test_gb(void)
{
    memset(gb_map, 1, sizeof(gb_map));
    gb_bg_t bg = {.chr = chr2, .map = gb_map, .palette = palette, .enabled = 1};
    gb_oam_entry_t sprites[40] = {{0}};
    for (unsigned i = 0; i < 40; ++i) sprites[i].y = 220;
    for (unsigned i = 0; i < 11; ++i)
        sprites[i] = (gb_oam_entry_t){.y = 26, .x = (uint8_t)(8 + i * 8), .tile = 1};
    memset(ovl, 0, sizeof(ovl));
    gb_render(fb, ovl, W, H, &bg, chr2, palette, sprites, 41, 0);
    assert(ovl[10 * W] == palette[1] && !ovl[10 * W + 80]);
    sprites[0].attr = GB_SPR_PRIORITY;
    memset(ovl, 0, sizeof(ovl));
    gb_render(fb, ovl, W, H, &bg, chr2, palette, sprites, 1, 0);
    assert(!ovl[10 * W]);
    bg.enabled = 0;
    gb_render(fb, ovl, W, H, &bg, chr2, palette, sprites, 1, 0);
    assert(ovl[10 * W] == palette[1]);
}

static void test_snes(void)
{
    static uint8_t tiles[257 * 32], sprite_chr[32];
    uint16_t map = 256;
    memset(tiles + 256 * 32, 0x11, 32);
    snes_bg_t bg = {.tiles = tiles, .map = &map, .palette = palette,
                    .map_w = 1, .map_h = 1, .bpp = 4, .enabled = 1};
    snes_bg_t disabled = {.map_w = 1, .map_h = 1};
    snes_mode1_render(fb, ovl, W, H, 0, &bg, &disabled, &disabled, 0);
    assert(ovl[0] == palette[1]); /* Tile 256 must not narrow to zero. */
    for (unsigned row = 0; row < 8; ++row) sprite_chr[row * 2] = 0xff;
    snes_sprite_t sprites[128] = {{0}};
    for (unsigned i = 0; i < 33; ++i)
        sprites[i] = (snes_sprite_t){.x = (int16_t)(i * 8), .y = 10,
                                    .w = 1, .h = 1, .enabled = 1};
    sprites[127] = (snes_sprite_t){.x = 0, .y = 250, .w = 1, .h = 1, .enabled = 1};
    memset(ovl, 0, sizeof(ovl));
    snes_render_sprites(ovl, W, H, sprite_chr, palette, sprites, 129, 0);
    assert(ovl[10 * W] == palette[1] && !ovl[10 * W + 256]);
    assert(ovl[250 * W] == palette[1]); /* Beyond native scanline arena. */
}

static void test_genesis(void)
{
    uint8_t tiles[4 * 32] = {0};
    uint16_t empty = 0, map[9];
    for (unsigned i = 1; i <= 3; ++i) memset(tiles + i * 32, (int)i * 17, 32);
    for (unsigned y = 0; y < 3; ++y)
        for (unsigned x = 0; x < 3; ++x) map[y * 3 + x] = (x + y) % 3 + 1;
    genesis_plane_t plane = {.tiles = tiles, .map = &empty, .cram = palette,
                            .map_w = 1, .map_h = 1, .enabled = 1};
    genesis_window_t window = {.map = map, .map_w = 3, .map_h = 3, .w = 48, .h = 32};
    genesis_render(fb, ovl, W, H, 0, &plane, NULL, &window, NULL, 0, 0);
    for (unsigned y = 0; y < 4; ++y)
        for (unsigned x = 0; x < 6; ++x)
            assert(ovl[y * 8 * W + x * 8] == palette[(x + y) % 3 + 1]);
}

int main(void)
{
    memset(chr2 + 16, 0xff, 8); /* Tile1=color1, tile2=color2. */
    memset(chr2 + 32 + 8, 0xff, 8);
    for (unsigned i = 0; i < 128; ++i) palette[i] = 0xff102030u + i;
    test_nes(); test_gb(); test_snes(); test_genesis();
    puts("PPU PASS: forward OAM limits, oversized counts/frames, stale opacity, signed NES scroll, SNES tile256 and arbitrary Genesis windows");
    return 0;
}
