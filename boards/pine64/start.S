/*
 * PINE A64+ bare-metal AArch32 startup, reconstructed from the SD2 payload
 * which passed the board's CPU/BSS/NEON/cache/SD/HDMI bring-up checks.
 * U-Boot must use bootm with CONFIG_ARMV8_SWITCH_TO_EL1=y. Entry is ARM
 * state, nonsecure SVC/EL1, with MMU and D-cache disabled. This is not an
 * AArch64 `go` entry point. TF-A has already prepared A53 cache coherency.
 */
    .syntax unified
    .arch armv8-a
    .fpu neon-fp-armv8
    .arm

    .section .vectors, "ax"
    .balign 32
    .global _start
_start:
    b _reset
    b .Lundefined
    b .Lsvc
    b .Lprefetch_abort
    b .Ldata_abort
    b .Lreserved
    b _irq_handler
    b .Lfiq

    .section .text.startup, "ax"
    .global _reset
    .type _reset, %function
_reset:
    mov r8, r2                       /* Linux boot ABI: r2 = DTB */
    mrs r9, cpsr
    cpsid aif
    and r0, r9, #0x1f
    cmp r0, #0x13
    bne .Lbad_mode

    cps #0x12
    ldr sp, =__irq_stack_top
    cps #0x17
    ldr sp, =__abort_stack_top
    cps #0x1b
    ldr sp, =__undef_stack_top
    cps #0x11
    ldr sp, =__fiq_stack_top
    cps #0x13
    ldr sp, =__stack_top

    mrc p15, 0, r0, c1, c0, 0
    tst r0, #5                      /* Reject live M or C; never discard dirty cache. */
    bne .Lbad_cache_state
    /* AArch64 RES1 SCTLR bits have AArch32 meanings (TRE, AFE, UWXN,
     * etc.). Normalize before enabling short-descriptor translation.
     * Clear TE, AFE, TRE, EE, UWXN, WXN, V, I, C, A, M. */
    ldr r1, =0x72183007
    bic r0, r0, r1
    mcr p15, 0, r0, c1, c0, 0
    dsb sy
    isb
    ldr r0, =_start
    mcr p15, 0, r0, c12, c0, 0      /* VBAR */
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0      /* I-cache invalidate all */
    mcr p15, 0, r0, c7, c5, 6      /* Branch predictor invalidate */
    mcr p15, 0, r0, c8, c7, 0      /* Unified TLB invalidate */
    dsb sy
    isb
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #(1 << 12)
    mcr p15, 0, r0, c1, c0, 0
    mov r10, r0
    isb

    mrc p15, 0, r0, c1, c0, 2      /* CPACR: all 32 D registers and NEON */
    orr r0, r0, #(15 << 20)
    bic r0, r0, #(1 << 30)
    bic r0, r0, #(1 << 31)
    mcr p15, 0, r0, c1, c0, 2
    isb
    mov r0, #(1 << 30)
    vmsr fpexc, r0
    mov r0, #0
    vmsr fpscr, r0

    ldr r0, =__bss_start
    ldr r1, =__bss_end
    bl .Lzero_words
    ldr r0, =__bss_low_start
    ldr r1, =__bss_low_end
    bl .Lzero_words
    ldr r0, =__bss_usb_start
    ldr r1, =__bss_usb_end
    bl .Lzero_words
    /* Own all 8 MiB of VRAM, including buffers not touched by the menu. */
    ldr r0, =__vram_start
    ldr r1, =__vram_end
    bl .Lzero_words
    ldr r0, =pine64_boot_fdt
    str r8, [r0]
    ldr r0, =pine64_boot_cpsr
    str r9, [r0]
    ldr r0, =pine64_boot_sctlr
    str r10, [r0]
    bl pine64_boot_report
    bl main
    ldr r0, =.Lmain_returned_msg
    bl .Learly_puts
.Lhalt:
    cpsid aif
    wfi
    b .Lhalt

.Lzero_words:
    mov r2, #0
