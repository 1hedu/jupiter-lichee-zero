/*
 * Allwinner V3s System on Chip emulation
 *
 * Part of licheeEmu. Derived from hw/arm/allwinner-h3.c (GPLv2+).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Reference: Allwinner V3s Datasheet V1.0, linux-sunxi wiki.
 *
 * Phase 1 scope: CPU, DRAM, GIC, UART0, CCU, SysCtrl, CPUcfg, SID, PIT,
 *                SRAM. Everything else is create_unimplemented_device() so
 *                stray accesses log a warning instead of aborting.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "hw/qdev-core.h"
#include "hw/sysbus.h"
#include "hw/char/serial-mm.h"
#include "hw/misc/unimp.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "hw/arm/allwinner-v3s.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/gtimer.h"

/* Memory map — V3s datasheet §3 and jupiter include/v3s.h */
const hwaddr allwinner_v3s_memmap[] = {
    [AW_V3S_DEV_SRAM_A1]    = 0x00000000,
    [AW_V3S_DEV_SRAM_A2]    = 0x00044000,
    [AW_V3S_DEV_SRAM_C]     = 0x01d00000,
    [AW_V3S_DEV_SYSCTRL]    = 0x01c00000,
    [AW_V3S_DEV_SID]        = 0x01c14000,
    [AW_V3S_DEV_CCU]        = 0x01c20000,
    [AW_V3S_DEV_PIO]        = 0x01c20800,
    [AW_V3S_DEV_PIT]        = 0x01c20c00,
    [AW_V3S_DEV_UART0]      = 0x01c28000,
    [AW_V3S_DEV_UART1]      = 0x01c28400,
    [AW_V3S_DEV_UART2]      = 0x01c28800,
    [AW_V3S_DEV_GIC_DIST]   = 0x01c81000,
    [AW_V3S_DEV_GIC_CPU]    = 0x01c82000,
    [AW_V3S_DEV_GIC_HYP]    = 0x01c84000,
    [AW_V3S_DEV_GIC_VCPU]   = 0x01c86000,
    [AW_V3S_DEV_CPUCFG]     = 0x01c25c00,
    [AW_V3S_DEV_HSTIMER]    = 0x01c60000,
    [AW_V3S_DEV_DMA]        = 0x01c02000,
    [AW_V3S_DEV_CODEC]      = 0x01c22c00,
    [AW_V3S_DEV_MMC0]       = 0x01c0f000,
    [AW_V3S_DEV_CEDAR]      = 0x01c0e000,
    [AW_V3S_DEV_DE2]        = 0x01000000,
    [AW_V3S_DEV_MIXER0]     = 0x01100000,
    [AW_V3S_DEV_TCON0]      = 0x01c0c000,
    [AW_V3S_DEV_SDRAM]      = 0x40000000,
};

/*
 * Unimplemented devices — mapped so stray bare-metal register writes log
 * instead of crashing. Subset of the full V3s MMIO map; addresses match
 * the guest driver expectations in jupiter's include/v3s.h.
 *
 * Phase 2 will peel display blocks (DE2/Mixer/TCON0) off this list and
 * give them real device models.
 */
static const struct { const char *name; hwaddr base; hwaddr size; } v3s_unimp[] = {
    { "spi0",         0x01c05000, 4 * KiB  },
    { "spi1",         0x01c06000, 4 * KiB  },
    { "owa",          0x01c21000, 1 * KiB  },  /* SPDIF */
    { "pwm",          0x01c21400, 1 * KiB  },
    { "keyadc",       0x01c21800, 1 * KiB  },
    { "codec-analog", 0x01c23000, 1 * KiB  },
    { "twi0",         0x01c2ac00, 1 * KiB  },
    { "twi1",         0x01c2b000, 1 * KiB  },
    { "usb0-otg",     0x01c19000, 4 * KiB  },
    { "usb0-phy",     0x01c1a000, 4 * KiB  },
    { "csi0",         0x01cb0000, 320 * KiB },
};

/* Per-CPU IRQs */
enum {
    AW_V3S_GIC_PPI_MAINT     =  9,
    AW_V3S_GIC_PPI_HYPTIMER  = 10,
    AW_V3S_GIC_PPI_VIRTTIMER = 11,
    AW_V3S_GIC_PPI_SECTIMER  = 13,
    AW_V3S_GIC_PPI_PHYSTIMER = 14,
};

