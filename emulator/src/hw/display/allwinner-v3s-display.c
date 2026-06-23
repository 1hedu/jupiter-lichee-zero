/*
 * Allwinner V3s display pipeline (DE2 + Mixer0 + TCON0)
 *
 * Scope: render Jupiter's VI0 (game world, XRGB8888, 480x272) layer with
 * optional UI0 (overlay, ARGB8888, per-pixel alpha) composited on top, to
 * a QEMU graphic console. All other layers/channels ignored.
 *
 * Not a register-accurate model of the Allwinner DE2 — every write is
 * recorded verbatim and only the handful of bits Jupiter's video.c relies
 * on are interpreted on scan-out.
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
#include "exec/address-spaces.h"
#include "sysemu/dma.h"
#include "ui/console.h"
#include "hw/display/allwinner-v3s-display.h"

/*
 * Register offsets within the 48KB mixer region at 0x01100000.
 * Matches Jupiter's include/v3s.h exactly.
 */
#define MIX_GLB_CTL         0x0000   /* bit 0: enable mixer           */
#define MIX_GLB_SIZE        0x000C   /* WH(w-1, h-1) layout           */
#define MIX_GLB_DBUF        0x0008   /* bit 0: commit double-buffer   */

/* VI0 overlay at mixer + 0x2000. Channel 0 = game layer. */
#define VI0_ATTR(ov)        (0x2000 + 0x30 * (ov))  /* bit 0: enable, bits 8-12: fmt */
#define VI0_MBSIZE(ov)      (0x2004 + 0x30 * (ov))
#define VI0_COOR(ov)        (0x2008 + 0x30 * (ov))
#define VI0_PITCH0(ov)      (0x200C + 0x30 * (ov))
#define VI0_TOP_LADDR0(ov)  (0x2018 + 0x30 * (ov))  /* guest FB addr */

/* UI0 overlay at mixer + 0x4000 (channel 2 per V3s layout). */
#define UI0_ATTR(ov)        (0x4000 + 0x20 * (ov))  /* bit 0: enable, bits 24-31: galpha */
#define UI0_SIZE(ov)        (0x4004 + 0x20 * (ov))
#define UI0_COORD(ov)       (0x4008 + 0x20 * (ov))
#define UI0_PITCH(ov)       (0x400C + 0x20 * (ov))
#define UI0_ADDR(ov)        (0x4010 + 0x20 * (ov))

/* TCON0 registers (at 0x01C0C000). */
#define TCON_GCTL           0x00     /* bit 31: global enable */
#define TCON0_CTL           0x40     /* bit 31: ch0 enable    */

#define BIT31               (1U << 31)

/* ---------- helpers -------------------------------------------------- */

static bool display_active(AwV3sDisplayState *s)
{
    /* Jupiter considers the display live when both TCON_GCTL and TCON0_CTL
     * have their enable bit set. Before that we show black. */
    return (s->tcon[TCON_GCTL / 4]  & BIT31)
        && (s->tcon[TCON0_CTL  / 4] & BIT31);
}

