/*
 * Host test for hstx_dvi_stream.c — the RP2350 HSTX scanout stream.
 *
 * Feeds the generated DMA segments, concatenated exactly as the
 * ping-pong DMA would deliver them, into a model of the HSTX command
 * expander:
 *   RAW n         n data words, one symbol period each
 *   RAW_REPEAT n  1 data word, repeated for n periods
 *   TMDS n        n pixels from ceil(n/4) words, low byte first
 *   NOP           no data, no output
 * then slices the output into 800-period lines and checks every period
 * of several frames: porch/sync placement and polarity, vsync lines,
 * letterbox/pillarbox black, and every framebuffer pixel in place —
 * across a buffer swap and a mid-run resolution change.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hstx_dvi_stream.h"

#define FB_MAX (320 * 224)
static uint8_t fbs[2][FB_MAX] __attribute__((aligned(4)));
static uint8_t fb160[2][160 * 144] __attribute__((aligned(4)));
static int frames_started;
static uint32_t frame_w[8], frame_h[8];
static int fails;

#define FAIL(...) do { if (fails < 12) { printf("FAIL: " __VA_ARGS__); printf("\n"); } fails++; } while (0)

/* frame plan: frames 0-1 320x224 (alternate buffers), 2+ 160x144 */
static void frame_start(hstx_stream_t *s)
{
    int f = frames_started++;
    s->fb = (f < 2) ? fbs[f & 1] : fb160[f & 1];
    s->w  = (f < 2) ? 320 : 160;
    s->h  = (f < 2) ? 224 : 144;
    if (f < 8) { frame_w[f] = s->w; frame_h[f] = s->h; }
}

static uint8_t pattern(int buf, uint32_t x, uint32_t y)
{
    return (uint8_t)((x * 7 + y * 13 + buf * 101) | 1);  /* never 0 */
}

/* ---- expander model output: one entry per symbol period ---- */
typedef struct { uint8_t is_ctrl; uint32_t v; } period_t;
static period_t line[HSTX_H_TOTAL + 4096];
static uint32_t nper;

static int frame_of_line_check;   /* which frame the line belongs to */
static uint32_t cur_line;         /* 0..524 within frame */
static int lines_checked;

static void check_line(void)
{
    if (nper != HSTX_H_TOTAL) {
        FAIL("frame %d line %u has %u periods", frame_of_line_check, cur_line, nper);
        return;
    }
    int f = frame_of_line_check;
    uint32_t L = cur_line;
    int vsync = (L >= HSTX_V_FRONT_PORCH &&
                 L <  HSTX_V_FRONT_PORCH + HSTX_V_SYNC_WIDTH);
    uint32_t want_v = vsync ? 0 : 1;

    for (uint32_t p = 0; p < HSTX_H_TOTAL; p++) {
        int in_sync = p >= HSTX_H_FRONT_PORCH &&
                      p <  HSTX_H_FRONT_PORCH + HSTX_H_SYNC_WIDTH;
        int in_active = p >= HSTX_H_TOTAL - HSTX_H_ACTIVE;
        if (!in_active || L < HSTX_V_BLANK) {
            if (!line[p].is_ctrl) { FAIL("f%d L%u p%u: pixel in blanking", f, L, p); return; }
            uint32_t lane0 = line[p].v & 0x3FF, lane12 = line[p].v >> 10;
            uint32_t hv;
            switch (lane0) {
            case HSTX_TMDS_CTRL_00: hv = 0; break;   /* V0 H0 */
            case HSTX_TMDS_CTRL_01: hv = 1; break;   /* V0 H1 */
            case HSTX_TMDS_CTRL_10: hv = 2; break;   /* V1 H0 */
            case HSTX_TMDS_CTRL_11: hv = 3; break;   /* V1 H1 */
            default: FAIL("f%d L%u p%u: bad ctrl symbol", f, L, p); return;
            }
            if (lane12 != (HSTX_TMDS_CTRL_00 | (HSTX_TMDS_CTRL_00 << 10))) {
                FAIL("f%d L%u p%u: lanes 1-2 not CTRL_00", f, L, p); return;
            }
            uint32_t h = hv & 1, v = hv >> 1;
            if (h != (in_sync ? 0u : 1u)) { FAIL("f%d L%u p%u: hsync wrong", f, L, p); return; }
            if (v != want_v)              { FAIL("f%d L%u p%u: vsync wrong", f, L, p); return; }
        } else {
            if (line[p].is_ctrl) { FAIL("f%d L%u p%u: ctrl in active", f, L, p); return; }
            uint32_t x = p - (HSTX_H_TOTAL - HSTX_H_ACTIVE);
            uint32_t row = L - HSTX_V_BLANK;
            uint32_t w = frame_w[f], h = frame_h[f];
            uint32_t pad_l = ((HSTX_H_ACTIVE - w) / 2) & ~3u;
            uint32_t y_off = (HSTX_V_ACTIVE - h) / 2;
            uint8_t want = 0;
            if (row >= y_off && row < y_off + h && x >= pad_l && x < pad_l + w)
                want = pattern(f & 1, x - pad_l, row - y_off);
            if (line[p].v != want) {
                FAIL("f%d L%u x%u: pixel %02x want %02x", f, L, x, line[p].v, want);
                return;
            }
        }
    }
    lines_checked++;
}

