/*
 * Jupiter SDK — H.264 + audio synchronized playback via CedarVE.
 *
 * Loads a vbin v3 container produced by scripts/wc1_video_pack.py
 * (all-IDR baseline H.264 frames + raw s16le PCM audio appended) and
 * plays it back with audio as the master clock. Each frame is decoded
 * just-in-time when the audio playback position reaches its
 * presentation time (frame_idx / fps seconds).
 *
 * Build: make GAME=examples/cedar_video_av/main.c
 *
 * Audio mixing is driven from an hstimer ISR (the same pattern that
 * solved music-streaming jitter) so video decode/blit stalls don't
 * drop audio samples.
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "hstimer.h"
#include <string.h>
#include <stddef.h>

extern const uint8_t _binary_build_title_vbin_start[];
extern const uint8_t _binary_build_title_vbin_end[];

/* ---- vbin v3 header (48 B) -- matches scripts/wc1_video_pack.py ---- */
typedef struct __attribute__((packed)) {
    char     magic[4];
    uint16_t version;
    uint16_t reserved1;
    uint16_t width;
    uint16_t height;
    uint16_t fps_num;
    uint16_t fps_den;
    uint32_t qp;
    uint32_t num_frames;
    uint32_t video_size;
    uint32_t audio_offset;
    uint32_t audio_size;
    uint16_t audio_rate;
    uint8_t  audio_channels;
    uint8_t  audio_bps;
    uint8_t  reserved2[8];
} vbin_hdr_t;

#define LUMA_ADDR    0x43100000
#define CHROMA_ADDR  0x43200000
#define MUSIC_CHAN   3   /* same channel WC1 uses for music */

extern int16_t  mix_buf[];
extern volatile uint32_t mix_rd;
extern volatile uint32_t mix_wr;
#define MIX_HIGH_WATER  3968u

static void audio_mix_tick(void)
{
    /* Need to produce 22050 Hz × 2 stereo slots = 44100 slots/sec.
     * hstimer fires every 32 scanlines = 1.87 ms ≈ 536 ticks/sec, so
     * each tick must yield ~82 slots = 41 stereo frames to keep up.
     * audio_mix(64) gives ~155% of consumption with margin for occasional
     * dropped ticks. (Was audio_mix(32) which only kept up with 11025 Hz.) */
    if ((mix_wr - mix_rd) < MIX_HIGH_WATER) audio_mix(64);
}

