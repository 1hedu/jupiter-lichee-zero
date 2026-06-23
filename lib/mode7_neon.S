/*
 * Jupiter SDK — mode7_neon.S
 * Hand-scheduled Mode 7 scanline renderer for Cortex-A7.
 *
 * Renders one scanline of an affine-transformed floor (Mode 7 style).
 * Each texel is looked up from a tile map + color LUT, and output
 * pixels are doubled (2 screen pixels per texel) for the SNES-style
 * pixel-doubled aesthetic.
 *
 * Optimization strategy:
 *   - 4-texel unrolled loop with interleaved memory access
 *   - ARM UBFX-style bit extraction for UV → map index
 *   - NEON VDUP+VST for doubled-pixel output (8 pixels per store)
 *   - Computation of texel N+1 overlaps load latency of texel N
 *
 * The table lookups (map + LUT) MUST be scalar — NEON has no
 * gather-load. The win comes from:
 *   1. Interleaving hides load latency on the in-order A7
 *   2. NEON batches 8 pixel-doubled stores into one VST1
 *   3. Tight register allocation avoids all stack spills
 *
 * Target: ARM Cortex-A7 (ARMv7-A) with NEON
 */

    .syntax unified
    .arch   armv7-a
    .fpu    neon-vfpv4
    .arm
    .text


/* ================================================================
 * mode7_scanline — render one Mode 7 scanline with pixel doubling
 * ================================================================
 *
 * void mode7_scanline(uint32_t *row,        // r0  output pixel ptr
 *                     const uint8_t *map,   // r1  tile map base
 *                     const uint32_t *lut,  // r2  color LUT (256 × u32)
 *                     int32_t u,            // r3  starting U (fixed 8.8)
 *                     int32_t v,            // [sp+0]  starting V
 *                     int32_t du,           // [sp+4]  U step per texel
 *                     int32_t dv,           // [sp+8]  V step per texel
 *                     uint32_t count,       // [sp+12] texels to render
 *                     uint32_t map_w_bits,  // [sp+16] log2(map width)
 *                     uint32_t map_mask);   // [sp+20] map width - 1
 *
 * Fixed point: FP_UV = 8 (hardcoded). U and V have 8 fractional bits.
 * Map coordinates: (u >> 8) & map_mask, (v >> 8) & map_mask.
 * Map index: (tv << map_w_bits) | tu.
 *
 * Output: each texel writes 2 adjacent pixels (pixel doubling).
 * Total output pixels = count × 2.
 *
 * Common case: map 64×64, count=240 (M7_W), map_w_bits=6, map_mask=63.
 * ================================================================ */
    .global mode7_scanline
    .type   mode7_scanline, %function
    .align  4

