/*
 * Jupiter SDK — Jupiter Planet + DuckTales "The Moon" (NES)
 *
 * Combines the procedural Jupiter planet renderer (from the video fork's
 * jupiter_logo demo) with the NES VGM playback path. Runs at 60fps with
 * audio_update interleaved so the DMA pipeline never starves.
 */
#include "jupiter.h"
#include "hstimer.h"
#include "../../libvgm/vgm_player.h"
#include "the_moon.h"

/* ---- Audio plumbing (shared with vgm_play) ---- */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;
extern volatile uint32_t audio_transitions;

static vgm_player_t vgm;
static int16_t stereo[2048];

/* ISR-side VGM-to-mix_buf producer. Same shape as the original
 * vgm_fill but WITHOUT the legacy audio_update() pump — that was a
 * polling-mode refill that's redundant when audio_dma_isr is also
 * refilling, and calling it from ISR context would re-enter the
 * audio refill path. The DMA ISR handles mix_buf → AUDIO_BUF
 * transport independently. */
static void vgm_fill_isr(uint32_t n)
{
    uint32_t done = 0;
    while (done < n) {
        uint32_t chunk = n - done;
        if (chunk > 128) chunk = 128;
        vgm_render(&vgm, stereo + done * 2, chunk);
        for (uint32_t i = 0; i < chunk; i++) {
            if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2))
                continue;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2];
            mix_wr++;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2 + 1];
            mix_wr++;
        }
        done += chunk;
    }
}

/* hstimer-driven audio tick. Renders ~800 stereo frames every ~15 ms
 * (66 Hz × 800 = 52,800 fps, slight overproduction vs 48 kHz codec —
 * the backpressure check in vgm_fill_isr drops samples when mix_buf
 * fills, so net production matches consumption).
 *
 * WHY ISR-DRIVEN: the original main-loop vgm_fill kept up FINE when
 * the demo was a standalone bin (290 KB binary, hot path stays in
 * icache). In the menu binary (10 MB), the planet renderer's hot
 * path evicts cache lines that NES_APU_np_Render needs, slowing the
 * per-sample emulation just enough that 800 samples no longer fit in
 * the post-frame wait budget. Audio production then lags video
 * → mix_buf underruns → rapid dropouts.
 *
 * Decoupling audio from the main-loop cadence makes production
 * cache-pressure-independent: each tick renders its 800 samples
 * directly, no waiting for video frames. Same total CPU cost,
 * just deterministic timing. */
#define AUDIO_TICK_SCANLINES  256u                  /* ~66 Hz */
#define AUDIO_TICK_TARGET     ((MIX_BUF_SIZE * 3) / 4)  /* keep mix_buf at 75% */
#define AUDIO_TICK_MAX        800u                   /* cap per-tick work */

static volatile int audio_isr_armed   = 0;
static volatile int audio_isr_running = 0;

static void vgm_audio_tick(void)
{
    if (!audio_isr_armed) return;   /* guard during reload */
    audio_isr_running = 1;
    __sync_synchronize();
    /* Re-check armed after publishing isr_running so the loop reload
     * path can spin-wait on isr_running to know we've exited. */
    if (audio_isr_armed) {
        uint32_t depth = mix_wr - mix_rd;
        if (depth < AUDIO_TICK_TARGET) {
            uint32_t deficit_slots  = AUDIO_TICK_TARGET - depth;
            uint32_t deficit_frames = deficit_slots / 2;
            if (deficit_frames > AUDIO_TICK_MAX) deficit_frames = AUDIO_TICK_MAX;
            if (deficit_frames > 0) vgm_fill_isr(deficit_frames);
        }
    }
    __sync_synchronize();
    audio_isr_running = 0;
}

/* ====================================================================
 * Planet renderer — lifted from video fork's jupiter_logo, with row-loop
 * audio pumping so playback survives long renders.
 * ==================================================================== */

#define FP 10
#define FP_ONE (1 << FP)

static uint32_t isqrt(uint32_t n)
{
    uint32_t r = 0, b = 1U << 30;
    while (b > n) b >>= 2;
    while (b) {
        if (n >= r + b) { n -= r + b; r = (r >> 1) + b; }
        else { r >>= 1; }
        b >>= 2;
    }
    return r;
}

#define SIN_N    256
#define SIN_MASK 255
#define SIN_FP   12
static int32_t sin_lut[SIN_N], cos_lut[SIN_N];

static void init_sincos(void)
{
    for (int i = 0; i < SIN_N; i++) {
        int j = i & 127;
        int32_t v = (4 * j * (128 - j)) >> (14 - SIN_FP);
        sin_lut[i] = (i >= 128) ? -v : v;
    }
    for (int i = 0; i < SIN_N; i++)
        cos_lut[i] = sin_lut[(i + 64) & SIN_MASK];
}

