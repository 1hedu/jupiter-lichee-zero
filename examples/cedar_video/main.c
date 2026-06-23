/*
 * Jupiter SDK — H.264 video playback via CedarVE, with live N64-pad
 * tuning of every cedar_h264_decode() parameter.
 *
 * Loads a .vbin container produced by scripts/wc1_video_pack.py
 * (sequence of all-IDR baseline H.264 frames) and plays it back, while
 * letting the user dial in each decoder parameter on the fly to find
 * the values that produce a clean image.
 *
 * Controls (N64 pad):
 *   D-pad LEFT / RIGHT   : hdr_bits      -1 / +1
 *   D-pad UP   / DOWN    : hdr_bits      -8 / +8   (coarse)
 *   C-Left / C-Right     : pic_init_qp   -1 / +1
 *   C-Up   / C-Down      : slice_qp_delta -1 / +1
 *   L                    : chroma_qp_off -1
 *   R                    : chroma_qp_off +1
 *   Z                    : toggle disable_deblock
 *   A                    : pause / play
 *   B                    : restart from frame 0
 *   START                : dump current settings to UART + freeze frame
 *
 * Build: make GAME=examples/cedar_video/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include <stddef.h>

#define D(s) uart_puts("[ex] " s "\n")

extern const uint8_t _binary_build_title_vbin_start[];
extern const uint8_t _binary_build_title_vbin_end[];

/* ---- .vbin header (matches scripts/wc1_video_pack.py) ---- */
typedef struct __attribute__((packed)) {
    char     magic[4];
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint16_t fps_num;
    uint16_t fps_den;
    uint16_t hdr_bits;
    uint32_t qp;
    uint32_t num_frames;
    uint32_t total_size;
    uint8_t  reserved[4];
} vbin_hdr_t;

#define HDR_BITS_OVERRIDE  32
#define LUMA_ADDR    0x43100000
#define CHROMA_ADDR  0x43200000

/* ============================================================
 * Tiny 5x7 column-encoded font for the status overlay
 * ============================================================ */
#define FNT_W 5
#define FNT_H 7
typedef struct { char ch; uint8_t cols[FNT_W]; } glyph_t;
static const glyph_t g_font[] = {
    {' ', {0,0,0,0,0}},
    {'A', {0x7E,0x09,0x09,0x09,0x7E}},  {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},  {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},  {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},  {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},  {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},  {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},  {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},  {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}},  {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},  {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},  {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x7F,0x20,0x18,0x20,0x7F}},  {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}},  {'Z', {0x61,0x51,0x49,0x45,0x43}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},  {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},  {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},  {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},  {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},  {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},  {':', {0x00,0x24,0x00,0x00,0x00}},
    {'.', {0x00,0x40,0x00,0x00,0x00}},  {'/', {0x20,0x10,0x08,0x04,0x02}},
    {'+', {0x08,0x08,0x3E,0x08,0x08}},  {'_', {0x40,0x40,0x40,0x40,0x40}},
    {'(', {0x00,0x1C,0x22,0x41,0x00}},  {')', {0x00,0x41,0x22,0x1C,0x00}},
    {'=', {0x14,0x14,0x14,0x14,0x14}},  {',', {0x80,0x60,0x00,0x00,0x00}},
};
#define FONT_N (sizeof(g_font) / sizeof(g_font[0]))

static const uint8_t *glyph_for(char c) {
    if (c >= 'a' && c <= 'z') c -= 0x20;   /* uppercase fallback */
    for (uint32_t i = 0; i < FONT_N; i++) if (g_font[i].ch == c) return g_font[i].cols;
    return g_font[0].cols;
}

static void draw_text(uint32_t *fb, const char *s, int x, int y, uint32_t color, uint32_t bg) {
    while (*s) {
        const uint8_t *g = glyph_for(*s);
        for (int col = 0; col < FNT_W + 1; col++) {
            uint8_t bits = (col < FNT_W) ? g[col] : 0;
            for (int row = 0; row < FNT_H + 1; row++) {
                int py = y + row;
                if (py < 0 || py >= (int)LCD_H) continue;
                int px = x + col;
                if (px < 0 || px >= (int)LCD_W) continue;
                uint32_t c = (row < FNT_H && (bits & (1 << row))) ? color : bg;
                fb[py * LCD_W + px] = c;
            }
        }
        x += FNT_W + 1;
        s++;
    }
}

