/*
 * Mercury — HSTX DVI stream generator (RP2350)
 *
 * Pure C, no hardware access: produces the sequence of DMA segments
 * that feed the RP2350 HSTX command expander for 640x480p60 DVI, with
 * the Mercury framebuffer (w x h, R3G3B2) centered — pillarbox pads
 * left/right, letterbox lines top/bottom.
 *
 * hstx_dvi_out.c owns the hardware and calls hstx_stream_next() from
 * its DMA IRQ; mercury/test/ walks the same stream on a PC and decodes
 * it with the expander's semantics to prove every line is exactly 800
 * symbol periods with sync in the right place.
 */
#ifndef HSTX_DVI_STREAM_H
#define HSTX_DVI_STREAM_H

#include <stdint.h>

/* 640x480p60 timing (CEA-861 / pico-examples dvi_out_hstx_encoder) */
#define HSTX_H_FRONT_PORCH   16
#define HSTX_H_SYNC_WIDTH    96
#define HSTX_H_BACK_PORCH    48
#define HSTX_H_ACTIVE        640
#define HSTX_H_TOTAL         (HSTX_H_FRONT_PORCH + HSTX_H_SYNC_WIDTH + \
                              HSTX_H_BACK_PORCH + HSTX_H_ACTIVE)   /* 800 */
#define HSTX_V_FRONT_PORCH   10
#define HSTX_V_SYNC_WIDTH    2
#define HSTX_V_BACK_PORCH    33
#define HSTX_V_ACTIVE        480
#define HSTX_V_BLANK         (HSTX_V_FRONT_PORCH + HSTX_V_SYNC_WIDTH + \
                              HSTX_V_BACK_PORCH)                   /* 45 */
#define HSTX_V_TOTAL         (HSTX_V_BLANK + HSTX_V_ACTIVE)        /* 525 */

/* HSTX command expander opcodes (RP2350 datasheet, HSTX chapter) */
#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)

/* TMDS control symbols, 3 lanes packed 10 bits apart. Lane 0 carries
 * HSYNC (bit 0) / VSYNC (bit 1); lanes 1-2 send CTRL_00. Polarity is
 * active-low for 640x480 — so "V1H1" is the idle (no sync) state. */
#define HSTX_TMDS_CTRL_00 0x354u
#define HSTX_TMDS_CTRL_01 0x0abu
#define HSTX_TMDS_CTRL_10 0x154u
#define HSTX_TMDS_CTRL_11 0x2abu
#define HSTX_SYNC_V0_H0 (HSTX_TMDS_CTRL_00 | (HSTX_TMDS_CTRL_00 << 10) | (HSTX_TMDS_CTRL_00 << 20))
#define HSTX_SYNC_V0_H1 (HSTX_TMDS_CTRL_01 | (HSTX_TMDS_CTRL_00 << 10) | (HSTX_TMDS_CTRL_00 << 20))
#define HSTX_SYNC_V1_H0 (HSTX_TMDS_CTRL_10 | (HSTX_TMDS_CTRL_00 << 10) | (HSTX_TMDS_CTRL_00 << 20))
#define HSTX_SYNC_V1_H1 (HSTX_TMDS_CTRL_11 | (HSTX_TMDS_CTRL_00 << 10) | (HSTX_TMDS_CTRL_00 << 20))

/* 8bpp: the expander pops 4 pixels per 32-bit FIFO word */
#define HSTX_PIX_PER_WORD    4
/* Minimum DMA segment length: keep every segment >= the 8-entry HSTX
 * FIFO so the ping-pong DMA can't outrun the IRQ that reloads it. */
#define HSTX_MIN_SEG_WORDS   8

typedef struct hstx_stream hstx_stream_t;

/* Called once per frame, from inside hstx_stream_next() when line 0 is
 * scheduled (vertical blanking — tear-free point). Must set fb/w/h. */
typedef void (*hstx_frame_start_fn)(hstx_stream_t *s);

struct hstx_stream {
    /* latched each frame by frame_start */
    const uint8_t *fb;          /* w*h bytes, R3G3B2, 4-byte aligned */
    uint32_t w, h;              /* w multiple of 4, 32..640; h <= 478 */
    /* position */
    uint32_t line;              /* 0..HSTX_V_TOTAL-1 (0 = first blank) */
    uint32_t seg;               /* segment index within an image line */
    /* derived per frame */
    uint32_t pad_l, pad_r, y_off;
    uint32_t built_w;           /* width the head/tail lists match */
    hstx_frame_start_fn frame_start;
    void *user;
};

/* One stream instance only — the command lists are file-static. */
void hstx_stream_init(hstx_stream_t *s, hstx_frame_start_fn fs, void *user);

/* Produce the next DMA segment: *addr / *words (32-bit words). */
void hstx_stream_next(hstx_stream_t *s, const uint32_t **addr,
                      uint32_t *words);

#endif