#define PR     105
#define PR2    (PR * PR)
#define PD     (PR * 2 + 1)
#define PCX    (LCD_W / 2)
#define PCY    (LCD_H / 2 - 10)

#define NORM_SHIFT 20
#define NORM_INV   ((1 << NORM_SHIFT) / (PR << (SIN_FP - FP)))

static uint16_t nz_lut[PD * PD];
static uint16_t disc_half[PD];
static uint32_t lon_step[PD];

static void precompute_geometry(void)
{
    for (int dy = -PR; dy <= PR; dy++) {
        int dy2 = dy * dy;
        int rem = PR2 - dy2;
        uint16_t hw = (rem > 0) ? (uint16_t)isqrt((uint32_t)rem) : 0;
        disc_half[dy + PR] = hw;
        lon_step[dy + PR] = (hw > 0) ? (256U << 16) / (uint32_t)(hw * 2) : 0;

        for (int dx = -PR; dx <= PR; dx++) {
            int d2 = dx * dx + dy2;
            nz_lut[(dy + PR) * PD + (dx + PR)] =
                (d2 <= PR2) ? (uint16_t)isqrt((uint32_t)(PR2 - d2)) : 0;
        }
    }
}

#define LAT_STEPS 64
static uint32_t band_lut[LAT_STEPS];

static void precompute_bands(void)
{
    for (int i = 0; i < LAT_STEPS; i++) {
        int32_t alat = (i * FP_ONE) / LAT_STEPS;
        uint32_t c;
        if      (alat < FP_ONE *  8 / 100) c = 0x00EBD7AF;
        else if (alat < FP_ONE * 16 / 100) c = 0x00AA7346;
        else if (alat < FP_ONE * 24 / 100) c = 0x00E1CDA5;
        else if (alat < FP_ONE * 35 / 100) c = 0x00A07850;
        else if (alat < FP_ONE * 50 / 100) c = 0x00D7C8AA;
        else if (alat < FP_ONE * 62 / 100) c = 0x00AF916E;
        else if (alat < FP_ONE * 78 / 100) c = 0x00BEB4A5;
        else                                c = 0x009B9BAA;
        band_lut[i] = c;
    }
}

static uint32_t space_colors[LCD_H];

static void precompute_space(void)
{
    for (uint32_t y = 0; y < LCD_H; y++) {
        uint32_t t = (y * 256) / LCD_H;
        space_colors[y] = ((0x01 + (t * 0x03 >> 8)) << 16) |
                          ((0x01 + (t * 0x02 >> 8)) << 8)  |
                           (0x04 + (t * 0x08 >> 8));
    }
}

static void render_space_full(uint32_t *fb)
{
    for (uint32_t y = 0; y < LCD_H; y++) {
        uint32_t c = space_colors[y];
        uint32_t *row = fb + y * LCD_W;
        for (uint32_t x = 0; x < LCD_W; x++) row[x] = c;
    }
}