/* Format a signed int into a buffer at *pp. Advances *pp past the digits. */
static void fmt_int(char **pp, int v) {
    char *p = *pp;
    if (v < 0) { *p++ = '-'; v = -v; }
    char tmp[16];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = (char)('0' + v % 10); v /= 10; } }
    while (n > 0) *p++ = tmp[--n];
    *pp = p;
}

static void fmt_str(char **pp, const char *s) {
    char *p = *pp;
    while (*s) *p++ = *s++;
    *pp = p;
}

/* ============================================================
 * Status overlay: writes into the back buffer AFTER NV12->ARGB
 * ============================================================ */
typedef struct {
    int hdr_bits;
    int qp;
    int qp_delta;
    int chroma_off;
    int deblock;        /* disable_deblock value: 0=filter on, 1=filter off */
    int paused;
    uint32_t frame_idx;
    uint32_t total_frames;
} cfg_t;

static void overlay_status(uint32_t *fb, const cfg_t *c) {
    /* Black bar across the top with 4 lines of params. */
    const uint32_t bg  = 0xC0000000;
    const uint32_t fg  = 0xFFFFFF00;
    const uint32_t fg2 = 0xFFA0FFE0;
    const uint32_t fp  = 0xFFFF8080;

    /* Top bar height = 4 lines × (FNT_H+1=8) = 32 px. */
    for (int y = 0; y < 32; y++) {
        uint32_t *row = fb + y * LCD_W;
        for (int x = 0; x < (int)LCD_W; x++) row[x] = bg;
    }

    char buf[96];
    char *p;

    p = buf; fmt_str(&p, "HDR_BITS:"); fmt_int(&p, c->hdr_bits);
    *p = 0;  draw_text(fb, buf, 4, 0, fg, bg);

    p = buf; fmt_str(&p, "QP:"); fmt_int(&p, c->qp);
    fmt_str(&p, " QPD:"); fmt_int(&p, c->qp_delta);
    fmt_str(&p, " CHR:"); fmt_int(&p, c->chroma_off);
    fmt_str(&p, " DBLK_OFF:"); fmt_int(&p, c->deblock);
    *p = 0;  draw_text(fb, buf, 4, 8, fg2, bg);

    p = buf; fmt_str(&p, "FRAME:"); fmt_int(&p, c->frame_idx);
    fmt_str(&p, "/"); fmt_int(&p, c->total_frames);
    if (c->paused) fmt_str(&p, "  PAUSED");
    *p = 0;  draw_text(fb, buf, 4, 16, c->paused ? fp : fg, bg);

    p = buf;
    fmt_str(&p, "DPAD:HDR  C:QP/QPD  L/R:CHR  Z:DBLK  A:PAUSE  B:RESET  START:DUMP");
    *p = 0;  draw_text(fb, buf, 4, 24, 0xFF808080, bg);
}

static void clamp_int(int *v, int lo, int hi) {
    if (*v < lo) *v = lo;
    if (*v > hi) *v = hi;
}

static void dump_cfg_uart(const cfg_t *c) {
    uart_puts("[ex] CURRENT: hdr_bits="); uart_putdec((uint32_t)c->hdr_bits);
    uart_puts(" qp=");                   uart_putdec((uint32_t)c->qp);
    uart_puts(" qp_delta=");
    if (c->qp_delta < 0) { uart_putc('-'); uart_putdec((uint32_t)(-c->qp_delta)); }
    else                 { uart_putdec((uint32_t)c->qp_delta); }
    uart_puts(" chroma_off=");
    if (c->chroma_off < 0) { uart_putc('-'); uart_putdec((uint32_t)(-c->chroma_off)); }
    else                   { uart_putdec((uint32_t)c->chroma_off); }
    uart_puts(" disable_deblock=");      uart_putdec((uint32_t)c->deblock);
    uart_puts("\n");
}

/* ============================================================
 * Main
 * ============================================================ */