static void allwinner_v3s_init(Object *obj)
{
    AwV3sState *s = AW_V3S(obj);

    s->memmap = allwinner_v3s_memmap;

    for (int i = 0; i < AW_V3S_NUM_CPUS; i++) {
        object_initialize_child(obj, "cpu[*]", &s->cpus[i],
                                ARM_CPU_TYPE_NAME("cortex-a7"));
    }

    object_initialize_child(obj, "gic",     &s->gic,     TYPE_ARM_GIC);
    object_initialize_child(obj, "timer",   &s->timer,   TYPE_AW_A10_PIT);
    object_property_add_alias(obj, "clk0-freq", OBJECT(&s->timer), "clk0-freq");
    object_property_add_alias(obj, "clk1-freq", OBJECT(&s->timer), "clk1-freq");

    object_initialize_child(obj, "ccu",     &s->ccu,     TYPE_AW_H3_CCU);
    object_initialize_child(obj, "sysctrl", &s->sysctrl, TYPE_AW_H3_SYSCTRL);
    object_initialize_child(obj, "cpucfg",  &s->cpucfg,  TYPE_AW_CPUCFG);
    object_initialize_child(obj, "sid",     &s->sid,     TYPE_AW_SID);
    object_property_add_alias(obj, "identifier", OBJECT(&s->sid), "identifier");

    object_initialize_child(obj, "display", &s->display, TYPE_AW_V3S_DISPLAY);
    object_initialize_child(obj, "pio",     &s->pio,     TYPE_AW_V3S_PIO);
    object_initialize_child(obj, "hstimer", &s->hstimer, TYPE_AW_V3S_HSTIMER);
    object_initialize_child(obj, "dma",     &s->dma,     TYPE_AW_V3S_DMA);
    object_initialize_child(obj, "codec",   &s->codec,   TYPE_AW_V3S_CODEC);
    object_initialize_child(obj, "mmc0",    &s->mmc0,    TYPE_AW_SDHOST_SUN5I);
    object_initialize_child(obj, "cedar",   &s->cedar,   TYPE_AW_V3S_CEDAR);
    object_initialize_child(obj, "ym3438",  &s->ym3438,  TYPE_VIRT_YM3438);
}

