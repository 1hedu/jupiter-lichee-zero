/* Architecture fixture only: start in AArch64 EL2 as QEMU's loader,
 * poison the actual runtime BSS/VRAM and emulate the U-Boot EL1 handoff.
 * This does not emulate Allwinner firmware, DDR training, or peripherals.
 */
    .section .text, "ax"
    .global _fixture_start
_fixture_start:
    msr daifset, #15
    mrs x0, CurrentEL
    cmp x0, #8
    b.ne .Lhalt

    ldr x0, =BSS_START
    ldr x1, =BSS_END
    bl .Lpoison
    ldr x0, =0x40200000
    ldr x1, =0x41000000
    bl .Lpoison
    ldr x0, =0x43800000
    ldr x1, =0x44000000
    bl .Lpoison
    ldr x0, =USB_BSS_START
    ldr x1, =USB_BSS_END
    bl .Lpoison

    msr hcr_el2, xzr               /* AArch32 EL1; no traps or stage 2. */
    msr cptr_el2, xzr              /* FP/NEON accessible at EL1. */
    mov x0, #3
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr
    mov x0, #6
    msr mdcr_el2, x0               /* 6 A53 PMU event counters, no PMU trap. */
    /* U-Boot writes an AArch64 SCTLR value before AArch32 entry. Its
     * RES1 bits include AArch32 AFE/TRE/UWXN: startup must normalize it. */
    ldr x0, =0x30d00800
    msr sctlr_el1, x0
    ldr x0, =0x41000000
    msr elr_el2, x0
    mov x0, #0x1d3
    msr spsr_el2, x0
    mov x0, #0
    mov x1, #0
    ldr x2, =0x47000000
    dsb sy
    isb
    eret

.Lpoison:
    mov w3, #0xa5a5
    movk w3, #0xa5a5, lsl #16
1:  cmp x0, x1
    b.hs 2f
    str w3, [x0], #4
    b 1b
2:  ret
.Lhalt:
    wfi
    b .Lhalt