static void render_planet(uint32_t *fb, uint32_t frame)
{
    int32_t la = frame;
    int32_t lx = cos_lut[la & SIN_MASK];
    int32_t lz = sin_lut[la & SIN_MASK];
    int32_t ly = sin_lut[(la / 3) & SIN_MASK] >> 2;
    int32_t lon_offset_fp16 = (int32_t)(frame / 2) << 16;

    int y0 = PCY - PR;
    int y1 = PCY + PR;
    if (y0 < 0) y0 = 0;
    if (y1 >= (int)LCD_H) y1 = LCD_H - 1;

    for (int y = y0; y <= y1; y++) {
        int dy = y - PCY;
        int dyi = dy + PR;
        uint32_t *row = fb + y * LCD_W;
        const uint16_t *nz_row = &nz_lut[dyi * PD + PR];
        int32_t hw = (int32_t)disc_half[dyi];
        if (hw == 0) continue;

        int32_t lat_fp = (dy * FP_ONE) / PR;
        int32_t alat = lat_fp < 0 ? -lat_fp : lat_fp;
        int idx = (alat * LAT_STEPS) >> FP;
        if (idx >= LAT_STEPS) idx = LAT_STEPS - 1;
        uint32_t base_band = band_lut[idx];

        int grs_ok = (lat_fp < -FP_ONE * 20 / 100 && lat_fp > -FP_ONE * 34 / 100);
        int32_t grs_dlat = lat_fp + FP_ONE * 27 / 100;
        int32_t grs_ey = (grs_dlat * grs_dlat * 16) / (FP_ONE * FP_ONE / 256);

        uint32_t lstep = lon_step[dyi];
        int32_t dy_ly = dy * ly;

        int x0 = PCX - PR;
        int x1 = PCX + PR;
        if (x0 < 0) x0 = 0;
        if (x1 >= (int)LCD_W) x1 = LCD_W - 1;

        for (int x = x0; x <= x1; x++) {
            int dx = x - PCX;
            int32_t nz = (int32_t)nz_row[dx];
            if (nz == 0) continue;

            uint32_t lon_accum = (uint32_t)(dx + hw) * lstep + (uint32_t)lon_offset_fp16;
            int32_t lon = (int32_t)((lon_accum >> 16) & 0xFF);

            int32_t dot = dx * lx + dy_ly + nz * lz;
            int32_t diff = (dot * NORM_INV) >> NORM_SHIFT;

            uint32_t cr = (base_band >> 16) & 0xFF;
            uint32_t cg = (base_band >> 8) & 0xFF;
            uint32_t cb = base_band & 0xFF;

            if (grs_ok) {
                int32_t dlon = ((lon - 80 + 128) & 0xFF) - 128;
                if (dlon > -18 && dlon < 18) {
                    int32_t ed = dlon * dlon + grs_ey;
                    if (ed < 200) {
                        if      (ed < 80)  { cr=180; cg=65;  cb=40; }
                        else if (ed < 140) { cr=195; cg=80;  cb=45; }
                        else               { cr=210; cg=105; cb=60; }
                    }
                }
            }

            int32_t lit;
            if (diff > 0) {
                lit = 64 + ((FP_ONE - 64) * diff >> FP);
                if (lit > FP_ONE) lit = FP_ONE;
            } else {
                lit = 64;
            }
            cr = cr * lit >> FP;
            cg = cg * lit >> FP;
            cb = cb * lit >> FP;

            if (diff > FP_ONE / 4) {
                int32_t s = diff - FP_ONE / 4;
                s = (s * s) >> FP;
                s = (s * s) >> FP;
                cr += s * 200 >> FP;
                cg += s * 180 >> FP;
                cb += s * 140 >> FP;
            }

            if (nz < PR / 3) {
                int32_t glow = PR / 3 - nz;
                cb += glow * 3 / 2;
                cg += glow * 3 / 4;
            }

            if (cr > 255) cr = 255;
            if (cg > 255) cg = 255;
            if (cb > 255) cb = 255;

            row[x] = (cr << 16) | (cg << 8) | cb;
        }
    }
}

/* ---- Stars ---- */
static const struct {
    int x, y; uint8_t mag, twinkle;
} real_stars[] = {
    { 45,30,4,3},{ 78,55,3,5},{120,20,5,2},{ 35,90,2,7},{ 95,75,3,4},
    {150,45,2,6},{ 12,140,3,3},{ 60,165,2,8},{110,130,4,2},{ 25,200,3,5},
    { 85,230,2,4},{145,195,3,6},{ 50,250,2,7},{100,260,3,3},{ 20,110,5,2},
    {  8,55,3,5},{ 55,120,2,4},{ 30,160,3,6},{ 70,145,2,3},{380,25,3,4},
    {420,60,4,3},{460,35,2,7},{395,95,2,5},{440,110,3,2},{470,80,3,6},
    {350,40,2,8},{410,150,2,4},{450,180,3,5},{370,200,4,3},{430,220,2,6},
    {465,195,2,4},{385,250,3,7},{445,260,2,3},{200,8,2,5},{280,15,3,4},
    {320,5,2,6},{175,265,2,7},{260,255,3,3},{310,268,2,5},{160,15,2,4},
    {330,20,2,6},{ 15,10,3,3},{475,5,2,5},{ 10,260,2,4},{475,265,3,7},
    {  5,180,2,3},{478,150,2,5},
};
#define NUM_STARS (sizeof(real_stars) / sizeof(real_stars[0]))

