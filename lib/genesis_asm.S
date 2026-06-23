/*
 * Jupiter SDK — Genesis FM Engine (ARM Assembly)
 * Full operator eval, 8 algorithms, envelope, PSG, noise.
 */
    .syntax unified
    .arch   armv7-a
    .arm

/* fm_op_t offsets (20 bytes) */
.equ OP_PH,   0
.equ OP_PI,   4
.equ OP_MUL,  8
.equ OP_DET,  9
.equ OP_TL,  10
.equ OP_AR,  11
.equ OP_DR,  12
.equ OP_SR,  13
.equ OP_RR,  14
.equ OP_SL,  15
.equ OP_ENV, 16
.equ OP_EST, 18
.equ OPSZ,   20

/* fm_ch_t offsets (100 bytes) */
.equ CH_ALG,  84
.equ CH_FB,   85
.equ CH_FO0,  88
.equ CH_FO1,  92
.equ CH_ACT,  96
.equ CHSZ,   100

/* ============================================================
 * Macro: EVALOP — one FM operator
 * \oR = op pointer, \mR = modulation, \dR = output dest
 * Uses/clobbers: r10, r11, r12.  r8=sine_tab, r9=env_tab
 * ============================================================ */
.macro EVALOP oR, mR, dR
    ldrb    r12, [\oR, #OP_EST]
    cmp     r12, #0                     /* ENV_OFF? */
    moveq   \dR, #0
    beq     .Lop_end\@

    /* Sine lookup */
    ldr     r10, [\oR, #OP_PH]
    add     r10, r10, \mR, lsl #5
    lsr     r10, r10, #22               /* 10-bit phase */

    and     r11, r10, #0xFF             /* pos */
    tst     r10, #0x100
    rsbne   r11, r11, #255
    lsl     r11, r11, #1                /* byte offset for int16_t */
    ldrsh   \dR, [r8, r11]              /* sine value */
    tst     r10, #0x200
    rsbne   \dR, \dR, #0

    /* Envelope attenuation */
    ldrh    r10, [\oR, #OP_ENV]
    rsb     r11, r10, #1024
    sub     r11, r11, #1                /* 1023 - env */
    mul     \dR, \dR, r11
    asr     \dR, \dR, #10

    /* Total level */
    ldrb    r11, [\oR, #OP_TL]
    rsb     r11, r11, #127
    mul     \dR, \dR, r11
    asr     \dR, \dR, #7

    /* Phase advance */
    ldr     r11, [\oR, #OP_PH]
    ldr     r12, [\oR, #OP_PI]
    ldrb    r10, [\oR, #OP_MUL]
    cmp     r10, #0
    mulne   r12, r12, r10
    lsreq   r12, r12, #1
    ldrsb   r10, [\oR, #OP_DET]
    add     r11, r11, r12
    add     r11, r11, r10
    str     r11, [\oR, #OP_PH]

    /* Envelope tick (only if scheduled this sample by prescaler).
     * Slot sp+#32 holds 0/1 flag set once per sample at .Lsloop top. */
    ldrb    r12, [sp, #32]
    cmp     r12, #0
    beq     .Lop_end\@                  /* skip env tick this sample */

    ldrb    r12, [\oR, #OP_EST]
    ldrh    r10, [\oR, #OP_ENV]

    cmp     r12, #1                     /* ATK */
    bne     .Letd\@
    ldrb    r11, [\oR, #OP_AR]
    and     r11, r11, #31
    lsl     r11, r11, #1
    ldrh    r11, [r9, r11]              /* r11 = base rate */
    /* Exponential attack: dec = rate + (rate * env) >> 4
     * Matches real YM2612 (fast at top, slows near full volume).
     * mul clobbers r12 (state), but we know state==1 here, so just re-set it. */
    mul     r12, r11, r10               /* r12 = rate * env */
    add     r11, r11, r12, lsr #4       /* r11 = rate + (rate*env >> 4) = dec */
    mov     r12, #1                     /* restore state = ATK */
    cmp     r10, r11
    subhi   r10, r10, r11
    movls   r10, #0
    movls   r12, #2                     /* → DEC */
    b       .Letw\@
.Letd\@:
    cmp     r12, #2                     /* DEC */
    bne     .Lets\@
    ldrb    r11, [\oR, #OP_SL]
    and     r11, r11, #15
    lsl     r11, r11, #6               /* sl*64 */
    ldrb    r12, [\oR, #OP_DR]
    and     r12, r12, #31
    lsl     r12, r12, #1
    ldrh    r12, [r9, r12]
    add     r10, r10, r12
    cmp     r10, r11
    movge   r10, r11
    movge   r12, #3
    movlt   r12, #2
    b       .Letw\@
.Lets\@:
    cmp     r12, #3                     /* SUS */
    bne     .Letr\@
    ldrb    r11, [\oR, #OP_SR]
    and     r11, r11, #31
    lsl     r11, r11, #1
    ldrh    r11, [r9, r11]
    add     r10, r10, r11
    movw    r11, #1023
    cmp     r10, r11
    movgt   r10, r11
    mov     r12, #3
    b       .Letw\@
.Letr\@:                                /* REL */
    ldrb    r11, [\oR, #OP_RR]
    and     r11, r11, #15
    lsl     r11, r11, #1
    add     r11, r11, #1
    lsl     r11, r11, #1
    ldrh    r11, [r9, r11]
    add     r10, r10, r11
    mov     r12, #4
    movw    r11, #1023
    cmp     r10, r11
    movgt   r10, r11
    movgt   r12, #0                     /* → OFF */
.Letw\@:
    strh    r10, [\oR, #OP_ENV]
    strb    r12, [\oR, #OP_EST]
.Lop_end\@:
.endm

/* ============================================================
 * audio_genesis_mix_asm(uint32_t num_samples)
 * ============================================================ */
    .global audio_genesis_mix_asm
    .type   audio_genesis_mix_asm, %function
audio_genesis_mix_asm:
    push    {r4-r12, lr}
    sub     sp, sp, #40                 /* frame: 32 used + 8 for env prescaler */
    mov     r7, r0                      /* sample counter */

    ldr     r8, .Lp_sine
    ldr     r9, .Lp_ertab
    ldr     r4, .Lp_fmch
    ldr     r5, .Lp_mbuf
    ldr     r6, .Lp_mwr
    ldr     r6, [r6]
    ldr     r0, .Lp_mvol
    ldrb    r0, [r0]
    str     r0, [sp, #0]               /* master_volume */
    str     r4, [sp, #4]               /* fm_ch base */
    str     r5, [sp, #8]               /* mix_buf base */
    /* Load env-divider address once and stash in slot 36; the literal pool
     * for fm_env_div_count_asm at the function tail is past PC-relative
     * range from the loop body, so cache the pointer here. */
    ldr     r0, .Lp_edivc
    str     r0, [sp, #36]

.Lsloop:
    /* Envelope prescaler: load fm_env_tick_now/fm_env_div_count, set flag,
     * advance counter, store back. Slot sp+#32 caches the flag for EVALOP. */
    ldr     r2, [sp, #36]               /* address of fm_env_div_count_asm */
    ldrb    r3, [r2]                    /* div_count */
    cmp     r3, #0
    moveq   r0, #1
    movne   r0, #0
    strb    r0, [sp, #32]               /* fm_env_tick_now (this sample) */
    add     r3, r3, #1
    cmp     r3, #3
    movge   r3, #0
    strb    r3, [r2]

    mov     r0, #0                      /* sum */
    ldr     r4, [sp, #4]
    mov     r1, #0                      /* ch counter */

.Lchloop:
    ldrb    r2, [r4, #CH_ACT]
    cmp     r2, #0
    beq     .Lnxch

    /* Feedback */
    ldrb    r3, [r4, #CH_FB]
    cmp     r3, #0
    beq     .Lfb0
    ldr     r10, [r4, #CH_FO0]
    ldr     r11, [r4, #CH_FO1]
    add     r10, r10, r11
    rsb     r3, r3, #10
    asr     r3, r10, r3
    b       .Lfbd
.Lfb0:  mov     r3, #0
.Lfbd:
    str     r0, [sp, #12]              /* save sum */
    str     r1, [sp, #16]
    str     r4, [sp, #20]

    ldrb    r2, [r4, #CH_ALG]
    and     r2, r2, #7
    adr     r10, .Latab
    ldr     pc, [r10, r2, lsl #2]

    .ltorg                              /* literal pool */
.Lp_sine:  .word fm_qsine
.Lp_ertab: .word env_rate_tab
.Lp_fmch:  .word fm_ch
.Lp_mbuf:  .word mix_buf
.Lp_mwr:   .word mix_wr
.Lp_mvol:  .word master_volume
.Lp_psg:   .word psg_tone
.Lp_nch:   .word psg_noise_ch
.Lp_edivc: .word fm_env_div_count_asm

.Latab:
    .word .La0, .La1, .La2, .La3, .La4, .La5, .La6, .La7

/* Alg 0: 1->2->3->4 */
.La0:
    EVALOP r4, r3, r0
    mov r3, r0
    add r5, r4, #(OPSZ*1)
    EVALOP r5, r0, r0
    add r5, r4, #(OPSZ*2)
    EVALOP r5, r0, r0
    add r5, r4, #(OPSZ*3)
    EVALOP r5, r0, r0
    b .Ladone

/* Alg 1: (1+2)->3->4 */
.La1:
    EVALOP r4, r3, r0
    mov r3, r0
    mov r1, r0
    add r5, r4, #(OPSZ*1)
    mov r2, #0
    EVALOP r5, r2, r0
    add r0, r0, r1
    add r5, r4, #(OPSZ*2)
    EVALOP r5, r0, r0
    add r5, r4, #(OPSZ*3)
    EVALOP r5, r0, r0
    b .Ladone

/* Alg 2: 1->4, 2->3->4 */
.La2:
    EVALOP r4, r3, r0
    mov r3, r0
    mov r1, r0
    add r5, r4, #(OPSZ*1)
    mov r2, #0
    EVALOP r5, r2, r0
    add r5, r4, #(OPSZ*2)
    EVALOP r5, r0, r0
    add r0, r0, r1
    add r5, r4, #(OPSZ*3)
    EVALOP r5, r0, r0
    b .Ladone

/* Alg 3: 1->2->4, 3->4 */
.La3:
    EVALOP r4, r3, r0
    mov r3, r0
    add r5, r4, #(OPSZ*1)
    EVALOP r5, r0, r0
    mov r1, r0
    add r5, r4, #(OPSZ*2)
    mov r2, #0
    EVALOP r5, r2, r0
    add r0, r0, r1
    add r5, r4, #(OPSZ*3)
    EVALOP r5, r0, r0
    b .Ladone

/* Alg 4: (1->2)+(3->4) */
.La4:
    EVALOP r4, r3, r0
    mov r3, r0
    add r5, r4, #(OPSZ*1)
    EVALOP r5, r0, r0
    mov r1, r0
    add r5, r4, #(OPSZ*2)
    mov r2, #0
    EVALOP r5, r2, r0
    add r5, r4, #(OPSZ*3)
    EVALOP r5, r0, r0
    add r0, r0, r1
    b .Ladone

/* Alg 5: 1->(2+3+4) */
.La5:
    EVALOP r4, r3, r0
    mov r3, r0
    mov r1, r0
    add r5, r4, #(OPSZ*1)
    EVALOP r5, r1, r0
    str r0, [sp, #24]
    add r5, r4, #(OPSZ*2)
    EVALOP r5, r1, r0
    str r0, [sp, #28]
    add r5, r4, #(OPSZ*3)
    EVALOP r5, r1, r0
    ldr r2, [sp, #24]
    ldr r1, [sp, #28]
    add r0, r0, r2
    add r0, r0, r1
    b .Ladone

/* Alg 6: (1->2)+3+4 */
.La6:
    EVALOP r4, r3, r0
    mov r3, r0
    add r5, r4, #(OPSZ*1)
    EVALOP r5, r0, r0
    mov r1, r0
    add r5, r4, #(OPSZ*2)
    mov r2, #0
    EVALOP r5, r2, r0
    str r0, [sp, #24]
    add r5, r4, #(OPSZ*3)
    mov r2, #0
    EVALOP r5, r2, r0
    add r0, r0, r1
    ldr r2, [sp, #24]
    add r0, r0, r2
    b .Ladone

/* Alg 7: 1+2+3+4 */
.La7:
    EVALOP r4, r3, r0
    mov r3, r0
    mov r1, r0
    add r5, r4, #(OPSZ*1)
    mov r2, #0
    EVALOP r5, r2, r0
    str r0, [sp, #24]
    add r5, r4, #(OPSZ*2)
    mov r2, #0
    EVALOP r5, r2, r0
    str r0, [sp, #28]
    add r5, r4, #(OPSZ*3)
    mov r2, #0
    EVALOP r5, r2, r0
    add r0, r0, r1
    ldr r2, [sp, #24]
    ldr r1, [sp, #28]
    add r0, r0, r2
    add r0, r0, r1
    b .Ladone

    .ltorg

.Ladone:
    ldr r4, [sp, #20]
    ldr r10, [r4, #CH_FO0]
    str r10, [r4, #CH_FO1]
    str r3, [r4, #CH_FO0]
    ldr r10, [sp, #12]
    add r0, r0, r10
.Lnxch:
    add r4, r4, #CHSZ
    ldr r1, [sp, #16]
    add r1, r1, #1
    cmp r1, #6
    str r1, [sp, #16]
    blt .Lchloop

    /* PSG tones */
    ldr r4, .Lp_psg2
    mov r1, #0
.Lpsg:
    ldrb r2, [r4, #9]                  /* active */
    cmp r2, #0
    beq .Lpn
    ldrb r2, [r4, #8]                  /* vol */
    cmp r2, #15
    bge .Lpn
    rsb r2, r2, #15
    ldr r10, [r4, #0]                  /* phase */
    mov r11, #546
    mul r3, r2, r11
    tst r10, #0x80000000
    rsbne r3, r3, #0
    add r0, r0, r3
    ldr r11, [r4, #4]
    add r10, r10, r11
    str r10, [r4, #0]
.Lpn:
    add r4, r4, #12
    add r1, r1, #1
    cmp r1, #3
    blt .Lpsg

    /* Noise */
    ldr r4, .Lp_nch2
    ldrb r2, [r4, #20]
    cmp r2, #0
    beq .Lnd
    ldrh r10, [r4, #0]
    ldr r11, [r4, #4]
    ldr r12, [r4, #8]
    add r11, r11, #1
    cmp r11, r12
    blt .Lns
    mov r11, #0
    eor r3, r10, r10, lsr #1
    and r3, r3, #1
    lsr r10, r10, #1
    orr r10, r10, r3, lsl #14
    strh r10, [r4, #0]
.Lns:
    str r11, [r4, #4]
    ldrb r2, [r4, #13]
    and r3, r10, #1
    mov r11, #546
    mul r2, r2, r11
    cmp r3, #0
    rsbne r2, r2, #0
    add r0, r0, r2
.Lnd:

    /* Master volume + clip */
    ldr r2, [sp, #0]
    mul r0, r0, r2
    asr r0, r0, #8
    movw r2, #32767
    cmp r0, r2
    movgt r0, r2
    cmn r0, r2
    mvnlt r0, r2

    /* Store to mix_buf */
    ldr r5, [sp, #8]
    movw r2, #4095
    and r3, r6, r2
    lsl r3, r3, #1
    strh r0, [r5, r3]
    add r6, r6, #1

    subs r7, r7, #1
    bne .Lsloop

    ldr r0, .Lp_mwr2
    str r6, [r0]
    add sp, sp, #40
    pop {r4-r12, pc}

    .ltorg
.Lp_psg2:  .word psg_tone
.Lp_nch2:  .word psg_noise_ch
.Lp_mwr2:  .word mix_wr

    .size audio_genesis_mix_asm, . - audio_genesis_mix_asm

/* Standalone counter for the ASM mixer's envelope prescaler.
 * Independent from the C-side fm_env_div_count to keep ABI simple. */
    .data
    .global fm_env_div_count_asm
    .align 2
fm_env_div_count_asm:
    .byte 0
    .balign 4
    .text