static void emit(uint8_t is_ctrl, uint32_t v)
{
    line[nper].is_ctrl = is_ctrl;
    line[nper].v = v;
    if (++nper == HSTX_H_TOTAL) {
        check_line();
        nper = 0;
        if (++cur_line == HSTX_V_TOTAL) { cur_line = 0; frame_of_line_check++; }
    }
}

/* expander state machine */
static uint32_t cmd_op, cmd_left;
static int in_cmd;

static void feed(uint32_t w)
{
    if (!in_cmd) {
        cmd_op = (w >> 12) & 0xF;
        cmd_left = w & 0xFFF;
        if (cmd_op == 0xF) return;                       /* NOP */
        if (cmd_op != 0x0 && cmd_op != 0x1 && cmd_op != 0x2) {
            FAIL("unexpected opcode %x", cmd_op); return;
        }
        if (cmd_op == 0x2 && (cmd_left & 3)) FAIL("TMDS count %u not /4", cmd_left);
        in_cmd = cmd_left > 0;
        return;
    }
    switch (cmd_op) {
    case 0x0: emit(1, w); cmd_left--; break;             /* RAW */
    case 0x1: while (cmd_left) { emit(1, w); cmd_left--; } break; /* RAW_REPEAT */
    case 0x2:                                            /* TMDS */
        for (int i = 0; i < 4 && cmd_left; i++, cmd_left--)
            emit(0, (w >> (8 * i)) & 0xFF);
        break;
    }
    if (!cmd_left) in_cmd = 0;
}

int main(void)
{
    for (int b = 0; b < 2; b++)
        for (uint32_t y = 0; y < 224; y++)
            for (uint32_t x = 0; x < 320; x++)
                fbs[b][y * 320 + x] = pattern(b, x, y);
    /* frames 2+ are 160x144 at stride 160 */
    for (int b = 0; b < 2; b++)
        for (uint32_t y = 0; y < 144; y++)
            for (uint32_t x = 0; x < 160; x++)
                fb160[b][y * 160 + x] = pattern(b, x, y);

    hstx_stream_t s;
    hstx_stream_init(&s, frame_start, NULL);

    int segs = 0, short_segs = 0;
    const int FRAMES = 4;
    while (frame_of_line_check < FRAMES && segs < 2000000) {
        const uint32_t *addr; uint32_t words;
        hstx_stream_next(&s, &addr, &words);
        segs++;
        if (words < HSTX_MIN_SEG_WORDS) short_segs++;
        for (uint32_t i = 0; i < words; i++) feed(addr[i]);
    }

    printf("segments: %d   frames latched: %d   lines checked: %d / %d\n",
           segs, frames_started, lines_checked, FRAMES * HSTX_V_TOTAL);
    if (short_segs) FAIL("%d segments shorter than %d words", short_segs, HSTX_MIN_SEG_WORDS);
    if (lines_checked != FRAMES * HSTX_V_TOTAL) FAIL("not every line checked");
    if (fails) { printf("FAILED: %d\n", fails); return 1; }
    printf("ALL PASS — every period of %d frames (320x224 x2 incl. buffer swap, "
           "then 160x144 after a resolution change)\n", FRAMES);
    return 0;
}
