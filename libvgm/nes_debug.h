/* nes_debug.h — NES APU/DMC state inspection for Jupiter SDK */
#ifndef NES_DEBUG_H
#define NES_DEBUG_H
#include <stdint.h>

typedef struct {
    /* [0-19] Pulse channels (0=pulse1, 1=pulse2) */
    int      pulse_freq[2];       /* 0,1: raw timer period */
    int      pulse_hz[2];         /* 2,3: computed Hz */
    int      pulse_volume[2];     /* 4,5: current volume */
    int      pulse_duty[2];       /* 6,7: duty cycle 0-3 */
    int      pulse_enable[2];     /* 8,9: channel enabled */
    int      pulse_length[2];     /* 10,11: length counter */
    int      pulse_env_count[2];  /* 12,13: envelope counter */
    int      pulse_env_disable[2];/* 14,15: envelope disabled */
    int      pulse_sweep_en[2];   /* 16,17: sweep enabled */
    int      pulse_out[2];        /* 18,19: last stereo output */

    /* [20-25] Triangle */
    int      tri_freq;            /* 20 */
    int      tri_hz;              /* 21 */
    int      tri_enable;          /* 22 */
    int      tri_length;          /* 23 */
    int      tri_linear;          /* 24 */
    int      tri_phase;           /* 25 */

    /* [26-32] Noise */
    int      noise_volume;        /* 26 */
    int      noise_freq;          /* 27 */
    int      noise_enable;        /* 28 */
    int      noise_length;        /* 29 */
    int      noise_env_count;     /* 30 */
    int      noise_env_disable;   /* 31 */
    uint32_t noise_tap;           /* 32 */

    /* [33-34] DMC */
    int      dmc_damp;            /* 33 */
    int      dmc_active;          /* 34 */

    /* [35-37] Frame sequencer */
    int      frame_seq_step;      /* 35 */
    int      frame_seq_count;     /* 36 */
    int      frame_seq_mode;      /* 37: 4 or 5 step */

    /* [38-41] Clocks */
    uint32_t apu_clock;           /* 38 */
    uint32_t apu_rate;            /* 39 */
    uint32_t dmc_clock;           /* 40 */
    uint32_t dmc_rate;            /* 41 */
} nes_debug_state_t;

#endif