void main(void)
{
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();
    video_init();
    input_init(INPUT_N64);

    uart_puts("\n\n=== CedarVE H.264 video playback (N64 tuner) ===\n");

    memset32_neon(FB0_ADDR,  0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR,  0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(FB1_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    const vbin_hdr_t *hdr = (const vbin_hdr_t *)_binary_build_title_vbin_start;
    if (memcmp(hdr->magic, "VBIN", 4) != 0) {
        uart_puts("[ex] FATAL: bad magic in title.vbin\n");
        for (;;) {}
    }
    uart_puts("[ex] vbin OK: ");
    uart_putdec(hdr->width); uart_puts("x"); uart_putdec(hdr->height);
    uart_puts(" @ "); uart_putdec(hdr->fps_num); uart_puts("/");
    uart_putdec(hdr->fps_den); uart_puts(" fps  qp="); uart_putdec(hdr->qp);
    uart_puts("  frames="); uart_putdec(hdr->num_frames);
    uart_puts("  hdr_bits_in_file="); uart_putdec(hdr->hdr_bits);
    uart_puts("\n");

    cedar_init();

    /* Initial config — params start from .vbin defaults / safe values. */
    cfg_t cfg;
    cfg.hdr_bits     = (hdr->hdr_bits != 0) ? hdr->hdr_bits : HDR_BITS_OVERRIDE;
    cfg.qp           = (int)hdr->qp;
    cfg.qp_delta     = 0;
    cfg.chroma_off   = 0;
    cfg.deblock      = 0;       /* filter on */
    cfg.paused       = 0;
    cfg.frame_idx    = 0;
    cfg.total_frames = hdr->num_frames;

    /* Walk the frame entries. Each = uint32 nal_size + nal_size bytes. */
    const uint8_t *p_start = _binary_build_title_vbin_start + sizeof(vbin_hdr_t);

    int dst_x = ((int)LCD_W - (int)hdr->width) / 2;
    int dst_y = ((int)LCD_H - (int)hdr->height) / 2;
    if (dst_x < 0) dst_x = 0;
    if (dst_y < 0) dst_y = 0;

    uint32_t frame_us = (1000000u * hdr->fps_den) / hdr->fps_num;

    uint32_t fb_addrs[2] = { FB0_ADDR, FB1_ADDR };
    int fb_idx = 0;

    /* For paused mode: keep the same NAL ptr around to re-decode. */
    const uint8_t *cur_q   = p_start;     /* cursor into .vbin payload */
    uint32_t       cur_nal_size = 0;
    const uint8_t *cur_nal = NULL;
    int            need_decode = 1;       /* if 1, re-decode current frame */

    while (1) {
        uint32_t t_frame_start = timer_read();

        /* ---- Input poll: adjust cfg based on button presses ---- */
        input_poll();
        uint32_t pressed = input_pressed();   /* full 32-bit button mask */

        if (pressed & BTN_LEFT)    { cfg.hdr_bits   -= 1; need_decode = 1; }
        if (pressed & BTN_RIGHT)   { cfg.hdr_bits   += 1; need_decode = 1; }
        if (pressed & BTN_UP)      { cfg.hdr_bits   -= 8; need_decode = 1; }
        if (pressed & BTN_DOWN)    { cfg.hdr_bits   += 8; need_decode = 1; }
        if (pressed & BTN_C_LEFT)  { cfg.qp         -= 1; need_decode = 1; }
        if (pressed & BTN_C_RIGHT) { cfg.qp         += 1; need_decode = 1; }
        if (pressed & BTN_C_UP)    { cfg.qp_delta   -= 1; need_decode = 1; }
        if (pressed & BTN_C_DOWN)  { cfg.qp_delta   += 1; need_decode = 1; }
        if (pressed & BTN_L)       { cfg.chroma_off -= 1; need_decode = 1; }
        if (pressed & BTN_R)       { cfg.chroma_off += 1; need_decode = 1; }
        if (pressed & BTN_Z)       { cfg.deblock    ^= 1; need_decode = 1; }
        if (pressed & BTN_A)       { cfg.paused     ^= 1; }
        if (pressed & BTN_B)       { cfg.frame_idx = 0; cur_q = p_start; need_decode = 1; }
        if (pressed & BTN_START)   { dump_cfg_uart(&cfg); cfg.paused = 1; }

        clamp_int(&cfg.hdr_bits,   0, 200);
        clamp_int(&cfg.qp,         0, 51);
        clamp_int(&cfg.qp_delta,  -26, 25);
        clamp_int(&cfg.chroma_off, -12, 12);

        /* If not paused, advance to the next frame each iteration. */
        if (!cfg.paused || need_decode) {
            if (need_decode) {
                /* Re-decode the same frame we're currently on with new params. */
                /* cur_q is already pointing at the current frame's nal_size word. */
            }

            /* Read frame entry: uint32 nal_size, uint16 hdr_bits, uint16 reserved, NAL data. */
            memcpy(&cur_nal_size, cur_q, 4);
            uint16_t per_frame_hdr_bits = 0;
            memcpy(&per_frame_hdr_bits, cur_q + 4, 2);
            cur_nal = cur_q + 8;

            /* Stage NAL with start-code prefix. */
            #define NAL_STAGE 0x43F00000u
            *(volatile uint8_t *)(NAL_STAGE + 0) = 0x00;
            *(volatile uint8_t *)(NAL_STAGE + 1) = 0x00;
            *(volatile uint8_t *)(NAL_STAGE + 2) = 0x00;
            *(volatile uint8_t *)(NAL_STAGE + 3) = 0x01;
            for (uint32_t i = 0; i < cur_nal_size; i++) {
                *(volatile uint8_t *)(NAL_STAGE + 4 + i) = cur_nal[i];
            }
            dcache_clean_range(NAL_STAGE, cur_nal_size + 4);

            /* Use per-frame hdr_bits from the .vbin, falling back to
             * the manual cfg value if the file didn't store one (older
             * vbin format / 0 in the field). */
            uint32_t use_hdr_bits = per_frame_hdr_bits ? per_frame_hdr_bits
                                                       : (uint32_t)cfg.hdr_bits;
            int rc = cedar_h264_decode((const uint8_t *)NAL_STAGE,
                                       cur_nal_size + 4,
                                       hdr->width, hdr->height,
                                       use_hdr_bits,
                                       cfg.qp,
                                       cfg.qp_delta,
                                       cfg.chroma_off,
                                       cfg.deblock);
            if (rc != 0) {
                static int err = 0;
                if (err++ < 4) {
                    uart_puts("[ex] decode rc=");
                    if (rc < 0) { uart_putc('-'); uart_putdec((uint32_t)(-rc)); }
                    else        { uart_putdec((uint32_t)rc); }
                    uart_puts(" frame="); uart_putdec(cfg.frame_idx);
                    uart_puts("\n");
                }
            }

            uint32_t stride = (hdr->width + 15) & ~15;
            uint32_t mb_h   = ((hdr->height + 15) / 16) * 16;
            dcache_invalidate_range(LUMA_ADDR,   stride * mb_h);
            dcache_invalidate_range(CHROMA_ADDR, stride * mb_h / 2);

            /* Single-buffer: write directly to FB0 each frame. Double-
             * buffering produced "frame/black" alternation on this path
             * (something not landing on FB1's mapping?). For a static
             * render-then-display loop with full-frame writes, the
             * residual tearing is minor compared to cleanly seeing all
             * frames here. (void cast to silence unused warning.) */
            (void)fb_addrs; (void)fb_idx;
            uint32_t *vfb = (uint32_t *)FB0_ADDR;
            cedar_nv12_to_argb(vfb + dst_y * LCD_W + dst_x,
                               LCD_W, hdr->width, hdr->height);

            /* Status overlay on top of the decoded frame. */
            overlay_status(vfb, &cfg);

            dcache_clean_fb(FB0_ADDR);

            need_decode = 0;

            /* Advance frame cursor only if we're playing. Entry layout:
             *   uint32 nal_size + uint16 hdr_bits + uint16 reserved + nal_size bytes
             * = 8 bytes header + nal_size payload. */
            if (!cfg.paused) {
                cur_q += 8 + cur_nal_size;
                cfg.frame_idx++;
                if (cfg.frame_idx >= cfg.total_frames) {
                    cfg.frame_idx = 0;
                    cur_q = p_start;
                }
            }
        }

        while (ticks_to_us(timer_elapsed(t_frame_start, timer_read())) < frame_us) {
            /* spin */
        }
    }
}
