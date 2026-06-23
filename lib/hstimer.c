/*
 * Jupiter SDK — hstimer.c
 * High-Speed Timer driver for scanline-accurate mid-frame interrupts.
 *
 * V3s datasheet §4.6, pages 122-128.
 * Two 56-bit down-counters, AHB-clocked (~200MHz = 5ns/tick).
 */
#include "jupiter.h"
#include "hstimer.h"
#include <stddef.h>

/* Register map */
#define HSTMR_BASE       0x01C60000
#define HSTMR(off)       REG32(HSTMR_BASE + (off))

#define IRQ_EN           0x00
#define IRQ_STAS         0x04

/* Per-timer offsets: timer0 at 0x10, timer1 at 0x30 */
#define TMR_CTRL(t)      (0x10 + (t) * 0x20)
#define TMR_INTV_LO(t)   (0x14 + (t) * 0x20)
#define TMR_INTV_HI(t)   (0x18 + (t) * 0x20)
#define TMR_CURNT_LO(t)  (0x1C + (t) * 0x20)
#define TMR_CURNT_HI(t)  (0x20 + (t) * 0x20)

/* CTRL bits */
#define CTRL_MODE_SINGLE  (1u << 7)   /* 0=continuous, 1=single-shot */
#define CTRL_PRESCALE(n)  (((n) & 7) << 4)  /* 0=/1, 1=/2, 2=/4, 3=/8, 4=/16 */
#define CTRL_RELOAD       (1u << 1)
#define CTRL_ENABLE       (1u << 0)

/* GIC IRQ IDs */
#define HSTMR0_IRQ       83   /* SPI 51 + 32 */
#define HSTMR1_IRQ       84   /* SPI 52 + 32 */

/* Callbacks */
static void (*hstimer_cb[2])(void);

/* ISR for timer 0 */
static void hstimer0_isr(void)
{
    /* Clear pending (w1c) */
    HSTMR(IRQ_STAS) = (1u << 0);

    if (hstimer_cb[0])
        hstimer_cb[0]();
}

/* ISR for timer 1 */
static void hstimer1_isr(void)
{
    HSTMR(IRQ_STAS) = (1u << 1);

    if (hstimer_cb[1])
        hstimer_cb[1]();
}

void hstimer_init(void)
{
    /* Enable HSTimer bus clock gate (BUS_CLK_GATING0 bit 19) */
    REG32(CCU_BASE + 0x0060) |= (1u << 19);

    /* De-assert HSTimer reset (BUS_SOFT_RST0 bit 19) */
    REG32(CCU_BASE + 0x02C0) |= (1u << 19);

    /* Disable both timers */
    HSTMR(TMR_CTRL(0)) = 0;
    HSTMR(TMR_CTRL(1)) = 0;

    /* Clear any pending IRQs */
    HSTMR(IRQ_STAS) = 0x3;

    /* Disable IRQs initially */
    HSTMR(IRQ_EN) = 0;

    /* Register ISRs with GIC */
    irq_register(HSTMR0_IRQ, hstimer0_isr);
    irq_register(HSTMR1_IRQ, hstimer1_isr);
    irq_enable(HSTMR0_IRQ);
    irq_enable(HSTMR1_IRQ);

    hstimer_cb[0] = NULL;
    hstimer_cb[1] = NULL;

    uart_puts("[hstimer] init OK\n");
}

void hstimer_set_ticks(int timer, uint32_t ticks, int oneshot, void (*callback)(void))
{
    if (timer < 0 || timer > 1) return;

    /* Stop timer */
    HSTMR(TMR_CTRL(timer)) = 0;

    /* Store callback */
    hstimer_cb[timer] = callback;

    /* Set interval (write LO first, then HI per datasheet) */
    HSTMR(TMR_INTV_LO(timer)) = ticks;
    HSTMR(TMR_INTV_HI(timer)) = 0;

    /* Configure: prescaler /1, mode, reload + enable */
    uint32_t ctrl = CTRL_PRESCALE(0) | CTRL_RELOAD | CTRL_ENABLE;
    if (oneshot)
        ctrl |= CTRL_MODE_SINGLE;
    HSTMR(TMR_CTRL(timer)) = ctrl;

    /* Enable IRQ for this timer */
    HSTMR(IRQ_EN) = HSTMR(IRQ_EN) | (1u << timer);
}

void hstimer_set_scanline(int timer, uint32_t scanlines, void (*callback)(void))
{
    hstimer_set_ticks(timer, scanlines * HSTIMER_TICKS_PER_SCANLINE, 1, callback);
}

void hstimer_set_repeating(int timer, uint32_t scanlines, void (*callback)(void))
{
    hstimer_set_ticks(timer, scanlines * HSTIMER_TICKS_PER_SCANLINE, 0, callback);
}

void hstimer_stop(int timer)
{
    if (timer < 0 || timer > 1) return;
    HSTMR(TMR_CTRL(timer)) = 0;
    HSTMR(IRQ_EN) = HSTMR(IRQ_EN) & ~(1u << timer);
    hstimer_cb[timer] = NULL;
}

/* ---- Audio-mixer scanline ISR ---- */
extern int16_t mix_buf[];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern void audio_mix(uint32_t num_samples);

/* Filled by hstimer_audio_mix_init; the actual ISR below reads them. */
static uint32_t s_amix_high_water = 0;   /* mix_buf depth target (slots) */
static uint32_t s_amix_per_tick   = 0;   /* stereo frames to mix per tick */

static void hstimer_audio_mix_isr(void)
{
    if ((mix_wr - mix_rd) < s_amix_high_water) audio_mix(s_amix_per_tick);
}

void hstimer_audio_mix_init(uint32_t scanlines, uint32_t samples_per_tick)
{
    /* mix_buf is 4096 slots in lib/audio.c. Aim for "mostly full" — the
     * cedar_video_av canonical was 3968 (4096 - 128). Same target here. */
    s_amix_high_water = 4096u - 128u;
    s_amix_per_tick   = samples_per_tick ? samples_per_tick : 64u;
    hstimer_init();
    hstimer_set_repeating(0, scanlines ? scanlines : 32u, hstimer_audio_mix_isr);
}
