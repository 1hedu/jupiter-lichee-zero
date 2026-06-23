/*
 * Jupiter SDK — NES PPU-style background + sprite renderer
 *
 * Accurate 2C02 PPU rendering model:
 *   - 2bpp tiles, 16 bytes per 8x8 tile (two bitplanes)
 *   - 32×30 nametable with 64-byte attribute table
 *   - 4 BG palettes + 4 sprite palettes, 4 colors each
 *   - Universal background color at palette index 0
 *   - 64 sprites (OAM), 8x8 or 8x16, with priority/flip
 *   - Sprite priority: behind-BG sprites show through BG-transparent pixels
 *
 * Rendering model (DE2 layer mapping):
 *   Background → VI0 (opaque with backdrop color)
 *   Sprites    → UI0 (alpha-keyed overlay)
 *
 * Resolution: 256×240 full, 256×224 safe area (authentic mode uses 256×224).
 */
#ifndef NES_H
#define NES_H

#include <stdint.h>

/* Native NES resolution */
#define NES_NATIVE_W  256
#define NES_NATIVE_H  224   /* safe visible area (full is 240) */
#define NES_FULL_H    240

/* ================================================================== */
/* Tile format: 2bpp, 16 bytes per 8x8 tile                           */
/*                                                                      */
/* Bytes [0..7]  = bitplane 0 (bit 0 of color index)                   */
/* Bytes [8..15] = bitplane 1 (bit 1 of color index)                   */
/*                                                                      */
/* For row r, pixel p (0=left, 7=right):                               */
/*   bit0 = (tile[r]   >> (7-p)) & 1                                   */
/*   bit1 = (tile[r+8] >> (7-p)) & 1                                   */
/*   color_index = (bit1 << 1) | bit0    (0=transparent/bg, 1-3=opaque) */
/* ================================================================== */

/* Nametable: 32×30 tile indices (960 bytes) + 64-byte attribute table */
#define NES_NT_W   32
#define NES_NT_H   30
#define NES_NT_TILES  (NES_NT_W * NES_NT_H)  /* 960 */
#define NES_NT_ATTRS  64

/* Attribute table: each byte covers 4×4 tiles (32×32 pixels).
 * 2-bit palette select per 2×2-tile (16×16 pixel) quadrant.
 *
 * Byte layout:
 *   bits 1-0: top-left     quadrant (tiles col+0..1, row+0..1)
 *   bits 3-2: top-right    quadrant (tiles col+2..3, row+0..1)
 *   bits 5-4: bottom-left  quadrant (tiles col+0..1, row+2..3)
 *   bits 7-6: bottom-right quadrant (tiles col+2..3, row+2..3)
 *
 * To get palette for tile at (tx, ty):
 *   attr_idx = (ty / 4) * 8 + (tx / 4)
 *   shift    = ((ty & 2) << 1) | (tx & 2)
 *   pal_num  = (attr[attr_idx] >> shift) & 3
 */

/* Build an attribute byte from four 2-bit palette selections */
#define NES_ATTR(tl, tr, bl, br) \
    (((tl)&3) | (((tr)&3)<<2) | (((bl)&3)<<4) | (((br)&3)<<6))

/* ================================================================== */
/* Palette                                                              */
/* ================================================================== */

/* NES master palette: 64 RGB colors.
 * Index into this with the 6-bit NES color value from palette RAM. */
extern const uint32_t nes_master_palette[64];

/* Palette RAM layout (mirrors NES $3F00–$3F1F):
 *   [0]      = universal background color (6-bit NES color index)
 *   [1..3]   = BG palette 0, colors 1-3
 *   [4..7]   = BG palette 1 (index 4 mirrors universal BG)
 *   [8..11]  = BG palette 2
 *   [12..15] = BG palette 3
 *   [16]     = mirrors universal BG
 *   [17..19] = sprite palette 0, colors 1-3
 *   [20..23] = sprite palette 1
 *   [24..27] = sprite palette 2
 *   [28..31] = sprite palette 3
 *
 * Each entry is a 6-bit NES color index (0–63) into nes_master_palette[]. */

/* ================================================================== */
/* Sprite (OAM)                                                         */
/* ================================================================== */

/* OAM entry — matches NES byte order */
typedef struct {
    uint8_t y;         /* Y position (sprite visible at y+1) */
    uint8_t tile;      /* tile index (8x8: index into sprite CHR table) */
    uint8_t attr;      /* attributes (see below) */
    uint8_t x;         /* X position */
} nes_oam_entry_t;

/* OAM attribute bits */
#define NES_SPR_VFLIP     (1 << 7)
#define NES_SPR_HFLIP     (1 << 6)
#define NES_SPR_BEHIND    (1 << 5)  /* 1 = behind background */
#define NES_SPR_PAL(p)    ((p) & 3) /* sprite palette 0-3 */

#define NES_SPR_GET_PAL(a)   ((a) & 3)
#define NES_SPR_GET_VFLIP(a) ((a) >> 7)
#define NES_SPR_GET_HFLIP(a) (((a) >> 6) & 1)
#define NES_SPR_GET_BEHIND(a) (((a) >> 5) & 1)

/* Maximum sprites */
#define NES_MAX_SPRITES  64
#define NES_OAM_SIZE     (NES_MAX_SPRITES * sizeof(nes_oam_entry_t))

/* ================================================================== */
/* Background descriptor                                                */
/* ================================================================== */
typedef struct {
    const uint8_t  *chr;          /* 2bpp tile data (16 bytes per tile, 256 tiles = 4KB) */
    const uint8_t  *nametable;    /* 960 tile indices */
    const uint8_t  *attribute;    /* 64-byte attribute table */
    const uint8_t  *palette_ram;  /* 32 bytes: BG palettes [0..15], sprite palettes [16..31] */
    int16_t scroll_x, scroll_y;  /* pixel scroll offset */
    /* Optional per-scanline X scroll (for split-scroll effects).
     * Array of NES_NATIVE_H entries. NULL to disable. Added to scroll_x. */
    const int16_t  *line_scroll_x;
    uint8_t enabled;
} nes_bg_t;

/* ================================================================== */
/* Render function                                                      */
/* ================================================================== */

/*
 * Render one NES frame.
 *
 *   fb          VI0 framebuffer (XRGB8888, opaque — background renders here)
 *   ovl         UI0 overlay (ARGB8888, transparent where empty — sprites here)
 *   fb_w, fb_h  framebuffer dimensions (LCD_W × LCD_H)
 *   bg          background descriptor (nametable + CHR + attributes + palette)
 *   sprite_chr  sprite CHR data (may differ from BG CHR; 2bpp, 4KB)
 *   oam         64-entry OAM array (NULL to skip sprites)
 *   num_sprites number of active sprites (0–64)
 *   authentic   1 = centered 256×224 with pillarbox; 0 = full-screen
 */
void nes_render(uint32_t *fb, uint32_t *ovl,
                uint32_t fb_w, uint32_t fb_h,
                const nes_bg_t *bg,
                const uint8_t *sprite_chr,
                const nes_oam_entry_t *oam, uint32_t num_sprites,
                int authentic);

#endif /* NES_H */
