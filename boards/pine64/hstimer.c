/* A64's single HSTimer, GIC SPI51/ID83, A64 manual section 3.9.
 * Its AHB rate is measured against the fixed 24 MHz timer. Only index0
 * exists; do not write V3s timer1's reserved register addresses on A64. */
#include "jupiter.h"
#include "hstimer.h"
#include "pine64_video.h"

#define HS(off) REG32(0x01c60000u + (off))
#define HS_IRQ 83u
static void (*hs_callback)(void);
static uint32_t hs_rate;
static int hs_clocked;

static uint32_t lock_irq(void)
{
    uint32_t flags;
    __asm__ volatile("mrs %0, cpsr\ncpsid i" : "=r"(flags) :: "memory");
    return flags;
}
static void unlock_irq(uint32_t flags)
{
    if (!(flags & 0x80u)) __asm__ volatile("cpsie i" ::: "memory");
}
static void barrier(void) { __asm__ volatile("dsb sy" ::: "memory"); }

static void pause_hardware(void)
{
    HS(0x00) = 0;
    HS(0x10) = 0;
    barrier();
    /* At least two AHB cycles after pausing. Three peripheral reads also
     * flush the posted stop write before replacing the interval. */
    (void)HS(0x10); (void)HS(0x10); (void)HS(0x10);
    HS(0x04) = 1;
    barrier();
}

static void hs_isr(void)
{
    HS(0x04) = 1; /* HSTimer status is W1C, unlike TCON. */
    barrier();
    if (hs_callback) hs_callback();
}

void hstimer_init(void)
{
    uint32_t flags = lock_irq();
    irq_disable(HS_IRQ);
    REG32(0x01c20060u) |= (1u << 19);
    REG32(0x01c202c0u) |= (1u << 19);
    barrier();
    hs_clocked = 1;
    hs_rate = 0;
    hs_callback = 0;
    pause_hardware();
    HS(0x14) = 0xffffffffu;
    HS(0x18) = 0;
    HS(0x10) = 3; /* reload+enable, /1, normal continuous mode */
    barrier();
    uint32_t first = HS(0x1c);
    uint32_t start = timer_read(), elapsed = 0;
    for (uint32_t polls = 0; polls < 1000000u; ++polls) {
        elapsed = timer_elapsed(start, timer_read());
        if (elapsed >= 24000u) break; /* one millisecond */
    }
    uint32_t ticks = first - HS(0x1c);
    pause_hardware();
    if (elapsed >= 24000u && elapsed <= 240000u && ticks) {
        uint64_t measured = (uint64_t)ticks * 24000000u / elapsed;
        if (measured >= 1000000u && measured <= 600000000u)
            hs_rate = (uint32_t)measured;
    }
    irq_register(HS_IRQ, hs_isr);
    unlock_irq(flags);
    if (!hs_rate) {
        uart_puts("[hstimer/pine64] counter calibration FAILED; timer unavailable\n");
        return;
    }
    uart_puts("[hstimer/pine64] one timer; measured AHB Hz=");
    uart_putdec(hs_rate); uart_puts("; GIC ID83\n");
}

void hstimer_set_ticks(int timer, uint32_t ticks, int oneshot,
                       void (*callback)(void))
{
    if (timer != 0) {
        uart_puts("[hstimer/pine64] only timer0 exists on A64\n");
        return;
    }
    if (!hs_rate || !ticks || !callback) return;
    uint32_t flags = lock_irq();
    irq_disable(HS_IRQ);
    pause_hardware();
    hs_callback = callback;
    HS(0x14) = ticks;
    HS(0x18) = 0; /* interval LO must precede HI */
    HS(0x00) = 1;
    irq_enable(HS_IRQ);
    HS(0x10) = 3u | (oneshot ? 0x80u : 0u);
    barrier();
    unlock_irq(flags);
}

static uint32_t ticks_from_native_lines(uint32_t lines)
{
    if (!hs_rate || !lines || lines > 8192u) return 0;
    if (!pine64_video_frame_ticks() && !pine64_video_measure_timing()) {
        uart_puts("[hstimer/pine64] HDMI line timing unavailable\n");
        return 0;
    }
    uint64_t t = (uint64_t)lines * pine64_video_frame_ticks() * hs_rate /
                 ((uint64_t)pine64_video_total_lines() * 24000000u);
    return t <= 0xffffffffu ? (uint32_t)t : 0;
}

void hstimer_set_scanline(int timer, uint32_t lines, void (*callback)(void))
{
    hstimer_set_ticks(timer, ticks_from_native_lines(lines), 1, callback);
}
void hstimer_set_repeating(int timer, uint32_t lines, void (*callback)(void))
{
    hstimer_set_ticks(timer, ticks_from_native_lines(lines), 0, callback);
}
void hstimer_stop(int timer)
{
    if (timer != 0 || !hs_clocked) return;
    uint32_t flags = lock_irq();
    irq_disable(HS_IRQ);
    pause_hardware();
    hs_callback = 0;
    unlock_irq(flags);
}
int pine64_hstimer_ready(void) { return hs_rate != 0; }
uint32_t pine64_hstimer_hz(void) { return hs_rate; }
uint32_t pine64_hstimer_ticks_from_us(uint32_t us)
{
    uint64_t ticks = (uint64_t)us * hs_rate / 1000000u;
    return ticks <= 0xffffffffu ? (uint32_t)ticks : 0;
}
void pine64_hstimer_set_us(uint32_t us, int oneshot, void (*callback)(void))
{
    hstimer_set_ticks(0, pine64_hstimer_ticks_from_us(us), oneshot, callback);
}

extern volatile uint32_t mix_wr, mix_rd;
extern void audio_mix(uint32_t frames);
static uint32_t audio_frames;
static void audio_tick(void)
{
    if (mix_wr - mix_rd < 4096u - 128u) audio_mix(audio_frames);
}
void hstimer_audio_mix_init(uint32_t scanlines, uint32_t samples_per_tick)
{
    /* Audio cadence is time-based and must not change with HDMI EDID.
     * Preserve the SDK's 32-line ~1.87 ms nominal mixer cadence as a
     * duration, independently of the actual HDMI raster line period. */
    audio_frames = samples_per_tick ? samples_per_tick : 64u;
    uint32_t lines = scanlines ? scanlines : 32u;
    if (lines > 10000u) lines = 10000u;
    hstimer_init();
    pine64_hstimer_set_us(lines * 175u / 3u, 0, audio_tick);
}
