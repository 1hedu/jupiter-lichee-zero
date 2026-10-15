/*
 * Mercury — HSTX DVI stream generator (RP2350). See hstx_dvi_stream.h.
 *
 * Stream layout, one line = [front porch][sync][back porch][active]:
 *
 *   lines 0-9     vblank, VSYNC idle       1 segment
 *   lines 10-11   vblank, VSYNC asserted   1 segment
 *   lines 12-44   vblank, VSYNC idle       1 segment
 *   lines 45-524  active area (480 lines):
 *     letterbox rows                       1 segment: hblank + 640 black
 *     image rows                           2-3 segments:
 *        head  = hblank + [TMDS pad_l + zeros] + TMDS w   (command list)
 *        row   = w/4 words straight from the framebuffer
 *        tail  = TMDS pad_r + zeros (+NOP fill)           (if pad_r)
 *
 * Pads are explicit zero-pixel TMDS runs rather than TMDS_REPEAT so
 * every opcode used here has the exact semantics the official
 * pico-examples HSTX code proves on silicon (TMDS count = pixels,
 * 4 pixels per FIFO word at 8bpp).
 */
#include <stddef.h>
#include "hstx_dvi_stream.h"

#ifndef __not_in_flash_func
#define __not_in_flash_func(f) f    /* host build */
#endif

#define HBLANK_WORDS 8

/* DMA-readable command lists: deliberately non-const so they live in
 * SRAM (flash/XIP reads would stall the DMA behind cache misses). */
static uint32_t vblank_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | HSTX_H_FRONT_PORCH, HSTX_SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | HSTX_H_SYNC_WIDTH,  HSTX_SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (HSTX_H_BACK_PORCH + HSTX_H_ACTIVE), HSTX_SYNC_V1_H1,
    HSTX_CMD_NOP, HSTX_CMD_NOP,
};
static uint32_t vblank_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | HSTX_H_FRONT_PORCH, HSTX_SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | HSTX_H_SYNC_WIDTH,  HSTX_SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (HSTX_H_BACK_PORCH + HSTX_H_ACTIVE), HSTX_SYNC_V0_H1,
    HSTX_CMD_NOP, HSTX_CMD_NOP,
};

/* hblank prefix shared by every active-area line — exactly
 * HBLANK_WORDS (8) words, matching pico-examples' vactive_line. The
 * line_black / line_head sizing depends on this count; the host test
 * catches any drift (an extra word shifts every TMDS run by one). */
#define HBLANK_PREFIX                                       \
    HSTX_CMD_RAW_REPEAT | HSTX_H_FRONT_PORCH, HSTX_SYNC_V1_H1, \
    HSTX_CMD_NOP,                                           \
    HSTX_CMD_RAW_REPEAT | HSTX_H_SYNC_WIDTH,  HSTX_SYNC_V1_H0, \
    HSTX_CMD_NOP,                                           \
    HSTX_CMD_RAW_REPEAT | HSTX_H_BACK_PORCH,  HSTX_SYNC_V1_H1

/* letterbox line: hblank + 640 black pixels (zero-initialised tail) */
static uint32_t line_black[HBLANK_WORDS + 1 + HSTX_H_ACTIVE / HSTX_PIX_PER_WORD] = {
    HBLANK_PREFIX,
    HSTX_CMD_TMDS | HSTX_H_ACTIVE,
};

/* image-line head and tail, rebuilt when the width changes */
static uint32_t line_head[HBLANK_WORDS + 1 + HSTX_H_ACTIVE / HSTX_PIX_PER_WORD + 1];
static uint32_t line_head_words;
static uint32_t line_tail[1 + HSTX_H_ACTIVE / HSTX_PIX_PER_WORD + HSTX_MIN_SEG_WORDS];
static uint32_t line_tail_words;

