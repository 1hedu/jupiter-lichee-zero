/*
 * vgm_player.h — Lightweight VGM Player for Jupiter SDK
 *
 * Ported from libvgm (ValleyBell) C++ to bare-metal C.
 * Supports: YM2612 (Nuked-OPN2), SN76489, GB DMG, NES 2A03.
 */
#ifndef VGM_PLAYER_H
#define VGM_PLAYER_H

#include <stdint.h>

/* Chip enable flags */
#define VGM_CHIP_YM2612   (1 << 0)
#define VGM_CHIP_SN76489  (1 << 1)
#define VGM_CHIP_GB_DMG   (1 << 2)
#define VGM_CHIP_NES_APU  (1 << 3)

/* Player state */
#define VGM_STOPPED  0
#define VGM_PLAYING  1
#define VGM_LOOPED   2

/* VGM header (parsed subset) */
typedef struct {
    uint32_t file_ver;
    uint32_t eof_ofs;
    uint32_t data_ofs;      /* command stream start */
    uint32_t loop_ofs;      /* loop point (0 = no loop) */
    uint32_t data_end;
    uint32_t total_samples;
    uint32_t loop_samples;
    /* Chip clocks (0 = not present) */
    uint32_t ym2612_clock;
    uint32_t sn76489_clock;
    uint32_t gb_dmg_clock;
    uint32_t nes_apu_clock;
} vgm_header_t;

/* PCM data block (for YM2612 DAC) */
typedef struct {
    const uint8_t *data;
    uint32_t       size;
} vgm_pcm_block_t;

#define VGM_MAX_PCM_BLOCKS 8

/* Player instance */
typedef struct {
    /* File data */
    const uint8_t *file_data;
    uint32_t       file_len;
    vgm_header_t   hdr;

    /* Playback state */
    uint32_t file_pos;       /* next command offset */
    uint32_t wait_remain;    /* samples to wait before next cmd */
    uint32_t cur_sample;     /* total samples rendered */
    uint32_t cur_loop;       /* loop count */
    uint8_t  state;          /* VGM_STOPPED / VGM_PLAYING / VGM_LOOPED */
    uint8_t  chips_present;  /* bitmask of detected chips */

    /* Chip instances (opaque pointers to libvgm cores) */
    void *opn2;              /* Nuked-OPN2 (YM2612) */
    void *sn76489;           /* SN76489 PSG */
    void *gb;                /* Game Boy DMG */
    void *nes_apu;           /* NES APU (pulse+triangle+noise) */
    void *nes_dmc;           /* NES DMC (DPCM+frame counter) */

    /* PCM data blocks */
    vgm_pcm_block_t pcm_blocks[VGM_MAX_PCM_BLOCKS];
    uint8_t          pcm_block_count;
    uint32_t         pcm_ofs;   /* YM2612 DAC stream position */

    /* Output config */
    uint32_t sample_rate;
    uint32_t tick_accum;
    uint32_t total_ticks;
    uint32_t total_samples;    /* rate conversion accumulator */
} vgm_player_t;

/*
 * Parse VGM header and initialize chip emulators.
 * vgm_data: pointer to complete VGM file in memory (uncompressed).
 * Returns 0 on success, nonzero on error.
 */
int vgm_load(vgm_player_t *p, const uint8_t *vgm_data, uint32_t len,
             uint32_t sample_rate);

/* Start/restart playback from beginning. */
void vgm_play(vgm_player_t *p);

/* Stop playback and silence all chips. */
void vgm_stop(vgm_player_t *p);

/*
 * Render num_samples of audio into out_buf (interleaved stereo int16).
 * This is the main tick function — call from your DMA callback.
 * Advances the VGM command stream and clocks all active chip emulators.
 */
void vgm_render(vgm_player_t *p, int16_t *out_buf, uint32_t num_samples);

/* Free chip emulator instances. */
void vgm_unload(vgm_player_t *p);

/* Query */
static inline int vgm_is_playing(const vgm_player_t *p)
    { return p->state != VGM_STOPPED; }

#endif /* VGM_PLAYER_H */

/* Debug: register write log */
#define VGM_REG_LOG_SIZE 64
typedef struct {
    uint32_t tick;
    uint8_t  addr;
    uint8_t  val;
} vgm_reg_write_t;

typedef struct {
    vgm_reg_write_t log[VGM_REG_LOG_SIZE];
    uint32_t wr_idx;
    uint32_t total_writes;
    uint32_t writes_per_addr[256];
} vgm_reg_log_t;

extern vgm_reg_log_t vgm_nes_log;
extern vgm_reg_log_t vgm_gb_log;
extern vgm_reg_log_t vgm_ym_log;