1:  cmp r0, r1
    bxhs lr
    str r2, [r0], #4
    b 1b

    .global pine64_boot_report
    .type pine64_boot_report, %function
pine64_boot_report:
    push {r4, lr}
    ldr r0, =.Lboot_msg
    bl .Learly_puts
    ldr r0, =pine64_boot_cpsr
    ldr r0, [r0]
    bl .Learly_hex
    ldr r0, =.Lsctlr_msg
    bl .Learly_puts
    ldr r0, =pine64_boot_sctlr
    ldr r0, [r0]
    bl .Learly_hex
    ldr r0, =.Lfdt_msg
    bl .Learly_puts
    ldr r0, =pine64_boot_fdt
    ldr r0, [r0]
    bl .Learly_hex
    ldr r0, =.Lnewline_msg
    bl .Learly_puts
    pop {r4, pc}

    .global _irq_handler
    .type _irq_handler, %function
_irq_handler:
    sub lr, lr, #4
    srsdb sp!, #0x12                /* PC + CPSR, 8 bytes */
    push {r0-r12, lr}               /* 56 bytes */
    vpush {d0-d15}
    vpush {d16-d31}                 /* 256 bytes */
    vmrs r0, fpscr
    vmrs r1, fpexc
    push {r0, r1}                   /* 8 bytes; stack remains 8-byte aligned */
#ifdef PINE64_CPU_TEST
    ldr r0, =0x0801000c             /* QEMU virt GICv2, architecture test only */
#else
    ldr r0, =0x01c8200c             /* Nonsecure GICC_IAR (Group 1 alias) */
#endif
    ldr r1, [r0]
    ubfx r2, r1, #0, #10
    mov r3, #1020
    cmp r2, r3
    bhs .Lirq_done                  /* Reserved/spurious IDs must not be EOI'd. */
    push {r1, r2}                   /* Preserve full IAR; total C-call frame 336 B */
    mov r0, r2
    bl irq_handler
    pop {r1, r2}
    dsb sy                          /* Complete peripheral acknowledgement first. */
#ifdef PINE64_CPU_TEST
    ldr r0, =0x08010010
#else
    ldr r0, =0x01c82010
#endif
    str r1, [r0]
    dsb sy
.Lirq_done:
    pop {r0, r1}
    vpop {d16-d31}
    vpop {d0-d15}
    vmsr fpscr, r0
    vmsr fpexc, r1
    pop {r0-r12, lr}
    rfeia sp!

/* Fault reporting uses leaf assembly UART routines: no libc, VFP, or
 * allocator dependency, so failed initialization still gives a useful log.
 * Print exception LR and saved CPSR without guessing ARM/Thumb PC offsets.
 */
.Lundefined:
    ldr r0, =.Lundefined_msg
    b .Lsimple_fault
.Lsvc:
    ldr r0, =.Lsvc_msg
    b .Lsimple_fault
.Lreserved:
    ldr r0, =.Lreserved_msg
    b .Lsimple_fault
.Lfiq:
    ldr r0, =.Lfiq_msg
.Lsimple_fault:
    mov r4, lr
    mrs r5, spsr
    mov r8, #0
    b .Lreport_fault
.Lprefetch_abort:
    ldr r0, =.Lprefetch_msg
    mov r4, lr
    mrs r5, spsr
    mrc p15, 0, r6, c6, c0, 2      /* IFAR */
    mrc p15, 0, r7, c5, c0, 1      /* IFSR */
    mov r8, #1
    b .Lreport_fault
.Ldata_abort:
    ldr r0, =.Ldata_msg
    mov r4, lr
    mrs r5, spsr
    mrc p15, 0, r6, c6, c0, 0      /* DFAR */
    mrc p15, 0, r7, c5, c0, 0      /* DFSR */
    mov r8, #1
    b .Lreport_fault
.Lbad_mode:
    ldr r0, =.Lbad_mode_msg
    adr r4, _reset
    mov r5, r9
    mov r8, #0
    b .Lreport_fault
