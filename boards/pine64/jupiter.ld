/* PINE A64+ 2 GiB: bare-metal AArch32 payload loaded by U-Boot bootm.
 * Keep the boot entry and VRAM addresses validated by first boot. The menu
 * has substantially more static state, so place its BSS at 0x50000000 and
 * reserve 0x43000000..0x437fffff for the board-selected Cedar buffers.
 * TF-A owns secure SRAM and secondary CPUs; only CPU0 runs the application.
 */
ENTRY(_start)

MEMORY
{
    BSS_LOW (rw)  : ORIGIN = 0x40200000, LENGTH = 14M
    CODE    (rwx) : ORIGIN = 0x41000000, LENGTH = 32M
    VRAM    (rw)  : ORIGIN = 0x43800000, LENGTH = 8M
    BSS_HIGH(rw)  : ORIGIN = 0x50000000, LENGTH = 128M
    USB_DMA (rw)  : ORIGIN = 0x58000000, LENGTH = 1M
}

SECTIONS
{
    .text : {
        __text_start = .;
        KEEP(*(.vectors))
        *(.text.startup)
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        __text_end = .;
    } > CODE

    /* The existing application's C++ shim runs these once from main. */
    .init_array : {
        . = ALIGN(4);
        __init_array_start = .;
        KEEP(*(.init_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        __init_array_end = .;
    } > CODE
    .fini_array : {
        . = ALIGN(4);
        __fini_array_start = .;
        KEEP(*(.fini_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        __fini_array_end = .;
    } > CODE
    .data : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
        __image_end = .;
    } > CODE
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > BSS_HIGH
    .bss_low (NOLOAD) : {
        . = ALIGN(4);
        __bss_low_start = .;
        *(.bss_low .bss_low.*)
        . = ALIGN(4);
        __bss_low_end = .;
    } > BSS_LOW
    /* USB host descriptors and DMA buffers need a consistent CPU/device
     * view. mmu.c maps this section as shareable Normal non-cacheable. */
    .bss_usb (NOLOAD) : {
        . = ALIGN(64);
        __bss_usb_start = .;
        *(.bss_usb .bss_usb.*)
        . = ALIGN(64);
        __bss_usb_end = .;
    } > USB_DMA

    __stack_top          = ORIGIN(CODE) + LENGTH(CODE);
    __svc_stack_bottom   = __stack_top - 64K;
    __irq_stack_top      = __svc_stack_bottom;
    __irq_stack_bottom   = __irq_stack_top - 16K;
    __abort_stack_top    = __irq_stack_bottom;
    __abort_stack_bottom = __abort_stack_top - 8K;
    __undef_stack_top    = __abort_stack_bottom;
    __undef_stack_bottom = __undef_stack_top - 8K;
    __fiq_stack_top      = __undef_stack_bottom;
    __stack_bottom       = __fiq_stack_top - 4K;

    __vram_start = ORIGIN(VRAM);
    __vram_end   = ORIGIN(VRAM) + LENGTH(VRAM);
    __fb0_start  = ORIGIN(VRAM);
    __fb1_start  = ORIGIN(VRAM) + 0x00100000;
    __exidx_start = 0;
    __exidx_end = 0;

    ASSERT(_start == 0x41000000, "PINE64 entry must stay at 0x41000000")
    ASSERT(__image_end <= __stack_bottom, "PINE64 code/data overlaps exception stacks")
    ASSERT(__bss_end <= ORIGIN(BSS_HIGH) + LENGTH(BSS_HIGH), "PINE64 high BSS exceeds reserved RAM")
    ASSERT(__bss_low_end <= ORIGIN(CODE), "PINE64 low BSS overlaps code")
    ASSERT(__bss_usb_end <= ORIGIN(USB_DMA) + LENGTH(USB_DMA), "PINE64 USB DMA arena overflow")
    ASSERT((__stack_top & 7) == 0, "PINE64 AAPCS stack alignment")

    /DISCARD/ : {
        *(.comment)
        *(.ARM.attributes)
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
        *(.note.GNU-stack)
    }
}
