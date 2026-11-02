/* Deliberately overwrite every VFP register inside a real interrupt.
 * The architectural exception wrapper, unlike an ordinary function
 * caller, must preserve all of them plus FPSCR and the interrupted CPSR.
 */
    .syntax unified
    .arch armv8-a
    .fpu neon-fp-armv8
    .thumb
    .section .text.cpu_irq_callback, "ax"
    .global cpu_irq_callback
    .type cpu_irq_callback, %function
    .thumb_func
cpu_irq_callback:
    mov r0, sp
    ands r0, r0, #7
    ldr r1, =cpu_irq_seen
    ite eq
    moveq r0, #1
    movne r0, #2
    str r0, [r1]
    vmov.i8 q0, #0x55
    vmov.i8 q1, #0x55
    vmov.i8 q2, #0x55
    vmov.i8 q3, #0x55
    vmov.i8 q4, #0x55
    vmov.i8 q5, #0x55
    vmov.i8 q6, #0x55
    vmov.i8 q7, #0x55
    vmov.i8 q8, #0x55
    vmov.i8 q9, #0x55
    vmov.i8 q10, #0x55
    vmov.i8 q11, #0x55
    vmov.i8 q12, #0x55
    vmov.i8 q13, #0x55
    vmov.i8 q14, #0x55
    vmov.i8 q15, #0x55
    mov r0, #(1 << 24)
    vmsr fpscr, r0
    bx lr

    .section .text.cpu_irq_probe, "ax"
    .global cpu_irq_probe
    .type cpu_irq_probe, %function
    .thumb_func
cpu_irq_probe:
    push {r4-r7, lr}
    sub sp, sp, #4
    vpush {d8-d15}
    sub sp, sp, #256
    vmov.i8 q0, #0xa0
    vmov.i8 q1, #0xa1
    vmov.i8 q2, #0xa2
    vmov.i8 q3, #0xa3
    vmov.i8 q4, #0xa4
    vmov.i8 q5, #0xa5
    vmov.i8 q6, #0xa6
    vmov.i8 q7, #0xa7
    vmov.i8 q8, #0xa8
    vmov.i8 q9, #0xa9
    vmov.i8 q10, #0xaa
    vmov.i8 q11, #0xab
    vmov.i8 q12, #0xac
    vmov.i8 q13, #0xad
    vmov.i8 q14, #0xae
    vmov.i8 q15, #0xaf
    mov r0, #0
    vmsr fpscr, r0
    ldr r4, =cpu_irq_seen
    str r0, [r4]
    ldr r0, =0x08000f00          /* QEMU GICD_SGIR: SGI1 to current CPU. */
    ldr r1, =0x02000001
    dsb sy
    str r1, [r0]
    dsb sy
    ldr r5, =1000000
1:  ldr r0, [r4]
    cmp r0, #0
    bne 2f
    subs r5, r5, #1
    bne 1b
    mov r0, #1
    b .Ldone
2:  cmp r0, #1
    bne .Lbad
    vmrs r0, fpscr
    cmp r0, #0
    bne .Lbad
    mov r1, sp
    vstmia r1!, {d0-d15}
    vstmia r1!, {d16-d31}
    mov r1, sp
    mov r2, #0xa0
3:  mov r3, #16
4:  ldrb r0, [r1], #1
    cmp r0, r2
    bne .Lbad
    subs r3, r3, #1
    bne 4b
    adds r2, r2, #1
    cmp r2, #0xb0
    bne 3b
    mov r0, #0
    b .Ldone
.Lbad:
    mov r0, #2
.Ldone:
    add sp, sp, #256
    vpop {d8-d15}
    add sp, sp, #4
    pop {r4-r7, pc}
    .section .note.GNU-stack, "", %progbits
