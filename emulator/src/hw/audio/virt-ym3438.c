/*
 * Virtual YM3438 — wraps Nuked-OPN2 behind a GPIO-driven QEMU device.
 *
 * Wiring (set in lichee-zero.c / allwinner-v3s.c):
 *   line  0..7  → PB0..PB7 (data bus)
 *   line  8..9  → PG0..PG1 (A0, A1)
 *   line 10     → PG2      (/WR, active low)
 *   line 11     → PG3      (/CS, active low)
 *   line 12     → PG4      (/IC, active low reset)
 *
 * On every pin transition we update the shadow. A /WR rising edge while
 * /CS is asserted (low) latches one bus cycle: port = {A1,A0}, data = PB.
 * /IC falling edge resets the chip.
 *
 * The pump_timer runs every 10 ms, asks Nuked-OPN2 for ~441 stereo samples
 * (at 44.1 kHz) and pushes them via AUD_write.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "audio/audio.h"
#include "hw/audio/virt-ym3438.h"

/* Vendored Nuked-OPN2 (libvgm flavor) */
#include "ym3438.h"

#define DEFAULT_CLOCK_HZ   7670454u   /* canonical NTSC OPN2 clock */
#define OUTPUT_RATE_HZ     44100u
#define PUMP_SAMPLES       (OUTPUT_RATE_HZ / 100)  /* 441 — chunk size */

#define YM_PIN_BIT(b)  (1u << (b))  /* QEMU's BIT() macro collides; rename. */

static inline int pin(VirtYm3438State *s, int n) { return (s->pin_state >> n) & 1; }

static void virt_ym3438_strobe_write(VirtYm3438State *s)
{
    /* Real YM3438 latches on /WR rising edge with /CS low. */
    uint8_t a0 = pin(s, VIRT_YM3438_PIN_A0);
    uint8_t a1 = pin(s, VIRT_YM3438_PIN_A1);
    uint8_t port = (a1 << 1) | a0;
    uint8_t data = s->pin_state & 0xFF;
    nukedopn2_write(s->chip, port, data);
}

static void virt_ym3438_gpio_in(void *opaque, int line, int level)
{
    VirtYm3438State *s = opaque;
    uint16_t prev = s->pin_state;

    if (line < 0 || line >= VIRT_YM3438_NUM_PINS) {
        return;
    }
    if (level) {
        s->pin_state |= YM_PIN_BIT(line);
    } else {
        s->pin_state &= ~YM_PIN_BIT(line);
    }

    /* /IC falling edge → reset chip */
    if (line == VIRT_YM3438_PIN_nIC) {
        if (((prev >> VIRT_YM3438_PIN_nIC) & 1) && !level) {
            nukedopn2_reset_chip(s->chip);
        }
        return;
    }

    /* /WR rising edge with /CS asserted → strobe a write */
    if (line == VIRT_YM3438_PIN_nWR) {
        uint8_t prev_wr = (prev >> VIRT_YM3438_PIN_nWR) & 1;
        if (!prev_wr && level && !pin(s, VIRT_YM3438_PIN_nCS)) {
            virt_ym3438_strobe_write(s);
        }
    }
}

/* Audio backend callback — fill `len` bytes (stereo s16) from the chip. */
static void virt_ym3438_audio_cb(void *opaque, int len)
{
    VirtYm3438State *s = opaque;
    if (!s->voice) {
        return;
    }
    DEV_SMPL bufL[PUMP_SAMPLES];
    DEV_SMPL bufR[PUMP_SAMPLES];
    DEV_SMPL *channels[2] = { bufL, bufR };

    int frames_total = len / 4;          /* 4 = bytes per stereo S16 frame */
    while (frames_total > 0) {
        int n = frames_total > PUMP_SAMPLES ? PUMP_SAMPLES : frames_total;
        nukedopn2_update(s->chip, n, channels);

        int16_t mix[PUMP_SAMPLES * 2];
        for (int i = 0; i < n; i++) {
            int32_t l = bufL[i], r = bufR[i];
            if (l >  32767) l =  32767;
            if (l < -32768) l = -32768;
            if (r >  32767) r =  32767;
            if (r < -32768) r = -32768;
            mix[i * 2 + 0] = (int16_t)l;
            mix[i * 2 + 1] = (int16_t)r;
        }
        AUD_write(s->voice, mix, n * 4);
        frames_total -= n;
    }
}

static void virt_ym3438_reset(DeviceState *dev)
{
    VirtYm3438State *s = VIRT_YM3438(dev);
    s->pin_state = YM_PIN_BIT(VIRT_YM3438_PIN_nWR) |
                   YM_PIN_BIT(VIRT_YM3438_PIN_nCS) |
                   YM_PIN_BIT(VIRT_YM3438_PIN_nIC);  /* idle: control lines high */
    if (s->chip) {
        nukedopn2_reset_chip(s->chip);
    }
}

static void virt_ym3438_realize(DeviceState *dev, Error **errp)
{
    VirtYm3438State *s = VIRT_YM3438(dev);

    s->clock_hz = DEFAULT_CLOCK_HZ;
    s->rate_hz  = OUTPUT_RATE_HZ;

    s->chip = nukedopn2_init(s->clock_hz, s->rate_hz);
    if (!s->chip) {
        error_setg(errp, "virt-ym3438: nukedopn2_init failed");
        return;
    }

    if (!AUD_register_card("virt-ym3438", &s->card, errp)) {
        return;
    }
    struct audsettings as = {
        .freq = s->rate_hz,
        .nchannels = 2,
        .fmt = AUDIO_FORMAT_S16,
        .endianness = 0,
    };
    s->voice = AUD_open_out(&s->card, NULL, "virt-ym3438.out", s,
                            virt_ym3438_audio_cb, &as);
    if (s->voice) {
        AUD_set_active_out(s->voice, 1);
    }
    /* No -audio backend → s->voice stays NULL; chip writes still get
     * snooped, but cb is a no-op. SoC realize must succeed regardless. */

    qdev_init_gpio_in(dev, virt_ym3438_gpio_in, VIRT_YM3438_NUM_PINS);
}

static void virt_ym3438_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize   = virt_ym3438_realize;
    device_class_set_legacy_reset(dc, virt_ym3438_reset);
    dc->desc = "Virtual YM3438 (Nuked-OPN2)";
}

static const TypeInfo virt_ym3438_type_info = {
    .name          = TYPE_VIRT_YM3438,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(VirtYm3438State),
    .class_init    = virt_ym3438_class_init,
};

static void virt_ym3438_register_types(void)
{
    type_register_static(&virt_ym3438_type_info);
}

type_init(virt_ym3438_register_types)
