/* Host MMIO contract tests. No claim that these emulate an HDMI PHY.
 * Exercise actual driver code with mapped register windows and a fake
 * oscillator/TCON status source, including restoring a menu after scaler. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "jupiter.h"
#include "pine64_video.h"
#include "hstimer.h"

#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
static uint32_t now, next_blank = 400000u;
static int blank_enabled = 1, freeze_timer;
static uint32_t calls, clean_count;
static const uint32_t frame_period = 400000u;
static void (*raster_callback)(void);
static unsigned raster_lines, raster_stops;
static int raster_oneshot;
void hstimer_set_scanline(int timer, uint32_t lines, void (*callback)(void))
{
    assert(timer == 0 && lines > 0 && lines <= 8192u);
    raster_lines = lines;
    raster_callback = callback;
    raster_oneshot = 1;
}
void hstimer_set_repeating(int timer, uint32_t lines, void (*callback)(void))
{
    assert(timer == 0); /* A64 has no HSTimer1. */
    raster_lines = lines;
    raster_callback = callback;
    raster_oneshot = 0;
}
void hstimer_stop(int timer)
{
    assert(timer == 0);
    raster_callback = 0;
    raster_oneshot = 0;
    ++raster_stops;
}
static void fire_raster_timer(void)
{
    assert(raster_callback);
    uint32_t total = (R(0x01c0d0a4u) >> 16) / 2u;
    uint32_t current = R(0x01c0d0fcu) & 0xfffu;
    R(0x01c0d0fcu) = (R(0x01c0d0fcu) & ~0xfffu) |
                       ((current + raster_lines) % total);
    void (*callback)(void) = raster_callback;
    /* Model a one-shot completing before callback dispatch. If the
     * callback rearms timer0, no epilogue may cancel the new interval. */
    if (raster_oneshot) raster_callback = 0;
    callback();
}
void uart_puts(const char *s) { (void)s; }
void uart_putdec(uint32_t v) { (void)v; }
void uart_puthex(uint32_t v) { (void)v; }
void dcache_clean_range(uint32_t address, uint32_t bytes)
{
    assert(address >= 0x40000000u && bytes <= LCD_FB_BYTES);
    ++clean_count;
}
uint32_t timer_read(void)
{
    ++calls;
    if (!freeze_timer) now += 240u;
    if (blank_enabled && now >= next_blank) {
        R(0x01c0d004u) |= 1u << 14;
        next_blank += frame_period;
    }
    return 0xffffffffu - now;
}
uint32_t timer_elapsed(uint32_t a, uint32_t b) { return a - b; }
static void valid_mode(void)
{
    R(0x01c20064u) = 0x1810;
    R(0x01c202c4u) = 0x1c10;
    R(0x01c20104u) = R(0x01c2011cu) = R(0x01c20150u) = 0x80000000u;
    R(0x01c0d000u) = R(0x01c0d090u) = 0x80000000u;
    R(0x01c0d094u) = R(0x01c0d098u) = R(0x01c0d09cu) = (1279u << 16) | 719u;
    R(0x01c0d0a4u) = (1500u << 16) | 24u;
    R(0x0100000cu) = 0x32;
    R(0x01c00004u) = 0x01876543u;
}
static void no_mode_test(void)
{
    R(0x01100000u) = 0xaabbccddu;
    video_init();
    assert(!pine64_video_is_ready());
    assert(R(0x01100000u) == 0xaabbccddu);
    assert(clean_count == 0);
}
static void interlace_test(void)
{
    valid_mode(); R(0x01c0d090u) |= 1u << 20;
    video_init(); assert(!pine64_video_is_ready());
}
static void main_mode_test(void)
{
    valid_mode(); video_init();
    assert(pine64_video_is_ready());
    assert(pine64_video_output_width() == 1280 && pine64_video_output_height() == 720);
    assert(pine64_video_total_lines() == 750);
    assert(clean_count == 2);
    assert(R(0x0110000cu) == (719u << 16 | 1279u));
    assert(R(0x01102018u) == FB0_ADDR && R(0x01103010u) == OVL_ADDR);
    assert(R(0x0110100cu) == (224u << 16 | 400u));
    assert(R(0x01101080u) == 0x10);
    assert(R(0x01000010u) & 1u);
    assert(R(0x0100000cu) == 0x33);
    assert(R(0x01c00004u) == 0x00876543u);

    video_vi1_init(10, 20, 32, 16);
    assert(R(0x01104000u) == 0xff000401u);
    assert(R(0x01104010u) == SPR_ADDR && R(0x0110400cu) == 128);
    assert(R(0x0110102cu) == (244u << 16 | 410u));
    assert(R(0x01101080u) == 0x210);
    video_vi1_init(UINT32_MAX, 0, 1, 1);
    video_vi1_init(479, 271, 2, 1);
    assert(R(0x0110102cu) == (244u << 16 | 410u));

    assert(!pine64_video_scale_rgb(FB1_ADDR, 0, 136));
    assert(!pine64_video_scale_rgb(0xbffffffcu, 240, 136));
    assert(pine64_video_scale_rgb(FB1_ADDR, 240, 136));
    assert(R(0x01102004u) == (135u << 16 | 239u));
    assert(R(0x01120088u) == 0x80000u && R(0x0112008cu) == 0x80000u);
    for (unsigned i = 0; i < 32; ++i) {
        assert(R(0x01120200u + 4 * i) == 0x40000000u);
        assert(R(0x01120300u + 4 * i) == 0);
        assert(R(0x01120400u + 4 * i) == 0x4000u);
    }
    video_init();
    assert(R(0x01120000u) == 0 && R(0x01104000u) == 0);
    assert(R(0x01102004u) == (271u << 16 | 479u));
    assert(R(0x0110200cu) == LCD_PITCH && R(0x01102018u) == FB0_ADDR);
    assert(R(0x01101080u) == 0x10 && R(0x01101000u) == 0x301);
    assert(UI_ATTR(0) == 0xff000005u);

    video_mode(0);
    assert(BLD_ROUTE == 0 && BLD_PIPE_CTL == 0x101);
    video_mode(2);
    assert(BLD_ROUTE == 0x210 && BLD_PIPE_CTL == 0x701);
    video_mode3_alpha(96);
    assert(UI_ATTR(0) == 0x60000005u);
    video_mode(1);
    assert(UI_ATTR(0) == 0xff000005u);

    video_mode4_nv12(0x43000000u, 0x43100000u, 64, 64, 64, 328, 104);
    assert(VI_ATTR(0) == 0x801u);
    assert(VI_TOP_LADDR0(0) == 0x43000000u && VI_TOP_LADDR1(0) == 0x43100000u);
    assert(VI_PITCH0(0) == 64 && VI_PITCH1(0) == 64);
    assert(BLD_INSIZE(0) == WH(64, 64));
    assert(BLD_OFFSET(0) == (328u << 16 | 728u));
    assert(R(0x01120080u) == WH(64, 64) && R(0x011200c0u) == WH(32, 32));
    assert(R(0x01120088u) == 0x100000u && R(0x0112008cu) == 0x100000u);
    assert(R(0x011200c8u) == 0x80000u && R(0x011200ccu) == 0x80000u);
    assert(R(0x011200d8u) == 0xfffc0000u && R(0x01120000u) == 0x11);
    assert(R(0x011aa050u) == 1 && R(0x011aa060u) == 0x4a8u);
    assert(R(0x011aa068u) == 0x662u && R(0x011aa06cu) == 0xfffc8451u);
    video_mode4_nv12(0x43000000u, 0x43100000u, 63, 64, 64, 0, 0);
    video_mode4_nv12(0x43000000u, 0x43100000u, 64, 64, 62, 0, 0);
    video_mode4_nv12(0xbffffffcu, 0x43100000u, 64, 64, 64, 0, 0);
    video_mode4_nv12(0x43000000u, 0x43100000u, 64, 64, 64, 450, 0);
    assert(BLD_OFFSET(0) == (328u << 16 | 728u));
    video_mode4_off();
    assert(R(0x01120000u) == 0 && R(0x011aa050u) == 0);
    assert(VI_ATTR(0) == 0x8401u && VI_TOP_LADDR0(0) == FB0_ADDR);
    assert(VI_TOP_LADDR1(0) == 0 && BLD_OFFSET(0) == (224u << 16 | 400u));

    video_mode5_split(FB0_ADDR, FB1_ADDR);
    assert(BLD_ROUTE == 0x120 && BLD_PIPE_CTL == 0x701);
    assert(VI_MBSIZE(0) == WH(480, 136));
    assert(PINE64_UI1_SIZE(0) == WH(480, 136) && PINE64_UI1_ADDR(0) == FB1_ADDR);
    assert(PINE64_UI1_ATTR(0) == 0xff000401u && PINE64_UI1_PITCH(0) == LCD_PITCH);
    assert(BLD_OFFSET(0) == (224u << 16 | 400u));
    assert(BLD_OFFSET(1) == (360u << 16 | 400u));
    assert(BLD_OFFSET(2) == (224u << 16 | 400u));
    video_mode5_off();
    assert(PINE64_UI1_ATTR(0) == 0 && VI_MBSIZE(0) == WH(480, 272));
    assert(BLD_ROUTE == 0x10 && BLD_INSIZE(1) == WH(480, 272));

    const int16_t shifts[] = { 4, -3 };
    R(0x01c0d0fcu) = (900u << 16) | 10u; /* Upper bits are TCON0. */
    video_mode7_lineshift(shifts, 2, 8);
    assert(raster_callback && raster_oneshot && raster_lines == 239);
    fire_raster_timer(); assert(VI_TOP_LADDR0(0) == FB0_ADDR + 16);
    assert(!raster_oneshot && raster_lines == 8);
    fire_raster_timer(); assert(VI_TOP_LADDR0(0) == FB0_ADDR - 12);
    fire_raster_timer(); assert(VI_TOP_LADDR0(0) == FB0_ADDR - 12);
    unsigned stops = raster_stops;
    R(0x01c0d0fcu) = 10;
    video_mode7_line_reset(FB1_ADDR);
    assert(raster_callback && raster_oneshot && raster_lines == 239);
    assert(raster_stops == stops + 1 && VI_TOP_LADDR0(0) == FB1_ADDR);
    fire_raster_timer(); assert(VI_TOP_LADDR0(0) == FB1_ADDR + 16);
    video_mode(1);
    assert(!raster_callback && VI_TOP_LADDR0(0) == FB1_ADDR);
    video_mode7_lineshift(shifts, 0, 8); assert(!raster_callback);
    video_mode7_lineshift(shifts, 2, 0); assert(!raster_callback);
    video_mode7_lineshift(shifts, 2, 8);
    fire_raster_timer();
    BLD_CK_CTL = BLD_CK_CFG = 7;
    VI_ATTR(3) = UI_ATTR(3) = PINE64_UI1_ATTR(3) = 1;
    video_init();
    assert(!raster_callback && VI_TOP_LADDR0(0) == FB0_ADDR);
    assert(!BLD_CK_CTL && !BLD_CK_CFG && !VI_ATTR(3) && !UI_ATTR(3) && !PINE64_UI1_ATTR(3));
    video_swap(FB1_ADDR); assert(R(0x01102018u) == FB1_ADDR);
    video_swap(0xfffffffcu); assert(R(0x01102018u) == FB1_ADDR);
    assert(pine64_video_measure_timing());
    assert(pine64_video_frame_ticks() > 399000u && pine64_video_frame_ticks() < 401000u);
    assert(pine64_video_line_ticks() >= 532u && pine64_video_line_ticks() <= 534u);
    R(0x01c0d004u) = (1u << 29) | (1u << 13);
    video_wait_vblank();
    assert((R(0x01c0d004u) & ((1u << 29) | (1u << 13))) == ((1u << 29) | (1u << 13)));
    assert(!(R(0x01c0d004u) & (1u << 14)));
    blank_enabled = 0;
    uint32_t before = calls;
    video_wait_vblank(); assert(calls - before < 20000u);
    freeze_timer = 1; before = calls;
    video_wait_vblank(); assert(calls - before <= 2000001u);
}

