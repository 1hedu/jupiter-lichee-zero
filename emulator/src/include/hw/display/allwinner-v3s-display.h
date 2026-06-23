/*
 * Allwinner V3s display pipeline (DE2 + Mixer0 + TCON0)
 *
 * Models enough of the hardware to scan a VI0 (game world, XRGB8888) +
 * UI0 (overlay, ARGB8888) layer pair into a QEMU graphic console.
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_DISPLAY_ALLWINNER_V3S_DISPLAY_H
#define HW_DISPLAY_ALLWINNER_V3S_DISPLAY_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "ui/console.h"

#define TYPE_AW_V3S_DISPLAY "allwinner-v3s-display"
OBJECT_DECLARE_SIMPLE_TYPE(AwV3sDisplayState, AW_V3S_DISPLAY)

/* Register region sizes carved out of the wider MMIO map. The real V3s
 * reserves more for each, but these cover what Jupiter's video.c touches
 * plus room for the full blender + first three overlay channels. */
#define AW_V3S_DE2_SIZE    0x100     /* 0x01000000: DE2 top-level clocks */
#define AW_V3S_MIX_SIZE    0xC000    /* 0x01100000: mixer + layers + blend */
#define AW_V3S_TCON_SIZE   0x200     /* 0x01C0C000: TCON0                  */

#define AW_V3S_LCD_W       480
#define AW_V3S_LCD_H       272

struct AwV3sDisplayState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion de2_mr;
    MemoryRegion mix_mr;
    MemoryRegion tcon_mr;

    uint32_t de2[AW_V3S_DE2_SIZE / 4];
    uint32_t mix[AW_V3S_MIX_SIZE / 4];
    uint32_t tcon[AW_V3S_TCON_SIZE / 4];

    QemuConsole *con;
    bool invalidate;
};

#endif /* HW_DISPLAY_ALLWINNER_V3S_DISPLAY_H */