static void build_lists(hstx_stream_t *s)
{
    static const uint32_t prefix[HBLANK_WORDS] = { HBLANK_PREFIX };
    uint32_t n = 0;

    for (uint32_t i = 0; i < HBLANK_WORDS; i++)
        line_head[n++] = prefix[i];
    if (s->pad_l) {
        line_head[n++] = HSTX_CMD_TMDS | s->pad_l;
        for (uint32_t i = 0; i < s->pad_l / HSTX_PIX_PER_WORD; i++)
            line_head[n++] = 0;                     /* black */
    }
    line_head[n++] = HSTX_CMD_TMDS | s->w;          /* pixels follow */
    line_head_words = n;

    n = 0;
    if (s->pad_r) {
        line_tail[n++] = HSTX_CMD_TMDS | s->pad_r;
        for (uint32_t i = 0; i < s->pad_r / HSTX_PIX_PER_WORD; i++)
            line_tail[n++] = 0;
        while (n < HSTX_MIN_SEG_WORDS)
            line_tail[n++] = HSTX_CMD_NOP;
    }
    line_tail_words = n;
    s->built_w = s->w;
}

static void latch_frame(hstx_stream_t *s)
{
    if (s->frame_start)
        s->frame_start(s);

    /* Sanitize: width a multiple of 4 in [32, 640] (DMA words, and the
     * minimum segment length), height leaving >= 1 letterbox line
     * (keeps head/tail idle when they're rebuilt here at line 0). */
    uint32_t w = s->w & ~3u, h = s->h;
    if (w > HSTX_H_ACTIVE) w = HSTX_H_ACTIVE;
    if (h > HSTX_V_ACTIVE - 2) h = HSTX_V_ACTIVE - 2;
    if (w < HSTX_MIN_SEG_WORDS * HSTX_PIX_PER_WORD || !s->fb) h = 0;
    s->w = w;
    s->h = h;

    s->pad_l = ((HSTX_H_ACTIVE - w) / 2) & ~3u;
    s->pad_r = HSTX_H_ACTIVE - w - s->pad_l;    /* also a multiple of 4 */
    s->y_off = (HSTX_V_ACTIVE - h) / 2;
    if (h && w != s->built_w)
        build_lists(s);
}

static inline void advance_line(hstx_stream_t *s)
{
    s->seg = 0;
    if (++s->line >= HSTX_V_TOTAL)
        s->line = 0;
}

void hstx_stream_init(hstx_stream_t *s, hstx_frame_start_fn fs, void *user)
{
    s->fb = NULL;
    s->w = s->h = 0;
    s->line = 0;
    s->seg = 0;
    s->pad_l = s->pad_r = s->y_off = 0;
    s->built_w = 0;
    s->frame_start = fs;
    s->user = user;
}

void __not_in_flash_func(hstx_stream_next)(hstx_stream_t *s,
                                           const uint32_t **addr,
                                           uint32_t *words)
{
    if (s->line == 0 && s->seg == 0)
        latch_frame(s);

    uint32_t line = s->line;

    if (line < HSTX_V_BLANK) {
        int vsync = line >= HSTX_V_FRONT_PORCH &&
                    line <  HSTX_V_FRONT_PORCH + HSTX_V_SYNC_WIDTH;
        *addr  = vsync ? vblank_vsync_on : vblank_vsync_off;
        *words = sizeof(vblank_vsync_off) / sizeof(uint32_t);
        advance_line(s);
        return;
    }

    uint32_t row = line - HSTX_V_BLANK;
    if (row < s->y_off || row >= s->y_off + s->h) {
        *addr  = line_black;
        *words = sizeof(line_black) / sizeof(uint32_t);
        advance_line(s);
        return;
    }

    switch (s->seg) {
    case 0:
        *addr  = line_head;
        *words = line_head_words;
        s->seg = 1;
        break;
    case 1:
        *addr  = (const uint32_t *)(s->fb + (row - s->y_off) * s->w);
        *words = s->w / HSTX_PIX_PER_WORD;
        if (line_tail_words) s->seg = 2;
        else                 advance_line(s);
        break;
    default:
        *addr  = line_tail;
        *words = line_tail_words;
        advance_line(s);
        break;
    }
}
