/*
 * Sega Jupiter — Startup Assembly
 * Target: ARM Cortex-A7 (ARMv7-A), Allwinner V3s
 *
 * Runs immediately after U-Boot SPL handover.
 * DRAM is live. We init CPU state, clear BSS, jump to main().
 * IRQ support: separate IRQ stack, GIC-based dispatch to C handler.
 */

    .syntax unified
    .arch armv7-a
    .arm

/* ---- Exception Vector Table (must be first, at 0x41000000) ---- */
    .section .vectors, "ax"
    .global _start

_start:
    b       _reset              /* Reset                */
    b       _undef_handler      /* Undefined Instruction */
    b       _hang               /* SVC                   */
    b       _pabort_handler     /* Prefetch Abort        */
    b       _dabort_handler     /* Data Abort            */
    b       _hang               /* Reserved              */
    b       _irq_handler        /* IRQ                   */
    b       _hang               /* FIQ                   */

/* ---- Undefined-Instruction handler ----
 * On UND the CPU sets LR_und = (address_of_bad_insn) + 4 (ARM) or + 2 (Thumb).
 * We log the bad PC, then return to LR_und — which skips PAST the offending
 * instruction and continues. Needed because our bare-metal Stratagus port
 * has C++ ctors that sometimes emit instructions libstdc++ expects to handle
 * via unwind tables we don't support; without this handler those ctors just
 * wedge the CPU forever. Skipping lets the rest of the ctor body run. */
    .global _undef_handler
    .type _undef_handler, %function
_undef_handler:
    /* Save r0-r3, ip, lr_und to the stack (we clobber them for the log). */
    stmfd   sp!, {r0-r4, lr}
    sub     r4, lr, #4          /* r4 = faulting PC (ARM mode default) */
    ldr     r0, =_undef_msg
    bl      uart_puts
    mov     r0, r4
    bl      uart_puthex
    ldr     r0, =_undef_nl
    bl      uart_puts
    ldmfd   sp!, {r0-r4, lr}
    movs    pc, lr              /* Return from exception, skip past bad insn */

    .section .rodata
_undef_msg: .asciz "[UND@"
_undef_nl:  .asciz "]\n"
_dabort_msg: .asciz "\n[DABORT pc=0x"
_dabort_far: .asciz " far=0x"
_dabort_dfsr: .asciz " dfsr=0x"
_dabort_nl:  .asciz "]\nHALT\n"
_pabort_msg: .asciz "\n[PABORT pc=0x"
_pabort_nl:  .asciz "]\nHALT\n"
    .section .vectors, "ax"

/* ---- Data Abort handler ----
 * On a load/store fault, print the faulting PC, the Fault Address Register
 * (FAR), and the Data Fault Status Register (DFSR), then halt. Without this
 * we just spun in _hang and the user got no information about what went
 * wrong. Now we get actionable trace at the moment of the bad access. */
    .global _dabort_handler
    .type _dabort_handler, %function
_dabort_handler:
    /* LR_abt = address_of_faulting_insn + 8 in ARM, +4 in Thumb. We don't
     * round-trip; we halt — so just compute LR-8 for ARM. */
    sub     r4, lr, #8          /* r4 = faulting PC */
    mrc     p15, 0, r5, c6, c0, 0   /* r5 = DFAR (faulting address)  */
    mrc     p15, 0, r6, c5, c0, 0   /* r6 = DFSR */
    ldr     r0, =_dabort_msg
    bl      uart_puts
    mov     r0, r4
    bl      uart_puthex
    ldr     r0, =_dabort_far
    bl      uart_puts
    mov     r0, r5
    bl      uart_puthex
    ldr     r0, =_dabort_dfsr
    bl      uart_puts
    mov     r0, r6
    bl      uart_puthex
    ldr     r0, =_dabort_nl
    bl      uart_puts
    /* If the fault was inside dlmalloc, also dump the last allocator-
     * entry caller LR + op + size + ptr — that names the malloc/free
     * that pushed the bad pointer in. */
    bl      jupiter_dlmalloc_dump_last_caller
1:  b       1b

    .global _pabort_handler
    .type _pabort_handler, %function
_pabort_handler:
    sub     r4, lr, #4          /* faulting PC */
    ldr     r0, =_pabort_msg
    bl      uart_puts
    mov     r0, r4
    bl      uart_puthex
    ldr     r0, =_pabort_nl
    bl      uart_puts
1:  b       1b


/* ---- Entry Point ---- */
    .section .text.startup, "ax"
    .global _reset
    .type _reset, %function