extern void *malloc(size_t);

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();
    video_init();
    input_init(INPUT_N64);

    uart_puts("\n\n=== CedarVE A/V playback (audio-mastered) ===\n");

    memset32_neon(FB0_ADDR,  0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    const vbin_hdr_t *hdr = (const vbin_hdr_t *)_binary_build_title_vbin_start;
    if (memcmp(hdr->magic, "VBIN", 4) != 0 || hdr->version != 3) {
        uart_puts("[ex] FATAL: bad vbin (need v3)\n");
        for (;;) {}
    }
    uart_puts("[ex] vbin: ");
    uart_putdec(hdr->width); uart_puts("x"); uart_putdec(hdr->height);
    uart_puts(" @ ");        uart_putdec(hdr->fps_num);
    uart_puts("/");           uart_putdec(hdr->fps_den);
    uart_puts(" fps  qp=");   uart_putdec(hdr->qp);
    uart_puts("  vframes=");  uart_putdec(hdr->num_frames);
    uart_puts("  audio=");    uart_putdec(hdr->audio_rate);
    uart_puts(" Hz x ");      uart_putdec(hdr->audio_channels);
    uart_puts(" ch  asize="); uart_putdec(hdr->audio_size);
    uart_puts("\n");

    cedar_init();

    /* ---- Audio bring-up ---- */
    audio_init();
    audio_set_rate(hdr->audio_rate);
    audio_set_hp_vol(63);
    audio_set_master_volume(255);
    /* Pre-fill mix_buf with silence so the codec has data to consume
     * once we start it (same pattern as examples/sdmmc_music). */
    for (int i = 0; i < 2048; i++) { mix_buf[i] = 0; mix_wr++; }
    audio_start();
    irq_global_enable();

    /* hstimer-driven mix tick at ~1.87 ms (32 scanlines). */
    hstimer_init();
    hstimer_set_repeating(0, 32, audio_mix_tick);

    /* ---- Copy audio into a heap buffer ---- */
    const uint8_t *audio_src = _binary_build_title_vbin_start
                             + sizeof(vbin_hdr_t)
                             + hdr->audio_offset;
    uint32_t audio_samples = hdr->audio_size / 2 / hdr->audio_channels;
    int16_t *audio_buf = (int16_t *)malloc(hdr->audio_size + 4);
    if (!audio_buf) {
        uart_puts("[ex] FATAL: malloc fail for audio\n");
        for (;;) {}
    }
    memcpy(audio_buf, audio_src, hdr->audio_size);

    /* Compute total audio duration (us). Used only for sanity / loop
     * length detection. Video runs as long as audio runs. */
    uint64_t audio_us = (uint64_t)audio_samples * 1000000u / hdr->audio_rate;
    uart_puts("[ex] audio duration "); uart_putdec((uint32_t)(audio_us / 1000));
    uart_puts(" ms (");                uart_putdec(audio_samples);
    uart_puts(" samples)\n");

    /* ---- Find video data ---- */
    const uint8_t *video_start = _binary_build_title_vbin_start + sizeof(vbin_hdr_t);

    /* ---- Index every frame's offset (so we can jump to the frame
     * that matches the current audio position even if we've skipped
     * past it). Each entry is 8 + nal_size, so we walk once. ---- */
    uint32_t *frame_offsets = (uint32_t *)malloc(hdr->num_frames * sizeof(uint32_t));
    uint16_t *frame_hdr_bits = (uint16_t *)malloc(hdr->num_frames * sizeof(uint16_t));
    if (!frame_offsets || !frame_hdr_bits) {
        uart_puts("[ex] FATAL: malloc fail for frame index\n");
        for (;;) {}
    }
    {
        const uint8_t *q = video_start;
        for (uint32_t f = 0; f < hdr->num_frames; f++) {
            frame_offsets[f] = (uint32_t)(q - video_start);
            uint32_t nal_size; memcpy(&nal_size, q, 4);
            uint16_t hb;       memcpy(&hb, q + 4, 2);
            frame_hdr_bits[f] = hb;
            q += 8 + nal_size;
        }
    }

    int dst_x = ((int)LCD_W - (int)hdr->width) / 2;
    int dst_y = ((int)LCD_H - (int)hdr->height) / 2;
    if (dst_x < 0) dst_x = 0;
    if (dst_y < 0) dst_y = 0;

    /* ---- Kick audio playback. We use src_rate=audio_rate so the
     * mixer plays at exactly that pace; no resample. ---- */
    audio_pcm_play_rate(MUSIC_CHAN, audio_buf, audio_samples,
                        220, /* loop= */ 0, hdr->audio_rate);

    /* Capture wall-clock start so we can compute current audio sample
     * position from elapsed time (no need to introspect the mixer's
     * channel state). At 1.2 GHz, pmu cycles wraps every 3.6 s — too
     * fast for a 20 s clip, so use timer_read() (likely slower). */
    uint32_t t_start = timer_read();
    int last_frame = -1;
    int dropped = 0, decoded = 0;

    /* Main playback loop. */
    while (1) {
        uint32_t elapsed_us = ticks_to_us(timer_elapsed(t_start, timer_read()));
        if ((uint64_t)elapsed_us >= audio_us) {
            /* Reached end of audio — break out. */
            break;
        }

        /* Compute the frame index whose presentation time is now. */
        uint32_t target_frame = (uint32_t)(((uint64_t)elapsed_us * hdr->fps_num)
                                            / (1000000ull * hdr->fps_den));
        if (target_frame >= hdr->num_frames) target_frame = hdr->num_frames - 1;

        if ((int)target_frame == last_frame) {
            /* Already showing this frame; loop again to wait. */
            continue;
        }
        if (last_frame >= 0 && (int)target_frame > last_frame + 1) {
            /* We're behind — count drops. */
            dropped += (int)target_frame - last_frame - 1;
        }
        last_frame = (int)target_frame;

        /* ---- Decode the target frame ---- */
        uint32_t off = frame_offsets[target_frame];
        const uint8_t *q = video_start + off;
        uint32_t nal_size; memcpy(&nal_size, q, 4);
        uint16_t hb = frame_hdr_bits[target_frame];
        const uint8_t *nal = q + 8;

        #define NAL_STAGE 0x43F00000u
        *(volatile uint8_t *)(NAL_STAGE + 0) = 0x00;
        *(volatile uint8_t *)(NAL_STAGE + 1) = 0x00;
        *(volatile uint8_t *)(NAL_STAGE + 2) = 0x00;
        *(volatile uint8_t *)(NAL_STAGE + 3) = 0x01;
        for (uint32_t i = 0; i < nal_size; i++) {
            *(volatile uint8_t *)(NAL_STAGE + 4 + i) = nal[i];
        }
        dcache_clean_range(NAL_STAGE, nal_size + 4);

        cedar_h264_decode((const uint8_t *)NAL_STAGE,
                          nal_size + 4,
                          hdr->width, hdr->height,
                          hb,
                          (int)hdr->qp,
                          0, 0, 0);

        uint32_t stride = (hdr->width + 15) & ~15;
        uint32_t mb_h   = ((hdr->height + 15) / 16) * 16;
        dcache_invalidate_range(LUMA_ADDR,   stride * mb_h);
        dcache_invalidate_range(CHROMA_ADDR, stride * mb_h / 2);

        uint32_t *fb = (uint32_t *)FB0_ADDR;
        cedar_nv12_to_argb(fb + dst_y * LCD_W + dst_x,
                           LCD_W, hdr->width, hdr->height);
        dcache_clean_fb(FB0_ADDR);

        decoded++;
    }

    /* Done. */
    uart_puts("[ex] playback complete: decoded "); uart_putdec(decoded);
    uart_puts(", dropped "); uart_putdec(dropped); uart_puts("\n");
    audio_pcm_stop(MUSIC_CHAN);

    /* Hold the last frame, idle. */
    for (;;) { }
}