static void allwinner_v3s_realize(DeviceState *dev, Error **errp)
{
    AwV3sState *s = AW_V3S(dev);
    unsigned i;

    /* CPU */
    for (i = 0; i < AW_V3S_NUM_CPUS; i++) {
        qdev_prop_set_bit(DEVICE(&s->cpus[i]), "has_el3", true);
        qdev_prop_set_bit(DEVICE(&s->cpus[i]), "has_el2", true);
        qdev_realize(DEVICE(&s->cpus[i]), NULL, &error_fatal);
    }

    /* GIC v2 */
    qdev_prop_set_uint32(DEVICE(&s->gic), "num-irq",
                         AW_V3S_GIC_NUM_SPI + GIC_INTERNAL);
    qdev_prop_set_uint32(DEVICE(&s->gic), "revision", 2);
    qdev_prop_set_uint32(DEVICE(&s->gic), "num-cpu", AW_V3S_NUM_CPUS);
    qdev_prop_set_bit(DEVICE(&s->gic), "has-security-extensions", false);
    qdev_prop_set_bit(DEVICE(&s->gic), "has-virtualization-extensions", true);
    sysbus_realize(SYS_BUS_DEVICE(&s->gic), &error_fatal);

    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 0, s->memmap[AW_V3S_DEV_GIC_DIST]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 1, s->memmap[AW_V3S_DEV_GIC_CPU]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 2, s->memmap[AW_V3S_DEV_GIC_HYP]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 3, s->memmap[AW_V3S_DEV_GIC_VCPU]);

    for (i = 0; i < AW_V3S_NUM_CPUS; i++) {
        DeviceState *cpudev = DEVICE(&s->cpus[i]);
        int ppibase = AW_V3S_GIC_NUM_SPI + i * GIC_INTERNAL + GIC_NR_SGIS;
        const int timer_irq[] = {
            [GTIMER_PHYS] = AW_V3S_GIC_PPI_PHYSTIMER,
            [GTIMER_VIRT] = AW_V3S_GIC_PPI_VIRTTIMER,
            [GTIMER_HYP]  = AW_V3S_GIC_PPI_HYPTIMER,
            [GTIMER_SEC]  = AW_V3S_GIC_PPI_SECTIMER,
        };
        for (int t = 0; t < ARRAY_SIZE(timer_irq); t++) {
            qdev_connect_gpio_out(cpudev, t,
                qdev_get_gpio_in(DEVICE(&s->gic), ppibase + timer_irq[t]));
        }
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i,
                           qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + AW_V3S_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + (2 * AW_V3S_NUM_CPUS),
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + (3 * AW_V3S_NUM_CPUS),
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + (4 * AW_V3S_NUM_CPUS),
                           qdev_get_gpio_in(DEVICE(&s->gic),
                                            ppibase + AW_V3S_GIC_PPI_MAINT));
    }

    /* PIT (reused from A10 model) */
    sysbus_realize(SYS_BUS_DEVICE(&s->timer), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->timer), 0, s->memmap[AW_V3S_DEV_PIT]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->timer), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_V3S_GIC_SPI_TIMER0));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->timer), 1,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_V3S_GIC_SPI_TIMER1));

    /* On-chip SRAMs. Sizes are V3s-specific (smaller than H3). */
    memory_region_init_ram(&s->sram_a1, OBJECT(dev), "v3s.sram-a1",
                           48 * KiB, &error_abort);
    memory_region_init_ram(&s->sram_a2, OBJECT(dev), "v3s.sram-a2",
                           16 * KiB, &error_abort);
    memory_region_init_ram(&s->sram_c,  OBJECT(dev), "v3s.sram-c",
                           64 * KiB, &error_abort);
    memory_region_add_subregion(get_system_memory(),
        s->memmap[AW_V3S_DEV_SRAM_A1], &s->sram_a1);
    memory_region_add_subregion(get_system_memory(),
        s->memmap[AW_V3S_DEV_SRAM_A2], &s->sram_a2);
    memory_region_add_subregion(get_system_memory(),
        s->memmap[AW_V3S_DEV_SRAM_C], &s->sram_c);

    /* CCU — reuse H3 model. Matches V3s PLL layout close enough that the
     * PLL_LOCK bit (bit 28) is set on read after write, which is all the
     * guest driver polls for. */
    sysbus_realize(SYS_BUS_DEVICE(&s->ccu), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccu), 0, s->memmap[AW_V3S_DEV_CCU]);

    sysbus_realize(SYS_BUS_DEVICE(&s->sysctrl), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sysctrl), 0, s->memmap[AW_V3S_DEV_SYSCTRL]);

    sysbus_realize(SYS_BUS_DEVICE(&s->cpucfg), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->cpucfg), 0, s->memmap[AW_V3S_DEV_CPUCFG]);

    sysbus_realize(SYS_BUS_DEVICE(&s->sid), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sid), 0, s->memmap[AW_V3S_DEV_SID]);

    /* Display pipeline: DE2 + Mixer0 + TCON0 all behind one device. */
    sysbus_realize(SYS_BUS_DEVICE(&s->display), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->display), 0, s->memmap[AW_V3S_DEV_DE2]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->display), 1, s->memmap[AW_V3S_DEV_MIXER0]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->display), 2, s->memmap[AW_V3S_DEV_TCON0]);

    /* PIO — pure GPIO. Virtual peripherals attach via input lines. */
    sysbus_realize(SYS_BUS_DEVICE(&s->pio), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->pio), 0, s->memmap[AW_V3S_DEV_PIO]);

    /* HSTimer (2 channels) — IRQs to GIC SPI 51/52. */
    sysbus_realize(SYS_BUS_DEVICE(&s->hstimer), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->hstimer), 0,
                    s->memmap[AW_V3S_DEV_HSTIMER]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->hstimer), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic),
                                        AW_V3S_GIC_SPI_HSTMR0));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->hstimer), 1,
                       qdev_get_gpio_in(DEVICE(&s->gic),
                                        AW_V3S_GIC_SPI_HSTMR1));

    /* DMA engine (8 channels). Shared IRQ → GIC SPI 50. */
    sysbus_realize(SYS_BUS_DEVICE(&s->dma), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->dma), 0, s->memmap[AW_V3S_DEV_DMA]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->dma), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic),
                                        AW_V3S_GIC_SPI_DMA));

    /* Audio codec (DAC). MMIO traps TXDATA writes from the DMA engine. */
    sysbus_realize(SYS_BUS_DEVICE(&s->codec), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->codec), 0, s->memmap[AW_V3S_DEV_CODEC]);
    aw_v3s_codec_attach_dma(&s->codec, &s->dma);

    /* SDMMC0 — register-compatible with the sun5i model used by H3.
     * IRQ is on GIC SPI 23. Jupiter's sdmmc.c is purely polled so the
     * IRQ wiring is forward-looking. */
    object_property_set_link(OBJECT(&s->mmc0), "dma-memory",
                             OBJECT(get_system_memory()), &error_fatal);
    sysbus_realize(SYS_BUS_DEVICE(&s->mmc0), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->mmc0), 0, s->memmap[AW_V3S_DEV_MMC0]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->mmc0), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic),
                                        AW_V3S_GIC_SPI_MMC0));
    object_property_add_alias(OBJECT(s), "sd-bus", OBJECT(&s->mmc0),
                              "sd-bus");

    /* Cedar VE — H.264 decoder backed by host libavcodec. */
    sysbus_realize(SYS_BUS_DEVICE(&s->cedar), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->cedar), 0,
                    s->memmap[AW_V3S_DEV_CEDAR]);

    /* Virtual YM3438 (Nuked-OPN2). Snoops PB0-7 + PG0-4 GPIO transitions
     * from the PIO and feeds Nuked-OPN2; output goes to the host audio
     * backend. PIO.pin_out[port*32+pin] → ym3438.gpio_in[line]. */
    sysbus_realize(SYS_BUS_DEVICE(&s->ym3438), &error_fatal);
    {
        DeviceState *pio_dev = DEVICE(&s->pio);
        DeviceState *ym_dev  = DEVICE(&s->ym3438);
        /* PB = port 1 → PB0..PB7 = lines 32..39 on PIO; map to ym3438 0..7 */
        for (int p = 0; p < 8; p++) {
            qdev_connect_gpio_out(pio_dev, 1 * 32 + p,
                                  qdev_get_gpio_in(ym_dev,
                                                   VIRT_YM3438_PIN_D0 + p));
        }
        /* PG = port 6 → PG0..PG4 = lines 192..196 on PIO */
        qdev_connect_gpio_out(pio_dev, 6 * 32 + 0,
                              qdev_get_gpio_in(ym_dev, VIRT_YM3438_PIN_A0));
        qdev_connect_gpio_out(pio_dev, 6 * 32 + 1,
                              qdev_get_gpio_in(ym_dev, VIRT_YM3438_PIN_A1));
        qdev_connect_gpio_out(pio_dev, 6 * 32 + 2,
                              qdev_get_gpio_in(ym_dev, VIRT_YM3438_PIN_nWR));
        qdev_connect_gpio_out(pio_dev, 6 * 32 + 3,
                              qdev_get_gpio_in(ym_dev, VIRT_YM3438_PIN_nCS));
        qdev_connect_gpio_out(pio_dev, 6 * 32 + 4,
                              qdev_get_gpio_in(ym_dev, VIRT_YM3438_PIN_nIC));
    }

    /* UART0 — 16550-compatible, 4-byte register stride, DLAB for baud. */
    serial_mm_init(get_system_memory(), s->memmap[AW_V3S_DEV_UART0], 2,
                   qdev_get_gpio_in(DEVICE(&s->gic), AW_V3S_GIC_SPI_UART0),
                   115200, serial_hd(0), DEVICE_NATIVE_ENDIAN);
    serial_mm_init(get_system_memory(), s->memmap[AW_V3S_DEV_UART1], 2,
                   qdev_get_gpio_in(DEVICE(&s->gic), AW_V3S_GIC_SPI_UART1),
                   115200, serial_hd(1), DEVICE_NATIVE_ENDIAN);
    serial_mm_init(get_system_memory(), s->memmap[AW_V3S_DEV_UART2], 2,
                   qdev_get_gpio_in(DEVICE(&s->gic), AW_V3S_GIC_SPI_UART2),
                   115200, serial_hd(2), DEVICE_NATIVE_ENDIAN);

    for (i = 0; i < ARRAY_SIZE(v3s_unimp); i++) {
        create_unimplemented_device(v3s_unimp[i].name,
                                    v3s_unimp[i].base,
                                    v3s_unimp[i].size);
    }
}

static void allwinner_v3s_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = allwinner_v3s_realize;
    dc->user_creatable = false;  /* serial_hd() in realize */
}

static const TypeInfo allwinner_v3s_type_info = {
    .name          = TYPE_AW_V3S,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(AwV3sState),
    .instance_init = allwinner_v3s_init,
    .class_init    = allwinner_v3s_class_init,
};

static void allwinner_v3s_register_types(void)
{
    type_register_static(&allwinner_v3s_type_info);
}

type_init(allwinner_v3s_register_types)