_reset:
    /* Disable IRQ and FIQ, enter SVC mode */
    cpsid   if, #0x13

    /* Set vector base address to our table at 0x41000000 */
    ldr     r0, =_start
    mcr     p15, 0, r0, c12, c0, 0     /* Write VBAR */

    /* Invalidate caches and TLB */
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0      /* I-cache      */
    mcr     p15, 0, r0, c7, c5, 6      /* Branch pred   */
    mcr     p15, 0, r0, c8, c7, 0      /* TLB           */
    dsb
    isb

    /* Disable MMU and D-cache, but ENABLE I-cache */
    mrc     p15, 0, r0, c1, c0, 0      /* Read SCTLR    */
    bic     r0, r0, #(1 << 0)          /* M: MMU off    */
    bic     r0, r0, #(1 << 1)          /* A: alignment faults off */
    bic     r0, r0, #(1 << 2)          /* C: D-cache off */
    orr     r0, r0, #(1 << 12)         /* I: I-cache ON  */
    mcr     p15, 0, r0, c1, c0, 0      /* Write SCTLR   */
    dsb
    isb

    /* Enable NEON/VFP fully.
     * Bits 20-23: CP10+CP11 full access
     * Bit 30 (D32DIS): CLEAR — enable d16-d31 (32 FP regs, not just 16)
     * Bit 31 (ASEDIS): CLEAR — enable Advanced SIMD (NEON)
     * If D32DIS or ASEDIS default to 1 on this SoC, code using d16-d31 fails
     * silently — manifests as corrupted output during heavy NEON use. */
    mrc     p15, 0, r0, c1, c0, 2      /* Read CPACR */
    orr     r0, r0, #(0xF << 20)       /* CP10+CP11 full access */
    bic     r0, r0, #(1 << 30)         /* D32DIS=0 */
    bic     r0, r0, #(1 << 31)         /* ASEDIS=0 */
    mcr     p15, 0, r0, c1, c0, 2
    isb
    mov     r0, #(1 << 30)             /* FPEXC.EN */
    vmsr    fpexc, r0

    /* Set up IRQ mode stack (8KB below SVC stack) */
    cps     #0x12                       /* Switch to IRQ mode */
    ldr     sp, =__irq_stack_top
    /* Set up ABT (data/prefetch abort) and UND (undef) mode stacks too —
     * the abort/undef handlers in this file use BL/STMFD, which need a
     * valid SP for the mode they run in. Without this, a data abort
     * jumps to _dabort_handler in ABT mode with sp=0/garbage, the first
     * call to uart_puts pushes through the bad SP, which itself faults
     * silently and re-enters ABT — endless silent recursion that looks
     * exactly like a hang. Reserve 4KB above the IRQ stack for each. */
    cps     #0x17                       /* ABT mode */
    ldr     sp, =__irq_stack_top
    sub     sp, sp, #(8 * 1024)         /* below IRQ stack */
    cps     #0x1B                       /* UND mode */
    ldr     sp, =__irq_stack_top
    sub     sp, sp, #(12 * 1024)        /* below ABT stack */
    cps     #0x13                       /* Back to SVC mode */

    /* Set SVC stack pointer (from linker script) */
    ldr     sp, =__stack_top

    /* Clear BSS (CODE-region zero-init data) */
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
.Lclear_bss:
    cmp     r0, r1
    bge     .Lbss_done
    str     r2, [r0], #4
    b       .Lclear_bss
.Lbss_done:

    /* Clear BSS_LOW (reclaimed 14 MB at 0x40200000-0x41000000 used as
     * overflow pool — dlmalloc's s_dl_low arena lives here, plus any
     * static array tagged __attribute__((section(".bss_low"))). Without
     * this loop, those start with raw DRAM contents from U-Boot, and
     * any C calloc/static-zero-init guarantee silently breaks for them.
     * Symptom: in large binaries where the 1 MB main arena fills before
     * a big calloc lands (e.g. NES_DMC's 256 KB tnd_table when launched
     * from the menu after C++ global ctors have nibbled the main arena),
     * the allocation spills to s_dl_low and returns GARBAGE instead of
     * zero — NES audio plays the wrong mix-LUT values and sounds rate-
     * mangled. Standalone single-demo binaries don't hit this because
     * their main arena never fills. */
    ldr     r0, =__bss_low_start
    ldr     r1, =__bss_low_end
    mov     r2, #0
.Lclear_bss_low:
    cmp     r0, r1
    bge     .Lbss_low_done
    str     r2, [r0], #4
    b       .Lclear_bss_low
.Lbss_low_done:

    /* Jump to C */
    bl      main

    /* If main returns, halt */
_hang:
    wfi
    b       _hang

/* ---- IRQ Handler ----
 * Entered in IRQ mode with SPSR = interrupted mode, LR = return addr + 4.
 * Save caller-saved regs, read GIC IAR, dispatch to C, write EOIR, return.
 */
    .global _irq_handler
    .type _irq_handler, %function
_irq_handler:
    sub     lr, lr, #4              /* Adjust return address */
    srsdb   sp!, #0x12              /* Save LR_irq + SPSR_irq to IRQ stack */
    push    {r0-r3, r12}           /* Save caller-saved regs */
    /* Save full caller-saved NEON/VFP context. AAPCS says d0-d7 and d16-d31
     * are caller-saved. On ARMv7-A w/ 32-reg VFP+NEON enabled (our CPACR),
     * compiler-emitted NEON in ISR path (memcpy auto-vec, math_neon calls)
     * WILL use d16-d31 and corrupt whoever was interrupted. Must save both. */
    vpush   {d0-d15}
    vpush   {d16-d31}
    vmrs    r0, fpscr
    push    {r0}

    /* Read interrupt ID from GIC */
    ldr     r0, =0x01C8200C        /* GICC_IAR */
    ldr     r1, [r0]
    ubfx    r2, r1, #0, #10        /* Extract interrupt ID (bits 9:0) */

    /* Spurious? (ID 1023) */
    ldr     r3, =1023
    cmp     r2, r3
    beq     .Lirq_done

    /* Call C handler: irq_handler(uint32_t irq_id) */
    mov     r0, r2
    push    {r1}                    /* Save raw IAR for EOIR */
    bl      irq_handler
    pop     {r1}

    /* End of interrupt */
    ldr     r0, =0x01C82010        /* GICC_EOIR */
    str     r1, [r0]               /* Write back the IAR value */

.Lirq_done:
    pop     {r0}
    vmsr    fpscr, r0
    vpop    {d16-d31}
    vpop    {d0-d15}
    pop     {r0-r3, r12}
    rfefd   sp!                     /* Restore CPSR + PC from stack */
