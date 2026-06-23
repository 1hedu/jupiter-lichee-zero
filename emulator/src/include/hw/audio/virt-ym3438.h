/*
 * Virtual YM3438 (OPN2C) — Nuked-OPN2 backed.
 *
 * The Lichee Pi Zero board wires a real YM3438 over GPIO bit-bang:
 *   PB0..PB7 → D0..D7
 *   PG0      → A0
 *   PG1      → A1
 *   PG2      → /WR
 *   PG3      → /CS
 *   PG4      → /IC (reset)
 *   φM clock from Si5351 / CSI_MCLK on PE1
 *
 * This QEMU device snoops those pin transitions, reconstructs each register
 * write, feeds Nuked-OPN2, and pumps stereo samples into the QEMU audio
 * backend on a periodic timer.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_AUDIO_VIRT_YM3438_H
#define HW_AUDIO_VIRT_YM3438_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "audio/audio.h"

#define TYPE_VIRT_YM3438 "virt-ym3438"
OBJECT_DECLARE_SIMPLE_TYPE(VirtYm3438State, VIRT_YM3438)

/* GPIO input lines (port * 32 + pin layout, but we expose a flat list to
 * the SoC wiring). */
enum {
    VIRT_YM3438_PIN_D0 = 0,   /* PB0..PB7 */
    VIRT_YM3438_PIN_D7 = 7,
    VIRT_YM3438_PIN_A0 = 8,   /* PG0 */
    VIRT_YM3438_PIN_A1 = 9,   /* PG1 */
    VIRT_YM3438_PIN_nWR = 10, /* PG2 */
    VIRT_YM3438_PIN_nCS = 11, /* PG3 */
    VIRT_YM3438_PIN_nIC = 12, /* PG4 */
    VIRT_YM3438_NUM_PINS = 13,
};

struct VirtYm3438State {
    SysBusDevice parent_obj;

    QEMUSoundCard card;
    SWVoiceOut   *voice;

    void *chip;        /* ym3438_t* — Nuked-OPN2 instance */

    uint16_t pin_state; /* shadow of all 13 pins, bit i = pin i level */

    uint32_t clock_hz; /* φM frequency assumed (default 7670454) */
    uint32_t rate_hz;  /* host output sample rate */
};

#endif
