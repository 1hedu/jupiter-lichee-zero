/*
 * Sega Jupiter — Linker Script
 * Target: Allwinner V3s (Lichee Pi Zero)
 *
 * Memory Layout (from SEGA_JUPITER_HARDWARE_REFERENCE.md §2,§3):
 *   DRAM:         0x40000000, 64MB
 *   SPL handover: 0x41000000
 *   Framebuffer:  0x42000000 (double-buffered 480x272 ARGB8888)
 *
 * Source: V3s Datasheet, U-Boot SPL config
 */

ENTRY(_start)

MEMORY
{
    /* Low BSS arena: 0x40200000..0x41000000 (14MB). U-Boot SPL uses
     * <2MB at the very bottom of DDR during boot; above 0x40200000 is
     * free post-handoff. Reclaimed as zero-init bss pool so it doesn't
     * compete with CODE's bss budget. Main binary still loads at
     * 0x41000000 — only zero-init state lives here. */
    BSS_LOW (rw)  : ORIGIN = 0x40200000, LENGTH = 14M
    CODE    (rwx) : ORIGIN = 0x41000000, LENGTH = 40M
    VRAM    (rw)  : ORIGIN = 0x43800000, LENGTH = 8M
}

SECTIONS
{
    .text : {
        __text_start = .;
        *(.vectors)
        *(.text.startup)
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        __text_end = .;
    } > CODE

    /* C++ global constructors (run once from main() before first C++ call) */
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
        __fini_array_end = .;
    } > CODE

    .data : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > CODE

    /* Main bss in CODE region (post .data). */
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > CODE

    /* Overflow bss: anything tagged .bss_low lives in the reclaimed
     * 8MB at 0x40800000. Tag large zero-init arrays with
     * __attribute__((section(".bss_low"))) to move them here. */
    .bss_low (NOLOAD) : {
        __bss_low_start = .;
        *(.bss_low .bss_low.*)
        . = ALIGN(4);
        __bss_low_end = .;
    } > BSS_LOW

    /* Stacks grow downward from these tops toward BSS end.
     * SVC: 64KB for deep C++/NEON chains (munt, nuked-sc55).
     * IRQ: lives below SVC region; has implicit room down to BSS end. */
    __irq_stack_top = ORIGIN(CODE) + LENGTH(CODE) - 64K;
    __stack_top     = ORIGIN(CODE) + LENGTH(CODE);

    __fb0_start = ORIGIN(VRAM);
    __fb1_start = ORIGIN(VRAM) + 0x00200000;  /* FB1 at VRAM+2MB */

    /* Satisfy __exidx references from libgcc unwinder (pulled in by
       .fnstart/.fnend in the ARM optimized-routines .S files).
       The actual sections are discarded below so these point nowhere,
       but the symbols must exist to link. */
    __exidx_start = 0;
    __exidx_end   = 0;

    /DISCARD/ : {
        *(.comment)
        *(.ARM.attributes)
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
    }
}
