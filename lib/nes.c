/*
 * Jupiter SDK — NES PPU-style renderer
 *
 * Accurate 2C02 rendering model:
 *   Background: 2bpp tiles, 32×30 nametable, attribute table palette select
 *   Sprites: 64 OAM entries, 8x8, priority/flip, 4 sprite palettes
 *   Compositing: sprite behind-BG shows through BG-transparent pixels
 *
 * DE2 mapping: Background → VI0 (opaque), Sprites → UI0 (alpha-keyed)
 */
#include "nes.h"
#include "jupiter.h"

/* ================================================================== */
/* NES Master Palette — 64 colors as XRGB8888                          */
/* Standard NTSC palette (widely used approximation)                    */
/* ================================================================== */
const uint32_t nes_master_palette[64] = {
    0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088,
    0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800,
    0xFF202A00, 0xFF083A00, 0xFF004000, 0xFF003C00,
    0xFF00323C, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4,
    0xFF8800B0, 0xFFA00060, 0xFF981A20, 0xFF783C00,
    0xFF545A00, 0xFF207200, 0xFF007C00, 0xFF007628,
    0xFF006678, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFECEEEC, 0xFF4C9AEC, 0xFF787CEC, 0xFFB062EC,
    0xFFE400DC, 0xFFEC0078, 0xFFEC3C30, 0xFFCC6C00,
    0xFF8C9000, 0xFF48A800, 0xFF10B800, 0xFF00B260,
    0xFF00A2C4, 0xFF4C4C4C, 0xFF000000, 0xFF000000,
    0xFFECEEEC, 0xFFA8CCEC, 0xFFBCBCEC, 0xFFD4B2EC,
    0xFFECAEEC, 0xFFECAED4, 0xFFECB4B0, 0xFFE4C490,
    0xFFCCD278, 0xFFB4DE78, 0xFFA8E290, 0xFF98E2B4,
    0xFFA0D6E4, 0xFFA0A2A0, 0xFF000000, 0xFF000000,
};

/* ================================================================== */
/* Decode one 2bpp pixel from CHR tile data                             */
/* ================================================================== */
static inline uint8_t chr_pixel(const uint8_t *tile, int row, int col)
{
    /* col: 0=left, 7=right */
    int shift = 7 - col;
    uint8_t bp0 = (tile[row]     >> shift) & 1;
    uint8_t bp1 = (tile[row + 8] >> shift) & 1;
    return (bp1 << 1) | bp0;
}

/* ================================================================== */
/* Resolve palette: NES color index → XRGB8888                         */
/* ================================================================== */
static inline uint32_t resolve_bg_color(const uint8_t *pal_ram,
                                         int pal_num, int color_idx)
{
    if (color_idx == 0)
        return nes_master_palette[pal_ram[0] & 0x3F];  /* universal BG */
    return nes_master_palette[pal_ram[pal_num * 4 + color_idx] & 0x3F];
}

static inline uint32_t resolve_spr_color(const uint8_t *pal_ram,
                                          int pal_num, int color_idx)
{
    /* Sprite palette starts at offset 16 in palette RAM */
    return nes_master_palette[pal_ram[16 + pal_num * 4 + color_idx] & 0x3F];
}

/* ================================================================== */
/* Get attribute palette for a tile position                            */
/* ================================================================== */
static inline int get_attr_palette(const uint8_t *attr, int tx, int ty)
{
    int attr_idx = (ty / 4) * 8 + (tx / 4);
    int shift = ((ty & 2) << 1) | (tx & 2);
    return (attr[attr_idx] >> shift) & 3;
}

/* ================================================================== */
/* Render background to VI0                                             */
/* ================================================================== */
static void render_bg(uint32_t *fb, uint32_t pitch,
                      uint32_t x0, uint32_t y0,
                      uint32_t rw, uint32_t rh,
                      const nes_bg_t *bg)
{
    if (!bg || !bg->enabled || !bg->chr || !bg->nametable) return;

    uint32_t backdrop = nes_master_palette[bg->palette_ram[0] & 0x3F];

    for (uint32_t sy = 0; sy < rh; sy++) {
        int16_t scr_x = bg->scroll_x;
        if (bg->line_scroll_x)
            scr_x += bg->line_scroll_x[sy];

        uint32_t world_y = (uint32_t)((int32_t)sy + bg->scroll_y) % (NES_NT_H * 8);
        uint32_t tile_row = world_y / 8;
        uint32_t fine_y   = world_y & 7;

        uint32_t *row = fb + (y0 + sy) * pitch + x0;

        for (uint32_t sx = 0; sx < rw; sx++) {
            uint32_t world_x = (uint32_t)((int32_t)sx + scr_x) % (NES_NT_W * 8);
            uint32_t tile_col = world_x / 8;
            uint32_t fine_x   = world_x & 7;

            /* Tile index from nametable */
            uint8_t tile_idx = bg->nametable[tile_row * NES_NT_W + tile_col];
            const uint8_t *tile = bg->chr + tile_idx * 16;

            /* Color index from CHR */
            uint8_t ci = chr_pixel(tile, fine_y, fine_x);

            if (ci == 0) {
                row[sx] = backdrop;
            } else {
                int pal_num = get_attr_palette(bg->attribute, tile_col, tile_row);
                row[sx] = resolve_bg_color(bg->palette_ram, pal_num, ci);
            }
        }
    }
}