mode7_scanline:
    push    {r4-r11, lr}

    /* Load stack arguments.
     * push saved 9 regs × 4 = 36 bytes, so stack args start at sp+36. */
    ldr     r5,  [sp, #36]         /* v                                 */
    ldr     r6,  [sp, #40]         /* du                                */
    ldr     r7,  [sp, #44]         /* dv                                */
    ldr     r8,  [sp, #48]         /* count (texels)                    */
    ldr     r10, [sp, #52]         /* map_w_bits                        */
    ldr     r9,  [sp, #56]         /* map_mask                          */

    /* Move u from r3 (arg reg) into r4 (callee-saved) */
    mov     r4, r3

    /*
     * Register allocation (stable for entire function):
     *   r0  = dst row pointer (advances by 32 bytes per 4 texels)
     *   r1  = map base (constant)
     *   r2  = LUT base (constant)
     *   r3  = scratch
     *   r4  = u (fixed point, advances by du each texel)
     *   r5  = v (fixed point, advances by dv each texel)
     *   r6  = du (constant)
     *   r7  = dv (constant)
     *   r8  = loop counter (counts down by 4)
     *   r9  = map_mask (constant)
     *   r10 = map_w_bits (constant, for LSL)
     *   r11 = scratch
     *   r12 = scratch (ip)
     *   lr  = scratch (saved on stack)
     *
     * NEON: d0–d3 (q0–q1) used for doubled pixel stores.
     *       All caller-saved — no vpush needed.
     */

    /* Fast path: 4-texel unrolled loop.
     * 4 texels × 2 pixels = 8 output pixels per iteration. */
    subs    r8, r8, #4
    blt     .Lm7_tail_entry

    /* ====== 4-TEXEL MAIN LOOP ======
     *
     * Strategy: compute texel N's index while texel N-1's memory
     * access is in flight. A7 in-order pipeline can't reorder loads,
     * but we can manually interleave work to fill the stall slots.
     *
     * After each color is resolved, VDUP packs it into a NEON
     * d-register (2×32-bit = the doubled pixel pair), freeing the
     * ARM register for the next texel.
     *
     * After 4 texels, one VST1 {q0,q1} writes all 8 pixels (32B).
     */
.Lm7_loop4:
    /* ---- Texel 0: compute map index ---- */
    lsr     r3, r4, #8             /* tu0 = u >> FP_UV                  */
    and     r3, r3, r9             /* tu0 &= map_mask                   */
    lsr     r11, r5, #8            /* tv0 = v >> FP_UV                  */
    and     r11, r11, r9           /* tv0 &= map_mask                   */
    orr     r3, r3, r11, lsl r10   /* idx0 = tu0 | (tv0 << map_w_bits) */
    add     r4, r4, r6             /* u += du                           */
    add     r5, r5, r7             /* v += dv                           */
    ldrb    r3, [r1, r3]           /* tile0 = map[idx0]                 */

    /* ---- Texel 1: overlaps tile0 load latency ---- */
    lsr     r11, r4, #8
    and     r11, r11, r9
    lsr     ip, r5, #8
    and     ip, ip, r9
    orr     r11, r11, ip, lsl r10
    add     r4, r4, r6
    add     r5, r5, r7
    ldr     r3, [r2, r3, lsl #2]   /* color0 = lut[tile0]              */
    ldrb    r11, [r1, r11]         /* tile1 = map[idx1]                */
    vdup.32 d0, r3                 /* d0 = [color0, color0] → NEON
                                      r3 now free for texel 2          */

    /* ---- Texel 2: overlaps tile1 load latency ---- */
    lsr     r3, r4, #8
    and     r3, r3, r9
    lsr     ip, r5, #8
    and     ip, ip, r9
    orr     r3, r3, ip, lsl r10
    add     r4, r4, r6
    add     r5, r5, r7
    ldr     r11, [r2, r11, lsl #2] /* color1 = lut[tile1]              */
    ldrb    r3, [r1, r3]           /* tile2 = map[idx2]                */
    vdup.32 d1, r11                /* d1 = [color1, color1]            */

    /* ---- Texel 3: overlaps tile2 load latency ---- */
    lsr     r11, r4, #8
    and     r11, r11, r9
    lsr     ip, r5, #8
    and     ip, ip, r9
    orr     r11, r11, ip, lsl r10
    add     r4, r4, r6
    add     r5, r5, r7
    ldr     r3, [r2, r3, lsl #2]   /* color2 = lut[tile2]              */
    ldrb    r11, [r1, r11]         /* tile3 = map[idx3]                */
    vdup.32 d2, r3                 /* d2 = [color2, color2]            */

    /* ---- Resolve texel 3 + store all 8 pixels ---- */
    ldr     r11, [r2, r11, lsl #2] /* color3 = lut[tile3]              */
    vdup.32 d3, r11                /* d3 = [color3, color3]            */

    /* q0 = {d0, d1} = {[c0,c0], [c1,c1]} = 4 output pixels
     * q1 = {d2, d3} = {[c2,c2], [c3,c3]} = 4 output pixels
     * VST1 with two q-registers: 32 bytes = 8 pixels in one burst. */
    vst1.32 {q0, q1}, [r0]!

    subs    r8, r8, #4
    bge     .Lm7_loop4


    /* ====== SCALAR TAIL: 1–3 remaining texels ====== */
.Lm7_tail_entry:
    adds    r8, r8, #4
    beq     .Lm7_done

.Lm7_tail1:
    lsr     r3, r4, #8
    and     r3, r3, r9
    lsr     r11, r5, #8
    and     r11, r11, r9
    orr     r3, r3, r11, lsl r10
    add     r4, r4, r6
    add     r5, r5, r7
    ldrb    r3, [r1, r3]
    ldr     r3, [r2, r3, lsl #2]   /* color                            */
    str     r3, [r0], #4           /* pixel A                           */
    str     r3, [r0], #4           /* pixel B (doubled)                 */
    subs    r8, r8, #1
    bgt     .Lm7_tail1

.Lm7_done:
    pop     {r4-r11, pc}
    .size   mode7_scanline, . - mode7_scanline


/* ================================================================
 * iso_scanline — affine tile sampler, 1:1 pixels, pure scalar
 * ================================================================
 *
 * 4-texel unrolled. Interleaved loads hide A7 pipeline stalls.
 * Pure scalar STR stores — NO NEON. ARM→NEON lane inserts (VMOV.32)
 * cost ~20 cycles each on the A7's decoupled pipeline. Four scalar
 * STR cost 4 cycles total. Don't fight the microarchitecture.
 *
 * Hardcoded: FP=12, MAP_MASK=31, MAP_W=32 (MAP_W_BITS=5)
 * LUT indexing: tile_colors[tile * 2 + ((tx ^ ty) & 1)]
 *
 * void iso_scanline(uint32_t *row,        // r0
 *                   const uint8_t *map,   // r1  (32×32)
 *                   const uint32_t *lut,  // r2  (interleaved)
 *                   int32_t u,            // r3  (FP12)
 *                   int32_t v,            // [sp+0]
 *                   int32_t du,           // [sp+4]
 *                   int32_t dv,           // [sp+8]
 *                   uint32_t count);      // [sp+12]
 * ================================================================ */
    .global iso_scanline
    .type   iso_scanline, %function
    .align  4

iso_scanline:
    push    {r4-r11, lr}

    ldr     r5,  [sp, #36]         /* v                                 */
    ldr     r6,  [sp, #40]         /* du                                */
    ldr     r7,  [sp, #44]         /* dv                                */
    ldr     r8,  [sp, #48]         /* count                             */
    mov     r4, r3                 /* u                                 */

    subs    r8, r8, #4
    blt     .Liso_tail_entry

.Liso_loop4:
    /* ---- Texel 0: compute index, start map load ---- */
    asr     r3, r4, #12
    and     r3, r3, #31
    asr     r9, r5, #12
    and     r9, r9, #31
    eor     r10, r3, r9            /* checker bits                      */
    orr     r11, r3, r9, lsl #5   /* map index                         */
    add     r4, r4, r6
    add     r5, r5, r7
    ldrb    r11, [r1, r11]         /* tile0                             */

    /* ---- Texel 1: overlaps tile0 load latency ---- */
    asr     r3, r4, #12
    and     r3, r3, #31
    asr     r9, r5, #12
    and     r9, r9, #31
    and     r10, r10, #1           /* checker0                          */
    add     r11, r10, r11, lsl #1  /* lut_idx0 = tile*2 + check        */
    eor     r10, r3, r9
    orr     ip, r3, r9, lsl #5
    add     r4, r4, r6
    add     r5, r5, r7
    ldr     r11, [r2, r11, lsl #2] /* color0                           */
    ldrb    ip, [r1, ip]           /* tile1                             */

    /* ---- Texel 2: overlaps tile1 load, store color0 ---- */
    asr     r3, r4, #12
    and     r3, r3, #31
    asr     r9, r5, #12
    and     r9, r9, #31
    and     r10, r10, #1
    add     ip, r10, ip, lsl #1
    eor     r10, r3, r9
    orr     lr, r3, r9, lsl #5
    add     r4, r4, r6
    add     r5, r5, r7
    str     r11, [r0], #4          /* STORE color0 pixel A             */
    str     r11, [r0], #4          /* STORE color0 pixel B (doubled)   */
    ldr     ip, [r2, ip, lsl #2]   /* color1                           */
    ldrb    lr, [r1, lr]           /* tile2                             */

    /* ---- Texel 3: overlaps tile2 load, store color1 ---- */
    asr     r3, r4, #12
    and     r3, r3, #31
    asr     r9, r5, #12
    and     r9, r9, #31
    and     r10, r10, #1
    add     lr, r10, lr, lsl #1
    eor     r10, r3, r9
    orr     r11, r3, r9, lsl #5
    add     r4, r4, r6
    add     r5, r5, r7
    str     ip, [r0], #4           /* STORE color1 pixel A             */
    str     ip, [r0], #4           /* STORE color1 pixel B (doubled)   */
    ldr     lr, [r2, lr, lsl #2]   /* color2                           */
    ldrb    r11, [r1, r11]         /* tile3                             */

    /* ---- Resolve texel 3 + store remaining ---- */
    and     r10, r10, #1
    add     r11, r10, r11, lsl #1
    str     lr, [r0], #4           /* STORE color2 pixel A             */
    str     lr, [r0], #4           /* STORE color2 pixel B (doubled)   */
    ldr     r11, [r2, r11, lsl #2] /* color3                           */
    str     r11, [r0], #4          /* STORE color3 pixel A             */
    str     r11, [r0], #4          /* STORE color3 pixel B (doubled)   */

    subs    r8, r8, #4
    bge     .Liso_loop4

.Liso_tail_entry:
    adds    r8, r8, #4
    beq     .Liso_done

.Liso_tail1:
    asr     r3, r4, #12
    and     r3, r3, #31
    asr     r9, r5, #12
    and     r9, r9, #31
    eor     r10, r3, r9
    orr     r11, r3, r9, lsl #5
    add     r4, r4, r6
    add     r5, r5, r7
    ldrb    r11, [r1, r11]
    and     r10, r10, #1
    add     r11, r10, r11, lsl #1
    ldr     r11, [r2, r11, lsl #2]
    str     r11, [r0], #4          /* pixel A                           */
    str     r11, [r0], #4          /* pixel B (doubled)                 */
    subs    r8, r8, #1
    bgt     .Liso_tail1

.Liso_done:
    pop     {r4-r11, pc}
    .size   iso_scanline, . - iso_scanline