/* Composite one scanline of VI0 (XRGB8888) with optional UI0 alpha on top. */
static void composite_line(uint32_t *dst,
                           const uint32_t *vi_line,
                           const uint32_t *ui_line,
                           bool ui_enabled,
                           uint8_t ui_galpha)
{
    int x;
    if (!ui_enabled) {
        for (x = 0; x < AW_V3S_LCD_W; x++) {
            dst[x] = 0xFF000000u | (vi_line[x] & 0x00FFFFFFu);
        }
        return;
    }
    for (x = 0; x < AW_V3S_LCD_W; x++) {
        uint32_t bg = vi_line[x];
        uint32_t ov = ui_line[x];
        uint32_t a  = (ov >> 24) & 0xFF;
        if (ui_galpha < 0xFF) {
            a = (a * ui_galpha) / 255;
        }
        if (a == 0) {
            dst[x] = 0xFF000000u | (bg & 0x00FFFFFFu);
            continue;
        }
        if (a == 0xFF) {
            dst[x] = 0xFF000000u | (ov & 0x00FFFFFFu);
            continue;
        }
        uint32_t br = (bg >> 16) & 0xFF, bgG = (bg >> 8) & 0xFF, bb = bg & 0xFF;
        uint32_t orC = (ov >> 16) & 0xFF, ogC = (ov >> 8) & 0xFF, ob = ov & 0xFF;
        uint32_t ia = 255 - a;
        uint32_t r = (orC * a + br  * ia) / 255;
        uint32_t g = (ogC * a + bgG * ia) / 255;
        uint32_t b = (ob  * a + bb  * ia) / 255;
        dst[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

/* ---------- display callback ---------------------------------------- */

static void aw_v3s_display_update(void *opaque)
{
    AwV3sDisplayState *s = opaque;
    DisplaySurface *surface = qemu_console_surface(s->con);
    uint32_t *dst = surface_data(surface);
    uint32_t vi_line[AW_V3S_LCD_W];
    uint32_t ui_line[AW_V3S_LCD_W];
    int y;

    if (!display_active(s)) {
        memset(dst, 0, AW_V3S_LCD_W * AW_V3S_LCD_H * 4);
        dpy_gfx_update_full(s->con);
        s->invalidate = false;
        return;
    }

    uint32_t vi_attr = s->mix[VI0_ATTR(0)     / 4];
    uint32_t vi_addr = s->mix[VI0_TOP_LADDR0(0) / 4];
    uint32_t ui_attr = s->mix[UI0_ATTR(0)     / 4];
    uint32_t ui_addr = s->mix[UI0_ADDR(0)     / 4];

    bool    vi_en = (vi_attr & 1) && vi_addr != 0;
    bool    ui_en = (ui_attr & 1) && ui_addr != 0;
    uint8_t ui_ga = (ui_attr >> 24) & 0xFF;

    if (!vi_en) {
        memset(dst, 0, AW_V3S_LCD_W * AW_V3S_LCD_H * 4);
        dpy_gfx_update_full(s->con);
        s->invalidate = false;
        return;
    }

    for (y = 0; y < AW_V3S_LCD_H; y++) {
        hwaddr vi_off = (hwaddr)vi_addr + (hwaddr)y * AW_V3S_LCD_W * 4;
        address_space_read(&address_space_memory, vi_off,
                           MEMTXATTRS_UNSPECIFIED,
                           vi_line, sizeof vi_line);
        if (ui_en) {
            hwaddr ui_off = (hwaddr)ui_addr + (hwaddr)y * AW_V3S_LCD_W * 4;
            address_space_read(&address_space_memory, ui_off,
                               MEMTXATTRS_UNSPECIFIED,
                               ui_line, sizeof ui_line);
        }
        composite_line(dst + y * AW_V3S_LCD_W,
                       vi_line, ui_line, ui_en, ui_ga);
    }

    dpy_gfx_update_full(s->con);
    s->invalidate = false;
}

static void aw_v3s_display_invalidate(void *opaque)
{
    AwV3sDisplayState *s = opaque;
    s->invalidate = true;
}

static const GraphicHwOps aw_v3s_display_ops = {
    .invalidate = aw_v3s_display_invalidate,
    .gfx_update = aw_v3s_display_update,
};

/* ---------- MMIO regions -------------------------------------------- */

static uint64_t de2_read(void *o, hwaddr off, unsigned sz)
{
    AwV3sDisplayState *s = o;
    if (off + sz > AW_V3S_DE2_SIZE) {
        return 0;
    }
    return s->de2[off / 4];
}

static void de2_write(void *o, hwaddr off, uint64_t val, unsigned sz)
{
    AwV3sDisplayState *s = o;
    if (off + sz > AW_V3S_DE2_SIZE) {
        return;
    }
    s->de2[off / 4] = (uint32_t)val;
}

static uint64_t mix_read(void *o, hwaddr off, unsigned sz)
{
    AwV3sDisplayState *s = o;
    if (off + sz > AW_V3S_MIX_SIZE) {
        return 0;
    }
    return s->mix[off / 4];
}

static void mix_write(void *o, hwaddr off, uint64_t val, unsigned sz)
{
    AwV3sDisplayState *s = o;
    if (off + sz > AW_V3S_MIX_SIZE) {
        return;
    }
    s->mix[off / 4] = (uint32_t)val;
    s->invalidate = true;
}

static uint64_t tcon_read(void *o, hwaddr off, unsigned sz)
{
    AwV3sDisplayState *s = o;
    if (off + sz > AW_V3S_TCON_SIZE) {
        return 0;
    }
    return s->tcon[off / 4];
}

static void tcon_write(void *o, hwaddr off, uint64_t val, unsigned sz)
{
    AwV3sDisplayState *s = o;
    if (off + sz > AW_V3S_TCON_SIZE) {
        return;
    }
    s->tcon[off / 4] = (uint32_t)val;
    s->invalidate = true;
}

#define MMIO_OPS(name)                                           \
    static const MemoryRegionOps name##_ops = {                  \
        .read = name##_read, .write = name##_write,              \
        .valid = { .min_access_size = 4, .max_access_size = 4 }, \
        .impl  = { .min_access_size = 4, .max_access_size = 4 }, \
        .endianness = DEVICE_NATIVE_ENDIAN,                      \
    }

MMIO_OPS(de2);
MMIO_OPS(mix);
MMIO_OPS(tcon);

/* ---------- QOM boilerplate ----------------------------------------- */

static void aw_v3s_display_reset(DeviceState *dev)
{
    AwV3sDisplayState *s = AW_V3S_DISPLAY(dev);
    memset(s->de2,  0, sizeof s->de2);
    memset(s->mix,  0, sizeof s->mix);
    memset(s->tcon, 0, sizeof s->tcon);
    s->invalidate = true;
}

static void aw_v3s_display_realize(DeviceState *dev, Error **errp)
{
    AwV3sDisplayState *s = AW_V3S_DISPLAY(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->de2_mr,  OBJECT(dev), &de2_ops,  s,
                          "allwinner-v3s-de2",   AW_V3S_DE2_SIZE);
    memory_region_init_io(&s->mix_mr,  OBJECT(dev), &mix_ops,  s,
                          "allwinner-v3s-mix",   AW_V3S_MIX_SIZE);
    memory_region_init_io(&s->tcon_mr, OBJECT(dev), &tcon_ops, s,
                          "allwinner-v3s-tcon0", AW_V3S_TCON_SIZE);

    sysbus_init_mmio(sbd, &s->de2_mr);
    sysbus_init_mmio(sbd, &s->mix_mr);
    sysbus_init_mmio(sbd, &s->tcon_mr);

    s->con = graphic_console_init(dev, 0, &aw_v3s_display_ops, s);
    qemu_console_resize(s->con, AW_V3S_LCD_W, AW_V3S_LCD_H);
}

static void aw_v3s_display_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = aw_v3s_display_realize;
    device_class_set_legacy_reset(dc, aw_v3s_display_reset);
    dc->desc = "Allwinner V3s display pipeline (DE2+Mixer+TCON0)";
}

static const TypeInfo aw_v3s_display_type_info = {
    .name          = TYPE_AW_V3S_DISPLAY,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AwV3sDisplayState),
    .class_init    = aw_v3s_display_class_init,
};

static void aw_v3s_display_register_types(void)
{
    type_register_static(&aw_v3s_display_type_info);
}

type_init(aw_v3s_display_register_types)