static void render_stars(volatile uint32_t *ovl, uint32_t frame)
{
    for (uint32_t i = 0; i < NUM_STARS; i++) {
        int cx = real_stars[i].x, cy = real_stars[i].y;
        int mag = real_stars[i].mag;
        int32_t twinkle = sin_lut[(frame * real_stars[i].twinkle) & SIN_MASK];
        int32_t bright = 0x40 + mag * 0x30 + (twinkle >> 5);
        if (bright < 0x10) bright = 0x10;
        if (bright > 0xFF) bright = 0xFF;

        int radius = (mag >= 4) ? 2 : (mag >= 3) ? 1 : 0;

        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int sx = cx + dx, sy = cy + dy;
                if (sx < 0 || sx >= (int)LCD_W || sy < 0 || sy >= (int)LCD_H) continue;
                int px = sx - PCX, py = sy - PCY;
                if (px * px + py * py < (PR + 4) * (PR + 4)) continue;

                int d = dx * dx + dy * dy;
                int32_t f = (d == 0) ? bright : (d <= 1) ? bright*3/4 :
                            (d <= 2) ? bright/2 : bright/4;
                if (f < 0x08) continue;

                uint32_t r, g, b;
                if (mag >= 4) { r=f; g=f*245/256; b=f*220/256; }
                else          { r=f*220/256; g=f*230/256; b=f; }
                uint32_t a = (f > 0x80) ? 0xFF : (uint32_t)(f * 2);
                ovl[sy * LCD_W + sx] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/* ==== ENTRY ==== */
void main(void)
{
    uart_puts("\n\n=== Jupiter + The Moon ===\n");
    timer_init();
    mmu_init();

    /* Precompute LUTs FIRST so the audio DMA isn't running starved while
     * we precompute. precompute_geometry alone is ~22 KB of LUT init. */
    init_sincos();
    precompute_space();
    precompute_bands();
    uart_puts("Precomputing planet geometry...\n");
    precompute_geometry();

    memset32_neon(FB0_ADDR, 0, LCD_W * LCD_H * 4);
    memset32_neon(FB1_ADDR, 0, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR, 0, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0, LCD_W * LCD_H * 4);

    video_init();

    render_space_full((uint32_t *)FB0_ADDR);
    render_space_full((uint32_t *)FB1_ADDR);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(FB1_ADDR);

    /* Now bring audio up — DMA starts, and the hstimer ISR will keep
     * mix_buf topped up independently of the main loop. */
    irq_init();
    audio_init();

    /* Load DuckTales. NES VGMs run native at 48 kHz, which already matches
     * audio.c's default rate, but call audio_set_rate to be safe. */
    uart_puts("Loading The Moon (NES)...\n");
    vgm_load(&vgm, vgm_the_moon, vgm_the_moon_len, 0);
    audio_set_rate(vgm.sample_rate);
    vgm_play(&vgm);

    uint32_t spf = vgm.sample_rate / 60;  /* per-frame budget for diag prints */
    (void)spf;

    /* Pre-fill enough samples to cover boot + first few ISR ticks. */
    vgm_fill_isr(800 * 2);
    audio_start();

    /* Arm the ISR audio producer. From here on, vgm sample production is
     * cadence-driven, not main-loop-driven. */
    hstimer_init();
    audio_isr_armed = 1;
    hstimer_set_repeating(0, AUDIO_TICK_SCANLINES, vgm_audio_tick);
    irq_global_enable();

    uart_puts("Entering orbit...\n\n");

    uint32_t back_fb  = FB1_ADDR, front_fb  = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;

    while (1) {
        uint32_t t0 = timer_read();

        /* 1) Render planet to back buffer */
        render_planet((uint32_t *)back_fb, frame);
        uint32_t t_planet = timer_read();

        /* 2) Render stars to back overlay (clear first via NEON) */
        memset32_neon(back_ovl, 0, LCD_W * LCD_H * 4);
        render_stars((volatile uint32_t *)back_ovl, frame);

        /* 3) Push to display */
        dcache_clean_fb(back_fb);
        dcache_clean_range(back_ovl, LCD_W * LCD_H * 4);
        video_swap(back_fb);
        video_set_overlay(back_ovl);

        uint32_t tmp;
        tmp = back_fb;  back_fb  = front_fb;  front_fb  = tmp;
        tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp;

        /* 4) Audio production runs in the hstimer ISR — main loop no
         * longer touches vgm_fill / audio_update. */

        /* 5) Pace to 60fps */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) { }

        /* 6) Loop the song forever. Disarm the ISR around the reload
         * — vgm_load calls calloc(NES_DMC) which is not safe to race
         * against vgm_render running in the ISR. Spin-wait on
         * isr_running so an in-flight render finishes BEFORE we
         * free its chip pointers (use-after-free otherwise; see
         * av_demo for the symptom analysis). */
        if (vgm.cur_loop >= 1) {
            audio_isr_armed = 0;
            __sync_synchronize();
            while (audio_isr_running) { __asm__ volatile("nop"); }
            vgm_stop(&vgm);
            vgm_unload(&vgm);
            vgm_load(&vgm, vgm_the_moon, vgm_the_moon_len, 0);
            vgm_play(&vgm);
            __sync_synchronize();
            audio_isr_armed = 1;
        }

        if ((frame % 60) == 0) {
            uint32_t us_planet = ticks_to_us(timer_elapsed(t0, t_planet));
            uint32_t us_total  = ticks_to_us(timer_elapsed(t0, timer_read()));
            uint32_t fill = mix_wr - mix_rd;
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" planet="); uart_putdec(us_planet);
            uart_puts("us total="); uart_putdec(us_total);
            uart_puts("us fill="); uart_putdec(fill);
            uart_puts("\n");
        }
        frame++;
    }
}
