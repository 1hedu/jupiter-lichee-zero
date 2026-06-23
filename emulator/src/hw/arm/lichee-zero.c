/*
 * Lichee Pi Zero board (Allwinner V3s)
 *
 * Part of licheeEmu. Bare-metal target — expects a flat binary loaded via
 * -kernel at 0x41000000, which is where U-Boot's `go 0x41000000` jumps to
 * after SPL + U-Boot proper on real hardware.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/loader.h"
#include "hw/sd/sd.h"
#include "sysemu/blockdev.h"
#include "sysemu/block-backend.h"
#include "sysemu/reset.h"
#include "hw/arm/allwinner-v3s.h"
#include "hw/input/virt-n64-pad.h"

/* PE20 — single-wire N64 data line on the Lichee Pi Zero's pin header
 * as convention'd by the Jupiter SDK's lib/input.c. */
#define LICHEE_ZERO_N64_PORT     4      /* port E */
#define LICHEE_ZERO_N64_PIN      20
#define LICHEE_ZERO_N64_PIO_LINE (LICHEE_ZERO_N64_PORT * 32 + LICHEE_ZERO_N64_PIN)

/* Physical entry point: U-Boot `go 0x41000000` jumps here. */
#define LICHEE_ZERO_PAYLOAD_ADDR   0x41000000ULL

static void lichee_zero_cpu_reset(void *opaque)
{
    ARMCPU *cpu = opaque;

    cpu_reset(CPU(cpu));
    /* Start bare-metal payload at the U-Boot handoff address. */
    cpu_set_pc(CPU(cpu), LICHEE_ZERO_PAYLOAD_ADDR);
}

static void lichee_zero_init(MachineState *machine)
{
    AwV3sState *soc;

    if (machine->firmware) {
        error_report("BIOS not supported for this machine");
        exit(1);
    }
    if (machine->ram_size != 64 * MiB) {
        error_report("lichee-zero requires exactly 64 MiB of RAM "
                     "(got %" PRIu64 " MiB)", machine->ram_size / MiB);
        exit(1);
    }

    soc = AW_V3S(object_new(TYPE_AW_V3S));
    object_property_add_child(OBJECT(machine), "soc", OBJECT(soc));
    object_unref(OBJECT(soc));

    /* Clocks: clk0 = 32.768 kHz (LOSC), clk1 = 24 MHz (HOSC). */
    object_property_set_int(OBJECT(soc), "clk0-freq", 32768, &error_abort);
    object_property_set_int(OBJECT(soc), "clk1-freq", 24 * 1000 * 1000,
                            &error_abort);

    qdev_realize(DEVICE(soc), NULL, &error_abort);

    /* Plug an SD card image (if -drive if=sd,file=... was passed) into
     * the SoC's sd-bus. Matches orangepi-pc's pattern. */
    {
        DriveInfo *di = drive_get(IF_SD, 0, 0);
        BlockBackend *blk = di ? blk_by_legacy_dinfo(di) : NULL;
        BusState *bus = qdev_get_child_bus(DEVICE(soc), "sd-bus");
        DeviceState *carddev = qdev_new(TYPE_SD_CARD);
        qdev_prop_set_drive_err(carddev, "drive", blk, &error_fatal);
        qdev_realize_and_unref(carddev, bus, &error_fatal);
    }

    /* DDR2 DRAM at 0x40000000. */
    memory_region_add_subregion(get_system_memory(),
                                soc->memmap[AW_V3S_DEV_SDRAM],
                                machine->ram);

    /*
     * Bare-metal load: drop -kernel verbatim at 0x41000000 and force the
     * CPU to start there on reset. This matches U-Boot `go 0x41000000` on
     * real hardware. No ELF parsing, no DTB, no PSCI.
     */
    if (machine->kernel_filename) {
        const hwaddr load_at = LICHEE_ZERO_PAYLOAD_ADDR;
        const hwaddr sdram_base = soc->memmap[AW_V3S_DEV_SDRAM];
        int64_t max = machine->ram_size - (load_at - sdram_base);
        int64_t sz = load_image_targphys(machine->kernel_filename,
                                         load_at, max);
        if (sz < 0) {
            error_report("could not load -kernel '%s' at 0x%" HWADDR_PRIx,
                         machine->kernel_filename, load_at);
            exit(1);
        }
    }

    /*
     * Attach a virtual N64 pad to PE20, matching the wiring the Jupiter
     * SDK's lib/input.c expects when initialized with INPUT_N64. Keyboard
     * → controller mapping lives in the pad device. Hardcoded for now;
     * future work can add -machine lichee-zero,controller=snes|genesis
     * variants.
     */
    {
        DeviceState *pad = qdev_new(TYPE_VIRT_N64_PAD);
        object_property_add_child(OBJECT(machine), "n64pad", OBJECT(pad));
        qdev_realize_and_unref(pad, NULL, &error_abort);

        /* pad drives the PIO's ext_in for PE20 */
        qdev_connect_gpio_out(pad, 0,
            qdev_get_gpio_in(DEVICE(&soc->pio), LICHEE_ZERO_N64_PIO_LINE));
        /* PIO notifies pad when the guest rewrites PE20 */
        qdev_connect_gpio_out(DEVICE(&soc->pio), LICHEE_ZERO_N64_PIO_LINE,
            qdev_get_gpio_in(pad, 0));
    }

    qemu_register_reset(lichee_zero_cpu_reset, &soc->cpus[0]);
}

static void lichee_zero_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-a7"),
        NULL
    };

    mc->desc = "Lichee Pi Zero (Allwinner V3s, Cortex-A7)";
    mc->init = lichee_zero_init;
    mc->min_cpus = AW_V3S_NUM_CPUS;
    mc->max_cpus = AW_V3S_NUM_CPUS;
    mc->default_cpus = AW_V3S_NUM_CPUS;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a7");
    mc->valid_cpu_types = valid_cpu_types;
    mc->default_ram_size = 64 * MiB;
    mc->default_ram_id = "lichee-zero.ram";
}

DEFINE_MACHINE("lichee-zero", lichee_zero_machine_init)
