#ifndef JUPITER_PINE64_TUSB_CONFIG_H
#define JUPITER_PINE64_TUSB_CONFIG_H

#define CFG_TUSB_MCU                 OPT_MCU_NONE
#define CFG_TUSB_OS                  OPT_OS_NONE
#define CFG_TUSB_DEBUG               0
#define CFG_TUSB_RHPORT0_MODE         (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#define CFG_TUD_ENABLED              0
#define CFG_TUH_ENABLED              1
#define TUP_USBIP_OHCI
#define TUP_OHCI_RHPORTS              1

/* One selected A64 OHCI controller. Full/low-speed HID; one external hub
 * is supported by the stack, but a direct boot keyboard is recommended. */
#define CFG_TUH_DEVICE_MAX           4
#define CFG_TUH_ENDPOINT_MAX         8
#define CFG_TUH_HUB                  1
#define CFG_TUH_HID                  4
#define CFG_TUH_CDC                  0
#define CFG_TUH_MSC                  0
#define CFG_TUH_MIDI                 0
#define CFG_TUH_VENDOR               0
#define CFG_TUH_ENUMERATION_BUFSIZE  1024
#define CFG_TUH_HID_EPIN_BUFSIZE      64
#define CFG_TUH_HID_EPOUT_BUFSIZE     64

/* Linker and MMU reserve 0x58000000..0x580fffff as Normal NC, shared,
 * execute-never. Includes HCCA, all ED/TDs, setup packets, enumeration,
 * hub and HID report buffers. No cacheable aliases of USB DMA memory. */
#define CFG_TUH_MEM_SECTION          __attribute__((section(".bss_usb")))
#define CFG_TUH_MEM_ALIGN            __attribute__((aligned(64)))
#define CFG_TUH_MEM_DCACHE_ENABLE    0

#endif
