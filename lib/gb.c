/*
 * Jupiter SDK — Game Boy / Game Boy Color PPU renderer
 * 160×144, 2bpp tiles, 40 sprites (10/line), GBC per-tile palettes.
 */
#include "gb.h"
#include "jupiter.h"

static inline uint8_t chr_pixel(const uint8_t *tile, int row, int col)
{
    int shift = 7 - col;
    return ((tile[row] >> shift) & 1) | (((tile[row + 8] >> shift) & 1) << 1);
}

static void render_bg(uint32_t *fb, uint32_t pitch,
                      uint32_t x0, uint32_t y0, uint32_t rw, uint32_t rh,
                      const gb_bg_t *bg)
{
    if (!bg || !bg->enabled || !bg->chr || !bg->map) return;
    uint32_t backdrop = bg->palette[0];

    for (uint32_t sy = 0; sy < rh; sy++) {
        uint32_t wy = ((uint32_t)((int32_t)sy + bg->scroll_y)) & 0xFF;
        uint32_t tr = wy / 8;
        uint32_t fy = wy & 7;
        uint32_t *row = fb + (y0 + sy) * pitch + x0;

        for (uint32_t sx = 0; sx < rw; sx++) {
            uint32_t wx = ((uint32_t)((int32_t)sx + bg->scroll_x)) & 0xFF;
            uint32_t tc = wx / 8;
            uint32_t fx = wx & 7;

            uint32_t mi = (tr & 31) * GB_MAP_W + (tc & 31);
            uint8_t tidx = bg->map[mi];

            /* GBC attributes */
            int pal = 0, hflip = 0, vflip = 0;
            if (bg->map_attr) {
                uint8_t attr = bg->map_attr[mi];
                pal = attr & 7;
                hflip = (attr >> 5) & 1;
                vflip = (attr >> 6) & 1;
            }

            int actual_fy = vflip ? (7 - (int)fy) : (int)fy;
            int actual_fx = hflip ? (7 - (int)fx) : (int)fx;
            uint8_t ci = chr_pixel(bg->chr + tidx * 16, actual_fy, actual_fx);

            row[sx] = (ci == 0) ? backdrop : bg->palette[pal * 4 + ci];
        }
    }
}

static void render_sprites(uint32_t *ovl, uint32_t pitch,
                           uint32_t x0, uint32_t y0, uint32_t rw, uint32_t rh,
                           const uint8_t *chr, const uint32_t *pal,
                           const gb_oam_entry_t *oam, uint32_t num,
                           const gb_bg_t *bg)
{
    if (!oam || !chr || !pal || num == 0) return;

    uint8_t line_count[GB_NATIVE_H];
    for (uint32_t i = 0; i < GB_NATIVE_H; i++) line_count[i] = 0;

    /* Build BG opacity for behind-priority sprites */
    static uint8_t bg_opaque[GB_NATIVE_W * GB_NATIVE_H];
    if (bg && bg->enabled && bg->chr && bg->map) {
        for (uint32_t sy = 0; sy < rh; sy++) {
            uint32_t wy = ((uint32_t)((int32_t)sy + bg->scroll_y)) & 0xFF;
            uint32_t tr = wy / 8, fy = wy & 7;
            for (uint32_t sx = 0; sx < rw; sx++) {
                uint32_t wx = ((uint32_t)((int32_t)sx + bg->scroll_x)) & 0xFF;
                uint32_t tc = wx / 8, fx = wx & 7;
                uint32_t mi = (tr & 31) * GB_MAP_W + (tc & 31);
                uint8_t tidx = bg->map[mi];
                int vf = 0, hf = 0;
                if (bg->map_attr) {
                    uint8_t a = bg->map_attr[mi];
                    hf = (a>>5)&1; vf = (a>>6)&1;
                }
                int afy = vf ? 7-(int)fy : (int)fy;
                int afx = hf ? 7-(int)fx : (int)fx;
                bg_opaque[sy * rw + sx] = chr_pixel(bg->chr + tidx*16, afy, afx) != 0;
            }
        }
    }

    /* Render in reverse OAM order (lower index = higher priority) */
    for (int s = (int)num - 1; s >= 0; s--) {
        const gb_oam_entry_t *sp = &oam[s];
        int sy = (int)sp->y - 16;  /* GB OAM Y is offset by 16 */
        int sx = (int)sp->x - 8;   /* GB OAM X is offset by 8 */
        int hflip = (sp->attr >> 5) & 1;
        int vflip = (sp->attr >> 6) & 1;
        int behind = (sp->attr >> 7) & 1;
        int spal = sp->attr & 7;

        const uint8_t *tile = chr + sp->tile * 16;

        for (int py = 0; py < 8; py++) {
            int screen_y = sy + py;
            if (screen_y < 0 || screen_y >= (int)rh) continue;
            if (line_count[screen_y] >= GB_SPRITES_PER_LINE) continue;

            int tr = vflip ? (7 - py) : py;
            int drew = 0;

            for (int px = 0; px < 8; px++) {
                int screen_x = sx + px;
                if (screen_x < 0 || screen_x >= (int)rw) continue;

                int tc = hflip ? (7 - px) : px;
                uint8_t ci = chr_pixel(tile, tr, tc);
                if (ci == 0) continue;

                if (behind && bg_opaque[screen_y * rw + screen_x]) continue;

                ovl[(y0 + screen_y) * pitch + (x0 + screen_x)] = pal[spal * 4 + ci];
                drew = 1;
            }
            if (drew) line_count[screen_y]++;
        }
    }
}

void gb_render(uint32_t *fb, uint32_t *ovl,
               uint32_t fb_w, uint32_t fb_h,
               const gb_bg_t *bg,
               const uint8_t *sprite_chr,
               const uint32_t *sprite_palette,
               const gb_oam_entry_t *oam, uint32_t num_sprites,
               int authentic)
{
    uint32_t rw, rh, x0, y0;
    if (authentic) {
        rw = GB_NATIVE_W; rh = GB_NATIVE_H;
        x0 = (fb_w - rw) / 2; y0 = (fb_h - rh) / 2;
    } else {
        rw = fb_w; rh = fb_h; x0 = 0; y0 = 0;
    }

    render_bg(fb, fb_w, x0, y0, rw, rh, bg);
    render_sprites(ovl, fb_w, x0, y0, rw, rh,
                   sprite_chr, sprite_palette, oam, num_sprites, bg);
}
