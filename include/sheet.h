/*
 * Jupiter SDK — Spriters-Resource sprite-sheet toolkit
 *
 * Turns an ARGB8888 sprite sheet (typically ripped art or a CedarVE
 * decode target) into console-native tile data:
 *
 *   1. Background-color detection (sheet_init: bg = argb[0])
 *   2. Palette extraction — up to N non-background colors + transparent
 *   3. Nearest-palette quantization (squared RGB distance)
 *   4. Integer nearest-neighbor up-scaling
 *   5. Cutting rectangles into NES / GB / SNES / Genesis tile formats
 *
 * Tile byte layouts (bit-exact with lib/nes.c, lib/gb.c, lib/snes.c,
 * lib/genesis.c — see sheet_fmt_t below).
 */
#ifndef SHEET_H
#define SHEET_H

#include <stdint.h>

/* Sprite sheet descriptor */
typedef struct {
    const uint32_t *argb;   /* sheet pixels, ARGB8888, row-major   */
    uint32_t w, h;          /* sheet dimensions in pixels          */
    uint32_t bg;            /* background color (auto: argb[0])    */
    int      bg_thresh;     /* background match threshold (squared
                             * RGB distance < thresh^2 snaps to
                             * index 0). Default 40. Set <= 0 to
                             * disable background/near-black
                             * snapping entirely — every pixel then
                             * goes through the nearest-color
                             * search, transparent slot included.  */
} sheet_t;

/*
 * Tile output formats. Pixel values are palette indices (0 =
 * transparent for sprites). "row r" = tile row 0-7, "col c" = pixel
 * column 0-7 left to right.
 *
 *   SHEET_FMT_NES_2BPP        16 B/tile — two 8-byte bitplanes:
 *                             byte[r]   = plane 0 (bit 7-c = col c)
 *                             byte[r+8] = plane 1
 *                             (lib/nes.c CHR layout)
 *
 *   SHEET_FMT_GB_2BPP         16 B/tile — planes interleaved per row:
 *                             byte[r*2]   = plane 0 (low)
 *                             byte[r*2+1] = plane 1 (high)
 *                             (lib/gb.c CHR layout)
 *
 *   SHEET_FMT_SNES_4BPP       32 B/tile — planar, sprite path:
 *                             byte[r*2]    = plane 0
 *                             byte[r*2+1]  = plane 1
 *                             byte[r*2+16] = plane 2
 *                             byte[r*2+17] = plane 3
 *                             (lib/snes.c sprite CHR layout)
 *
 *   SHEET_FMT_SNES_4BPP_PACKED 32 B/tile — packed nibbles, BG path:
 *                             byte[r*4+c/2], high nibble = left pixel
 *                             (what lib/snes.c's BG compositor reads)
 *
 *   SHEET_FMT_GEN_4BPP        32 B/tile — packed nibbles:
 *                             byte[r*4+c/2], high nibble = left pixel
 *                             (lib/genesis.c VRAM tile layout)
 */
typedef enum {
    SHEET_FMT_NES_2BPP,         /* 16B: plane0[8] plane1[8]           */
    SHEET_FMT_GB_2BPP,          /* 16B: per-row lo,hi interleaved     */
    SHEET_FMT_SNES_4BPP,        /* 32B planar (sprite path)           */
    SHEET_FMT_SNES_4BPP_PACKED, /* 32B packed nibbles (BG path)       */
    SHEET_FMT_GEN_4BPP,         /* 32B packed nibbles, hi nibble left */
} sheet_fmt_t;

/* Bytes per tile for a format (16 for 2bpp, 32 for 4bpp) */
int sheet_tile_bytes(sheet_fmt_t fmt);

/* Initialize a sheet descriptor: bg = argb[0], bg_thresh = 40. */
void sheet_init(sheet_t *s, const uint32_t *argb, uint32_t w, uint32_t h);

/*
 * Extract up to max_colors distinct non-background colors from the
 * rect (x,y,w,h) into pal[pal_off+1..]; pal[pal_off] is set to
 * 0x00000000 (transparent). Pixels near the background color or
 * near-black (RGB < 0x080808) are skipped. May be called repeatedly
 * on the same palette to accumulate colors from several rects.
 * Returns the palette size including the transparent entry.
 */
int  sheet_extract_palette(const sheet_t *s, int x, int y, int w, int h,
                           uint32_t *pal, int pal_off, int max_colors);

/*
 * Quantize one ARGB pixel to a palette index (0..pal_size-1, relative
 * to pal[pal_off]). With bg_thresh > 0, background-near and near-black
 * pixels snap to 0; otherwise nearest color by squared RGB distance
 * over all pal_size entries (transparent slot included).
 */
uint8_t sheet_pal_index(const sheet_t *s, uint32_t c,
                        const uint32_t *pal, int pal_off, int pal_size);

/*
 * Cut rect (x,y,w,h) of the sheet, up-scaled by integer `scale`
 * (nearest neighbor), into 8x8 tiles of format `fmt`. The tile grid is
 * ((w*scale+7)/8) x ((h*scale+7)/8); pixels beyond the scaled rect
 * quantize to 0. col_major: 0 = row-major tile order (NES/GB),
 * 1 = column-major (SNES/Genesis sprite storage, index = tx*th + ty).
 * Returns the number of tiles written (capped at max_tiles).
 */
int sheet_cut(const sheet_t *s, int x, int y, int w, int h, int scale,
              sheet_fmt_t fmt, int col_major,
              const uint32_t *pal, int pal_off, int pal_size,
              uint8_t *out_tiles, int max_tiles);

/*
 * Metasprite chunk descriptor (Genesis-style: hardware sprites are at
 * most 4x4 tiles, so a big character becomes a grid of chunks).
 * Coordinates are in tiles relative to the cut rect.
 */
typedef struct {
    int tile_base;      /* first tile index of this chunk's block  */
    int cx, cy;         /* chunk origin within the rect, in tiles  */
    int cw, ch;         /* chunk size in tiles (1..4)              */
} sheet_chunk_t;

/*
 * Cut rect (x,y,w,h) scaled by `scale` into <=4x4-tile chunks, storing
 * each chunk's tiles as a contiguous column-major block (stride = ch)
 * so a renderer addressing tile_base + tx * h + ty matches the
 * storage. Tiles land at tiles[(tile_base + n) * bytes-per-tile] —
 * pass the running tile counter as tile_base to accumulate several
 * frames into one bank (Genesis convention: tile 0 = transparent).
 * Chunks are emitted column-of-chunks first (cx outer, cy inner).
 * Stops early when max_tiles or max_chunks is reached. Returns the
 * number of chunks written; *tiles_used (may be NULL) receives the
 * number of tiles consumed.
 */
int sheet_cut_chunks(const sheet_t *s, int x, int y, int w, int h, int scale,
                     sheet_fmt_t fmt,
                     const uint32_t *pal, int pal_off, int pal_size,
                     uint8_t *tiles, int tile_base, int max_tiles,
                     sheet_chunk_t *chunks, int max_chunks,
                     int *tiles_used);

#endif