static void mode7_phase_test(uint32_t height, uint32_t total, uint32_t porch)
{
    valid_mode();
    uint32_t width = height == 720 ? 1280 : 1920;
    R(0x01c0d094u) = R(0x01c0d098u) = R(0x01c0d09cu) =
        ((width - 1u) << 16) | (height - 1u);
    R(0x01c0d0a4u) = ((total * 2u) << 16) | (porch - 1u);
    video_init();
    assert(pine64_video_is_ready());
    assert(pine64_video_measure_timing());
    int16_t shifts[LCD_H / 8];
    for (unsigned i = 0; i < LCD_H / 8; ++i) shifts[i] = (int16_t)(i + 1);
    uint32_t first = porch + (height - LCD_H) / 2u;
    assert(first == (height == 720 ? 249u : 445u));

    /* A vblank return need not coincide with line zero. Also use an
     * unrelated upper TCON0 value so selecting the wrong field fails. */
    R(0x01c0d0fcu) = (900u << 16) | 10u;
    video_mode7_lineshift(shifts, LCD_H / 8, 8);
    assert(raster_callback && raster_oneshot && raster_lines == first - 10u);
    assert(VI_TOP_LADDR0(0) == FB0_ADDR); /* No shift in blanking/border. */
    fire_raster_timer();
    assert((R(0x01c0d0fcu) & 0xfffu) == first);
    assert(VI_TOP_LADDR0(0) == FB0_ADDR + 4u);
    assert(!raster_oneshot && raster_lines == 8u);
    for (unsigned band = 1; band < LCD_H / 8; ++band) {
        fire_raster_timer();
        assert((R(0x01c0d0fcu) & 0xfffu) == first + band * 8u);
        assert(VI_TOP_LADDR0(0) == FB0_ADDR + (band + 1u) * 4u);
    }
    fire_raster_timer(); /* No read beyond the supplied 34 offsets. */
    assert(VI_TOP_LADDR0(0) == FB0_ADDR + (LCD_H / 8) * 4u);

    /* Resets cancel the running repeat and schedule the upcoming canvas
     * across frame wrap. Another reset cancels that pending one-shot. */
    R(0x01c0d0fcu) = total - 3u;
    video_mode7_line_reset(FB1_ADDR);
    assert(raster_oneshot && raster_lines == first + 3u);
    R(0x01c0d0fcu) = 12u;
    video_mode7_line_reset(FB0_ADDR);
    assert(raster_oneshot && raster_lines == first - 12u);
    fire_raster_timer();
    assert(VI_TOP_LADDR0(0) == FB0_ADDR + 4u);
    R(0x01c0d0fcu) = first; /* Exact canvas start needs no zero timer. */
    video_mode7_line_reset(FB1_ADDR);
    assert(!raster_oneshot && raster_lines == 8u);
    assert(VI_TOP_LADDR0(0) == FB1_ADDR + 4u);
    R(0x01c0d0fcu) = 10u;
    video_mode7_line_reset(FB0_ADDR);
    assert(raster_oneshot);
    video_mode7_lineshift_off();
    assert(!raster_callback && VI_TOP_LADDR0(0) == FB0_ADDR);

    /* Invalid live scan positions disable the effect, without a wait. */
    R(0x01c0d0fcu) = total;
    video_mode7_lineshift(shifts, LCD_H / 8, 8);
    assert(!raster_callback && VI_TOP_LADDR0(0) == FB0_ADDR);
}
static void mode7_phase_720p(void) { mode7_phase_test(720, 750, 25); }
static void mode7_phase_1080p(void) { mode7_phase_test(1080, 1125, 41); }

int main(void)
{
    assert(mmap((void *)0x01000000u, 0x300000u, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) != MAP_FAILED);
    assert(mmap((void *)0x01c00000u, 0x100000u, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) != MAP_FAILED);
    void (*cases[])(void) = { no_mode_test, interlace_test, main_mode_test,
                             mode7_phase_720p, mode7_phase_1080p };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        pid_t pid = fork(); assert(pid >= 0);
        if (!pid) { cases[i](); _exit(0); }
        int status; assert(waitpid(pid, &status, 0) == pid);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    puts("PINE64 video MMIO: HDMI handover, Modes 0/2/3/4/5/7, chroma scaler/CSC, layer reset, native timing and timeouts PASS");
    puts("Mode 7 phase PASS: 720p/1080p blanking + canvas offset, current-line field, all 34 bands, frame wrap and one-shot/repeat cancellation");
    return 0;
}
