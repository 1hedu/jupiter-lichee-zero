/*
 * Jupiter SDK — Game Boy / Game Boy Color PPU renderer
 *
 * 2bpp tiles (same bitplane layout as NES), 160×144 resolution.
 * GBC: per-tile palette select (8 BG + 8 sprite palettes of 4 colors).
 *
 * Rendering model:
 *   Background → VI0 (opaque)
 *   Sprites    → UI0 (alpha-keyed)
 */
#ifndef GB_H
#define GB_H

#include <stdint.h>

#define GB_NATIVE_W  160
#define GB_NATIVE_H  144

#define GB_MAP_W     32
#define GB_MAP_H     32
#define GB_MAP_SIZE  (GB_MAP_W * GB_MAP_H)

#define GB_MAX_SPRITES  40
#define GB_SPRITES_PER_LINE 10

/* OAM entry (matches GB byte order) */
typedef struct {
    uint8_t y;       /* Y + 16 (sprite visible when scanline in [y-16, y-9]) */
    uint8_t x;       /* X + 8 */
    uint8_t tile;    /* tile index */
    uint8_t attr;    /* attributes */
} gb_oam_entry_t;

/* OAM attribute bits (GBC) */
#define GB_SPR_PRIORITY   (1 << 7)  /* 1 = behind BG colors 1-3 */
#define GB_SPR_VFLIP      (1 << 6)
#define GB_SPR_HFLIP      (1 << 5)
#define GB_SPR_PAL_DMG    (1 << 4)  /* DMG: OBP1 select */
#define GB_SPR_BANK       (1 << 3)  /* GBC: tile VRAM bank */
#define GB_SPR_PAL(n)     ((n) & 7) /* GBC: palette 0-7 */

/* BG map attribute (GBC only, one per tile in a separate attribute map) */
#define GB_BG_PRIORITY    (1 << 7)
#define GB_BG_VFLIP       (1 << 6)
#define GB_BG_HFLIP       (1 << 5)
#define GB_BG_BANK        (1 << 3)
#define GB_BG_PAL(n)      ((n) & 7)

/* Background descriptor */
typedef struct {
    const uint8_t  *chr;          /* 2bpp tile data (16 bytes/tile) */
    const uint8_t  *map;          /* 32×32 tile indices */
    const uint8_t  *map_attr;     /* 32×32 GBC tile attributes (NULL for DMG) */
    const uint32_t *palette;      /* ARGB8888 colors: 8 palettes × 4 colors = 32 entries */
    int16_t scroll_x, scroll_y;
    uint8_t enabled;
} gb_bg_t;

/* Render one GB/GBC frame */
void gb_render(uint32_t *fb, uint32_t *ovl,
               uint32_t fb_w, uint32_t fb_h,
               const gb_bg_t *bg,
               const uint8_t *sprite_chr,
               const uint32_t *sprite_palette, /* 8 palettes × 4 colors = 32 ARGB entries */
               const gb_oam_entry_t *oam, uint32_t num_sprites,
               int authentic);

#endif
