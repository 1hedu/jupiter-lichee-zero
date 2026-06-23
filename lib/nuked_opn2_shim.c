/*
 * Jupiter SDK — Nuked-OPN2 shim for vgm_player
 *
 * vgm_player.c calls a small set of YM2612_* functions that are
 * normally provided by libvgm/emu/cores/ym2612.c (the lighter Gens
 * core). When this shim is linked instead of ym2612.c, the calls
 * forward to libvgm/emu/cores/ym3438.c which is Alexey Khokholov's
 * (Nuke.YKT) cycle-accurate Nuked-OPN2 emulator.
 *
 * Drop-in replacement: same symbol names, same call semantics,
 * just much higher accuracy and CPU cost.
 */
#include <stdint.h>

/* Nuked-OPN2 API (declared in libvgm/emu/cores/ym3438.h) */
extern void *nukedopn2_init(uint32_t clock, uint32_t rate);
extern void  nukedopn2_shutdown(void *chip);
extern void  nukedopn2_reset_chip(void *chip);
extern void  nukedopn2_write(void *chip, uint8_t port, uint8_t data);
extern void  nukedopn2_update(void *chip, uint32_t numsamples, int32_t **sndptr);

/* Symbols vgm_player.c expects from the YM2612 backend */

void *YM2612_Init(uint32_t clock, uint32_t rate, uint8_t interpolation)
{
    (void)interpolation;  /* Nuked has no fast/interp option — always cycle accurate */
    return nukedopn2_init(clock, rate);
}

void YM2612_End(void *chip)
{
    nukedopn2_shutdown(chip);
}

void YM2612_Reset(void *chip)
{
    nukedopn2_reset_chip(chip);
}

void YM2612_Write(void *chip, uint8_t adr, uint8_t data)
{
    nukedopn2_write(chip, adr, data);
}

void YM2612_Update(void *chip, int32_t **buf, uint32_t length)
{
    nukedopn2_update(chip, length, buf);
}
