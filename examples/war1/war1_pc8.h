/* Jupiter port: tiny runtime for reading .pc8 blobs + blitting tiles to FB.
 * Header-only — both the C main loop and the C++ bridge include this. */
#ifndef WAR1_PC8_H
#define WAR1_PC8_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* jupiter.h is a C header without its own extern "C" guards, so wrap the
 * include inside our extern "C" block so C-linkage functions stay C-linkage
 * when pc8.h is pulled into a .cpp file. */
#include "jupiter.h"

/* .pc8 v2: stores the raw 8-bit-indexed image plus a 256-entry ARGB
 * palette. Frame size is NOT baked into the blob — the caller (which
 * has Stratagus's CGraphic::Width/Height) supplies frame_w/frame_h
 * at blit time, and the blob is a sprite-sheet sliced in row-major order.
 *
 * Header (little-endian):
 *   u32 magic = 0x20384350 ("PC8 ")
 *   u16 image_w
 *   u16 image_h
 *   u16 version = 2
 *   u16 reserved = 0
 * then image_w * image_h bytes of pixel indices, then 256 * u32 palette.
 */
typedef struct {
    uint32_t magic;
    uint16_t image_w, image_h;
    uint16_t version, reserved;
} pc8_hdr_t;

typedef struct {
    uint16_t image_w, image_h;
    const uint8_t  *pixels;    /* image_w * image_h bytes */
    const uint32_t *palette;   /* 256 entries, ARGB8888 */
} pc8_t;

static inline int pc8_open(pc8_t *out, const uint8_t *data) {
    const pc8_hdr_t *h = (const pc8_hdr_t *)data;
    if (h->magic != 0x20384350u || h->version != 2) return -1;
    out->image_w = h->image_w;
    out->image_h = h->image_h;
    out->pixels = data + sizeof(pc8_hdr_t);
    out->palette = (const uint32_t *)(out->pixels +
                    (uint32_t)h->image_w * h->image_h);
    return 0;
}

/* Blit an arbitrary (gx,gy,w,h) sub-rectangle of the source image to
 * destination (px,py). Used by Stratagus HUD paths — DrawSubClip — which
 * pick pieces out of a panel sheet rather than indexing by frame. */
static inline void pc8_blit_subrect(const pc8_t *pc,
                                    int gx, int gy, int w, int h,
                                    int px, int py,
                                    uint32_t target_fb, int skip_zero) {
    if (w <= 0 || h <= 0) return;
    if (gx < 0) { w += gx; px -= gx; gx = 0; }
    if (gy < 0) { h += gy; py -= gy; gy = 0; }
    if (gx + w > (int)pc->image_w) w = (int)pc->image_w - gx;
    if (gy + h > (int)pc->image_h) h = (int)pc->image_h - gy;
    if (w <= 0 || h <= 0) return;
    uint32_t *fb = (uint32_t *)target_fb;
    for (int y = 0; y < h; y++) {
        int dy = py + y;
        if (dy < 0 || dy >= (int)LCD_H) continue;
        uint32_t *row = fb + dy * LCD_W;
        const uint8_t *src = pc->pixels + (uint32_t)(gy + y) * pc->image_w + gx;
        for (int x = 0; x < w; x++) {
            int dx = px + x;
            if (dx < 0 || dx >= (int)LCD_W) continue;
            uint8_t idx = src[x];
            uint32_t argb = pc->palette[idx];
            if (skip_zero && (argb >> 24) == 0) continue;
            row[dx] = argb;
        }
    }
}

/* Blit one frame, sliced row-major at frame_w × frame_h from the image. */
static inline void pc8_blit_frame(const pc8_t *pc,
                                  int frame_w, int frame_h, uint32_t frame_idx,
                                  int px, int py, uint32_t target_fb, int skip_zero) {
    if (frame_w <= 0 || frame_h <= 0) return;
    int cols = pc->image_w / frame_w;
    if (cols <= 0) return;
    int fc = (int)(frame_idx) % cols;
    int fr = (int)(frame_idx) / cols;
    int src_x0 = fc * frame_w;
    int src_y0 = fr * frame_h;
    if (src_y0 + frame_h > pc->image_h) return;
    uint32_t *fb = (uint32_t *)target_fb;
    for (int y = 0; y < frame_h; y++) {
        int dy = py + y;
        if (dy < 0 || dy >= (int)LCD_H) continue;
        uint32_t *row = fb + dy * LCD_W;
        const uint8_t *src = pc->pixels + (uint32_t)(src_y0 + y) * pc->image_w + src_x0;
        for (int x = 0; x < frame_w; x++) {
            int dx = px + x;
            if (dx < 0 || dx >= (int)LCD_W) continue;
            uint8_t idx = src[x];
            uint32_t argb = pc->palette[idx];
            /* skip_zero actually means "skip fully-transparent pixels".
             * War1gus sprite sheets mark cell-background with alpha=0 in
             * the palette (set by war1_convert.py for idx 0 OR any tRNS
             * entry). Checking the alpha channel handles both cases. */
            if (skip_zero && (argb >> 24) == 0) continue;
            row[dx] = argb;
        }
    }
}

/* Blit one frame mirrored horizontally — used for west-facing unit
 * animations. War1gus sprite sheets only contain east-facing frames;
 * Stratagus's CGraphic::DrawFrameClipX (and the player-color variant)
 * are called with the mirror flag for every west-facing direction
 * (W, NW, SW). Same indexing as pc8_blit_frame, but reads source
 * pixels right-to-left within each row. */
static inline void pc8_blit_frame_flipx(const pc8_t *pc,
                                        int frame_w, int frame_h, uint32_t frame_idx,
                                        int px, int py, uint32_t target_fb, int skip_zero) {
    if (frame_w <= 0 || frame_h <= 0) return;
    int cols = pc->image_w / frame_w;
    if (cols <= 0) return;
    int fc = (int)(frame_idx) % cols;
    int fr = (int)(frame_idx) / cols;
    int src_x0 = fc * frame_w;
    int src_y0 = fr * frame_h;
    if (src_y0 + frame_h > pc->image_h) return;
    uint32_t *fb = (uint32_t *)target_fb;
    for (int y = 0; y < frame_h; y++) {
        int dy = py + y;
        if (dy < 0 || dy >= (int)LCD_H) continue;
        uint32_t *row = fb + dy * LCD_W;
        const uint8_t *src = pc->pixels + (uint32_t)(src_y0 + y) * pc->image_w + src_x0;
        for (int x = 0; x < frame_w; x++) {
            int dx = px + x;
            if (dx < 0 || dx >= (int)LCD_W) continue;
            /* Mirror: source pixel comes from the opposite side of the frame. */
            uint8_t idx = src[frame_w - 1 - x];
            uint32_t argb = pc->palette[idx];
            if (skip_zero && (argb >> 24) == 0) continue;
            row[dx] = argb;
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* WAR1_PC8_H */
