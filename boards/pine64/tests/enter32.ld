ENTRY(_fixture_start)
SECTIONS {
    /* QEMU places its generated DTB at 0x40000000..0x40100000. */
    . = 0x40110000;
    .text : { *(.text) }
    /DISCARD/ : { *(.note*) *(.comment*) }
}