.Lbad_cache_state:
    mov r7, r0
    mov r6, #0
    ldr r0, =.Lbad_cache_msg
    adr r4, _reset
    mov r5, r9
    mov r8, #1
.Lreport_fault:
    cpsid aif
    bl .Learly_puts
    ldr r0, =.Llr_msg
    bl .Learly_puts
    mov r0, r4
    bl .Learly_hex
    ldr r0, =.Lcpsr_msg
    bl .Learly_puts
    mov r0, r5
    bl .Learly_hex
    cmp r8, #0
    beq 1f
    ldr r0, =.Lfar_msg
    bl .Learly_puts
    mov r0, r6
    bl .Learly_hex
    ldr r0, =.Lfsr_msg
    bl .Learly_puts
    mov r0, r7
    bl .Learly_hex
1:  ldr r0, =.Lhalt_msg
    bl .Learly_puts
    b .Lhalt

.Learly_puts:
#ifdef PINE64_CPU_TEST
    ldr r2, =0x09000000             /* QEMU virt's PL011; test build only */
#else
    ldr r2, =0x01c28000             /* U-Boot initialized UART0 at 115200 8N1 */
#endif
1:  ldrb r1, [r0], #1
    cmp r1, #0
    bxeq lr
#ifdef PINE64_CPU_TEST
2:  ldr r3, [r2, #0x18]
    tst r3, #0x20
    bne 2b
#else
2:  ldr r3, [r2, #0x14]
    tst r3, #0x20
    beq 2b
#endif
    str r1, [r2]
    b 1b
.Learly_hex:
    mov r2, #8
#ifdef PINE64_CPU_TEST
    ldr r3, =0x09000000
#else
    ldr r3, =0x01c28000
#endif
1:  ror r0, r0, #28
    and r1, r0, #15
    cmp r1, #9
    addls r1, r1, #'0'
    addhi r1, r1, #('a' - 10)
#ifdef PINE64_CPU_TEST
2:  ldr r12, [r3, #0x18]
    tst r12, #0x20
    bne 2b
#else
2:  ldr r12, [r3, #0x14]
    tst r12, #0x20
    beq 2b
#endif
    str r1, [r3]
    subs r2, r2, #1
    bne 1b
    bx lr
    .ltorg

    .section .bss.boot, "aw", %nobits
    .balign 4
    .global pine64_boot_fdt, pine64_boot_cpsr, pine64_boot_sctlr
pine64_boot_fdt:   .space 4
pine64_boot_cpsr:  .space 4
pine64_boot_sctlr: .space 4

    .section .rodata.boot, "a"
.Lboot_msg:          .asciz "\n[PINE64] AArch32 SVC/EL1 CPU0; entry CPSR=0x"
.Lsctlr_msg:         .asciz " SCTLR=0x"
.Lfdt_msg:           .asciz " DTB=0x"
.Lnewline_msg:       .asciz "\n"
.Lmain_returned_msg: .asciz "[PINE64] main returned; halting\n"
.Lundefined_msg:     .asciz "\n[PINE64] undefined instruction"
.Lsvc_msg:           .asciz "\n[PINE64] unexpected SVC"
.Lreserved_msg:      .asciz "\n[PINE64] reserved exception"
.Lfiq_msg:           .asciz "\n[PINE64] unexpected FIQ"
.Lprefetch_msg:      .asciz "\n[PINE64] prefetch abort"
.Ldata_msg:          .asciz "\n[PINE64] data abort"
.Lbad_mode_msg:      .asciz "\n[PINE64] expected AArch32 SVC/EL1 entry"
.Lbad_cache_msg:     .asciz "\n[PINE64] entry requires MMU and D-cache off"
.Llr_msg:            .asciz " exception LR=0x"
.Lcpsr_msg:          .asciz " saved CPSR=0x"
.Lfar_msg:           .asciz " FAR=0x"
.Lfsr_msg:           .asciz " FSR=0x"
.Lhalt_msg:          .asciz "\nHALT\n"
    .section .note.GNU-stack, "", %progbits
