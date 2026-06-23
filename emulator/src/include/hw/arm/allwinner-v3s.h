/*
 * Allwinner V3s System on Chip emulation
 *
 * Part of licheeEmu — bare-metal QEMU target for the Lichee Pi Zero.
 * Derived from hw/arm/allwinner-h3.h (Niek Linnenbank, GPLv2+).
 *
 * The V3s is a single-core Cortex-A7 SoC. Compared to the H3:
 *  - 1 CPU core (H3 has 4)
 *  - No HDMI / no GbE / no USB host / no NAND
 *  - 4.3" RGB parallel LCD via TCON0 + DE2 (VI0 + UI0 layers)
 *  - DDR2 DRAM at 0x40000000, typical 64 MiB
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_ARM_ALLWINNER_V3S_H
#define HW_ARM_ALLWINNER_V3S_H

#include "qom/object.h"
#include "hw/timer/allwinner-a10-pit.h"
#include "hw/intc/arm_gic.h"
#include "hw/misc/allwinner-h3-ccu.h"
#include "hw/misc/allwinner-h3-sysctrl.h"
#include "hw/misc/allwinner-cpucfg.h"
#include "hw/misc/allwinner-sid.h"
#include "hw/sd/allwinner-sdhost.h"
#include "hw/misc/allwinner-v3s-cedar.h"
#include "hw/display/allwinner-v3s-display.h"
#include "hw/gpio/allwinner-v3s-pio.h"
#include "hw/timer/allwinner-v3s-hstimer.h"
#include "hw/dma/allwinner-v3s-dma.h"
#include "hw/audio/allwinner-v3s-codec.h"
#include "hw/audio/virt-ym3438.h"
#include "target/arm/cpu.h"

enum {
    AW_V3S_DEV_SRAM_A1,
    AW_V3S_DEV_SRAM_A2,
    AW_V3S_DEV_SRAM_C,
    AW_V3S_DEV_SYSCTRL,
    AW_V3S_DEV_SID,
    AW_V3S_DEV_CCU,
    AW_V3S_DEV_PIO,
    AW_V3S_DEV_PIT,
    AW_V3S_DEV_HSTIMER,
    AW_V3S_DEV_DMA,
    AW_V3S_DEV_CODEC,
    AW_V3S_DEV_MMC0,
    AW_V3S_DEV_CEDAR,
    AW_V3S_DEV_UART0,
    AW_V3S_DEV_UART1,
    AW_V3S_DEV_UART2,
    AW_V3S_DEV_GIC_DIST,
    AW_V3S_DEV_GIC_CPU,
    AW_V3S_DEV_GIC_HYP,
    AW_V3S_DEV_GIC_VCPU,
    AW_V3S_DEV_CPUCFG,
    AW_V3S_DEV_DE2,
    AW_V3S_DEV_MIXER0,
    AW_V3S_DEV_TCON0,
    AW_V3S_DEV_SDRAM,
};

#define AW_V3S_NUM_CPUS          (1)
#define AW_V3S_GIC_NUM_SPI       (128)

/* GIC SPI IRQ numbers the guest driver expects (see jupiter v3s.h) */
enum {
    AW_V3S_GIC_SPI_UART0    =  0,
    AW_V3S_GIC_SPI_UART1    =  1,
    AW_V3S_GIC_SPI_UART2    =  2,
    AW_V3S_GIC_SPI_TIMER0   = 18,
    AW_V3S_GIC_SPI_TIMER1   = 19,
    AW_V3S_GIC_SPI_MMC0     = 23,
    AW_V3S_GIC_SPI_DMA      = 50,
    AW_V3S_GIC_SPI_HSTMR0   = 51,
    AW_V3S_GIC_SPI_HSTMR1   = 52,
};

#define TYPE_AW_V3S "allwinner-v3s"
OBJECT_DECLARE_SIMPLE_TYPE(AwV3sState, AW_V3S)

struct AwV3sState {
    /*< private >*/
    DeviceState parent_obj;
    /*< public >*/

    ARMCPU cpus[AW_V3S_NUM_CPUS];
    const hwaddr *memmap;

    AwA10PITState timer;
    AwH3ClockCtlState ccu;
    AwH3SysCtrlState sysctrl;
    AwCpuCfgState cpucfg;
    AwSidState sid;
    AwV3sDisplayState display;
    AwV3sPioState pio;
    AwV3sHsTimerState hstimer;
    AwV3sDmaState dma;
    AwV3sCodecState codec;
    AwSdHostState mmc0;
    AwV3sCedarState cedar;
    VirtYm3438State ym3438;
    GICState gic;

    MemoryRegion sram_a1;
    MemoryRegion sram_a2;
    MemoryRegion sram_c;
};

#endif /* HW_ARM_ALLWINNER_V3S_H */