/* ================================================================== */
/* Render sprites to UI0                                                */
/* ================================================================== */
static void render_sprites(uint32_t *ovl, uint32_t pitch,
                           uint32_t x0, uint32_t y0,
                           uint32_t rw, uint32_t rh,
                           const nes_bg_t *bg,
                           const uint8_t *sprite_chr,
                           const nes_oam_entry_t *oam, uint32_t num_sprites,
                           /* BG opacity buffer for behind-sprite priority */
                           const uint8_t *bg_opaque)
{
    if (!oam || !sprite_chr || num_sprites == 0) return;

    /* Per-scanline sprite limit tracking */
    uint8_t scanline_count[NES_FULL_H];
    for (uint32_t i = 0; i < NES_FULL_H; i++) scanline_count[i] = 0;

    /* Render sprites in reverse OAM order so lower indices have priority */
    for (int s = (int)num_sprites - 1; s >= 0; s--) {
        const nes_oam_entry_t *spr = &oam[s];
        int spr_y = (int)spr->y + 1;  /* NES Y is one less than actual */
        int spr_x = (int)spr->x;
        int hflip = NES_SPR_GET_HFLIP(spr->attr);
        int vflip = NES_SPR_GET_VFLIP(spr->attr);
        int behind = NES_SPR_GET_BEHIND(spr->attr);
        int spr_pal = NES_SPR_GET_PAL(spr->attr);

        const uint8_t *tile = sprite_chr + spr->tile * 16;

        for (int py = 0; py < 8; py++) {
            int screen_y = spr_y + py;
            if (screen_y < 0 || screen_y >= (int)rh) continue;

            /* 8 sprites per scanline limit */
            if (scanline_count[screen_y] >= 8) continue;

            int tile_row = vflip ? (7 - py) : py;
            int drew_pixel = 0;

            for (int px = 0; px < 8; px++) {
                int screen_x = spr_x + px;
                if (screen_x < 0 || screen_x >= (int)rw) continue;

                int tile_col = hflip ? (7 - px) : px;
                uint8_t ci = chr_pixel(tile, tile_row, tile_col);

                if (ci == 0) continue;  /* transparent */

                /* Behind-BG priority: only show through transparent BG pixels */
                if (behind && bg_opaque && bg_opaque[screen_y * rw + screen_x])
                    continue;

                uint32_t color = resolve_spr_color(bg->palette_ram, spr_pal, ci);
                ovl[(y0 + screen_y) * pitch + (x0 + screen_x)] = color;
                drew_pixel = 1;
            }

            if (drew_pixel)
                scanline_count[screen_y]++;
        }
    }
}

/* ================================================================== */
/* Main render function                                                 */
/* ================================================================== */
void nes_render(uint32_t *fb, uint32_t *ovl,
                uint32_t fb_w, uint32_t fb_h,
                const nes_bg_t *bg,
                const uint8_t *sprite_chr,
                const nes_oam_entry_t *oam, uint32_t num_sprites,
                int authentic)
{
    uint32_t rw, rh, x0, y0;

    if (authentic) {
        rw = NES_NATIVE_W;
        rh = NES_NATIVE_H;
        x0 = (fb_w - rw) / 2;
        y0 = (fb_h - rh) / 2;
    } else {
        rw = fb_w;
        rh = fb_h;
        x0 = 0;
        y0 = 0;
    }

    /* Render background to VI0 */
    render_bg(fb, fb_w, x0, y0, rw, rh, bg);

    /* Build BG opacity map for sprite behind-BG priority */
    static uint8_t bg_opaque_buf[NES_NATIVE_W * NES_FULL_H];
    if (bg && bg->enabled && bg->chr && bg->nametable) {
        for (uint32_t sy = 0; sy < rh; sy++) {
            int16_t scr_x = bg->scroll_x;
            if (bg->line_scroll_x)
                scr_x += bg->line_scroll_x[sy];
            uint32_t world_y = (uint32_t)((int32_t)sy + bg->scroll_y) % (NES_NT_H * 8);
            uint32_t tile_row = world_y / 8;
            uint32_t fine_y = world_y & 7;

            for (uint32_t sx = 0; sx < rw; sx++) {
                uint32_t world_x = (uint32_t)((int32_t)sx + scr_x) % (NES_NT_W * 8);
                uint32_t tile_col = world_x / 8;
                uint32_t fine_x = world_x & 7;
                uint8_t tile_idx = bg->nametable[tile_row * NES_NT_W + tile_col];
                uint8_t ci = chr_pixel(bg->chr + tile_idx * 16, fine_y, fine_x);
                bg_opaque_buf[sy * rw + sx] = (ci != 0) ? 1 : 0;
            }
        }
    }

    /* Render sprites to UI0 */
    render_sprites(ovl, fb_w, x0, y0, rw, rh,
                   bg, sprite_chr, oam, num_sprites,
                   bg_opaque_buf);
}
