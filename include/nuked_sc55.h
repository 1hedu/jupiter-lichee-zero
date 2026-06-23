/*
 * Jupiter SDK — Nuked-SC55 bare-metal driver
 *
 * Wraps the Nuked-SC55 emulator (C++) so it can be driven from plain
 * C code. Loads embedded ROM data, steps the H8 MCU + PCM at the
 * native 64 kHz output rate, and exposes a sample callback for the
 * audio pipeline to consume.
 */
#ifndef JUPITER_NUKED_SC55_H
#define JUPITER_NUKED_SC55_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize as SC-55 mkII with embedded ROMs.
 * Each pointer is to a contiguous byte array of the indicated length.
 * Returns 0 on success, negative on error. */
int sc55_init(const uint8_t *rom1, uint32_t rom1_len,
              const uint8_t *rom2, uint32_t rom2_len,
              const uint8_t *waverom1, uint32_t waverom1_len,
              const uint8_t *waverom2, uint32_t waverom2_len,
              const uint8_t *rom_sm, uint32_t rom_sm_len);

/* Inject one MIDI byte into the SC-55's UART input. */
void sc55_post_uart(uint8_t b);

/* Set a callback that fires for each stereo sample produced.
 * Native rate is 64 kHz. Callback runs in the context of sc55_step(). */
typedef void (*sc55_sample_cb_t)(int16_t left, int16_t right);
void sc55_set_sample_callback(sc55_sample_cb_t cb);

/* Run the emulator until it has produced `frames` stereo samples. */
void sc55_render(uint32_t frames);

/* Native output sample rate (Hz). 64000 for SC-55 mk1/JV-880, 66207 for mk2. */
uint32_t sc55_native_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* JUPITER_NUKED_SC55_H */
