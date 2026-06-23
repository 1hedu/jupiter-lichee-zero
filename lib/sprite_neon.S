/*
 * Jupiter SDK — sprite_neon.S
 * Hand-tuned NEON inner loops for sprite blitting on Cortex-A7.
 *
 * Three functions, one principle: zero branches in the pixel loop.
 *
 * The A7 is in-order. A mispredicted branch costs 10–13 cycles.
 * A 50%-transparent sprite mispredicts half its pixels in the C
 * version = thousands of wasted cycles per sprite. NEON bitwise
 * select eliminates every one.
 *
 * Calling convention: AAPCS
 *   r0–r3 arguments, r4–r11 callee-saved, r12/ip scratch
 *   d0–d7 (q0–q3) caller-saved, d8–d15 (q4–q7) callee-saved
 *   All three functions stay within caller-saved NEON registers.
 *
 * Pixel format: ARGB8888 little-endian (memory order: B, G, R, A)
 * Target: ARM Cortex-A7 (ARMv7-A) with NEON
 */

    .syntax unified
    .arch   armv7-a
    .fpu    neon-vfpv4
    .arm
    .text


/* ================================================================
 * _sprite_row_keyed — branchless alpha-key blit, one scanline
 * ================================================================
 *
 * Alpha-key: any pixel with alpha != 0 overwrites destination.
 * Pixels with alpha == 0 are transparent (destination preserved).
 *
 * void _sprite_row_keyed(const uint32_t *src,   // r0
 *                         uint32_t *dst,         // r1
 *                         uint32_t count);       // r2
 *
 * NEON strategy (4 pixels per iteration):
 *   VSHR  — isolate alpha byte into low bits of each 32-bit lane
 *   VCGT  — compare > 0: mask = 0xFFFFFFFF where opaque
 *   VBIT  — insert src into dst where mask is set (opaque pixels)
 *   VST1  — store merged result
 *
 * ~6 cycles per 4 pixels. Zero branches.
 * ================================================================ */
    .global _sprite_row_keyed
    .type   _sprite_row_keyed, %function
    .align  4

_sprite_row_keyed:
    /* Need at least 4 pixels for NEON path */
    subs    r2, r2, #4
    blt     .Lsk_tail

.Lsk_loop4:
    vld1.32 {q0}, [r0]!            /* q0 = 4 source pixels              */
    vld1.32 {q1}, [r1]             /* q1 = 4 destination pixels         */

    /* Build mask: 0xFFFFFFFF where alpha == 0 (transparent) */
    vshr.u32 q2, q0, #24           /* isolate alpha byte per lane       */
    vceq.u32 q2, q2, #0            /* mask: all-ones where alpha == 0   */

    /* VBIF: "bit insert if false" — where mask bit is 0, dst gets src.
     *   opaque  (mask=0): q1 gets q0 (source overwrites dest)   ✓
     *   transp  (mask=1): q1 unchanged (destination preserved)  ✓       */
    vbif    q1, q0, q2

    vst1.32 {q1}, [r1]!            /* store 4 merged pixels             */

    subs    r2, r2, #4
    bge     .Lsk_loop4

.Lsk_tail:
    adds    r2, r2, #4             /* restore remaining count           */
    bxeq    lr                     /* exactly 0 remaining → done        */

    /* Scalar tail: 1–3 pixels. Conditional store, no NEON. */
.Lsk_tail1:
    ldr     r3, [r0], #4
    tst     r3, #0xFF000000        /* test alpha byte                   */
    strne   r3, [r1]               /* ARM conditional store             */
    add     r1, r1, #4
    subs    r2, r2, #1
    bgt     .Lsk_tail1

    bx      lr
    .size   _sprite_row_keyed, . - _sprite_row_keyed


/* ================================================================
 * _sprite_row_keyed_flip — branchless alpha-key blit, reversed src
 * ================================================================
 *
 * Horizontally flipped sprite blit. Source reads right-to-left,
 * destination writes left-to-right.
 *
 * void _sprite_row_keyed_flip(const uint32_t *src_right,  // r0
 *                              uint32_t *dst,              // r1
 *                              uint32_t count);            // r2
 *
 *   src_right: pointer to RIGHTMOST visible source pixel.
 *   The function reads: src_right[0], src_right[-1], ...
 *
 * NEON strategy:
 *   Load 4 pixels from (src_right - 3) in forward memory order.
 *   VREV64.32 + VSWP reverses all 4 lanes (full 128-bit reverse).
 *   Then same VCGT + VBIT alpha-key merge as non-flipped.
 *
 * The forward load is prefetch-friendly even though we're logically
 * reading backward. Only the reversal is extra — one VREV + one VSWP.
 * ================================================================ */
    .global _sprite_row_keyed_flip
    .type   _sprite_row_keyed_flip, %function
    .align  4

_sprite_row_keyed_flip:
    subs    r2, r2, #4
    blt     .Lsf_tail

.Lsf_loop4:
    /* Load 4 pixels ending at src_right.
     * Memory: [R-3, R-2, R-1, R]  (forward in memory)
     * After reverse: [R, R-1, R-2, R-3]  (logical left-to-right) */
    sub     r3, r0, #12            /* r3 = src_right - 3 pixels         */
    vld1.32 {q0}, [r3]             /* load 4 ascending                  */
    sub     r0, r0, #16            /* advance source LEFT by 4 pixels   */

    /* Full 128-bit reverse of 32-bit lanes:
     * VREV64.32 swaps pairs within each 64-bit half.
     *   {A, B, C, D} → {B, A, D, C}
     * VSWP swaps the two 64-bit halves.
     *   {B, A, D, C} → {D, C, B, A}                                */
    vrev64.32 q0, q0
    vswp    d0, d1

    /* Same branchless alpha-key merge */
    vld1.32 {q1}, [r1]
    vshr.u32 q2, q0, #24
    vceq.u32 q2, q2, #0
    vbif    q1, q0, q2
    vst1.32 {q1}, [r1]!

    subs    r2, r2, #4
    bge     .Lsf_loop4

.Lsf_tail:
    adds    r2, r2, #4
    bxeq    lr

    /* Scalar tail: read backward one pixel at a time */
.Lsf_tail1:
    ldr     r3, [r0], #-4          /* load pixel, move source LEFT      */
    tst     r3, #0xFF000000
    strne   r3, [r1]
    add     r1, r1, #4
    subs    r2, r2, #1
    bgt     .Lsf_tail1

    bx      lr
    .size   _sprite_row_keyed_flip, . - _sprite_row_keyed_flip


/* ================================================================
 * _sprite_row_blend — premultiplied alpha blend, one scanline
 * ================================================================
 *
 * Born in assembly. No C prototype. The showcase function.
 *
 * Blend equation (premultiplied alpha, Porter-Duff "over"):
 *   result = src + dst × (1 – src_alpha)
 *
 * Premultiplied source means RGB channels are pre-scaled:
 *   stored_R = original_R × alpha / 255
 * This eliminates one multiply per channel at runtime.
 *
 * void _sprite_row_blend(const uint32_t *src,   // r0
 *                         uint32_t *dst,         // r1
 *                         uint32_t count);       // r2
 *
 * NEON processes 8 pixels per iteration:
 *   VLD4.8  deinterleaves 8 ARGB pixels into B/G/R/A byte planes
 *   VMVN    computes inv_alpha = 255 – src_alpha (bitwise NOT of u8)
 *   VMULL   dst_channel × inv_alpha → 16-bit product (3 channels)
 *   VRSHRN  rounded >> 8 ≈ /255 (max error: ±1 LSB, invisible)
 *   VADD    src + scaled_dst = final blend
 *   VST4.8  re-interleaves channels and stores
 *
 * Why VRSHRN instead of exact /255?
 *   Exact: (x * 257 + 256) >> 16. Costs 3 extra instructions/channel.
 *   VRSHRN: (x + 128) >> 8. One instruction. ±1 error out of 255.
 *   At 8 bits per channel the difference is imperceptible.
 *
 * ~16 cycles per 8 pixels. Fully pipelined. Zero branches.
 * ================================================================ */
    .global _sprite_row_blend
    .type   _sprite_row_blend, %function
    .align  4

_sprite_row_blend:
    push    {r4-r9, lr}            /* scalar tail needs work regs       */

    subs    r2, r2, #8
    blt     .Lb_tail_entry

.Lb_loop8:
    /* Deinterleave 8 src pixels into channel planes.
     * Little-endian ARGB8888: byte 0=B, 1=G, 2=R, 3=A.
     * VLD4.8 distributes byte N of each pixel into d(N).
     *   d0 = 8 × blue    d1 = 8 × green
     *   d2 = 8 × red     d3 = 8 × alpha                           */
    vld4.8  {d0, d1, d2, d3}, [r0]!

    /* 8 destination pixels, same deinterleave */
    vld4.8  {d4, d5, d6, d7}, [r1]

    /* inv_alpha = 255 – src_alpha.
     * VMVN on u8: bitwise NOT = 255 – x.
     * d16 is in scratch range (q8, not callee-saved). */
    vmvn.u8 d16, d3

    /* Scale dst channels by inverse alpha → 16-bit.
     * Three independent multiplies — fully pipelined on A7 NEON.  */
    vmull.u8 q9,  d4, d16          /* dst_B × inv_a → 16-bit           */
    vmull.u8 q10, d5, d16          /* dst_G × inv_a                    */
    vmull.u8 q11, d6, d16          /* dst_R × inv_a                    */

    /* Rounded shift right + narrow: 16-bit → 8-bit.
     * VRSHRN adds 1<<(shift-1) before truncating.
     * Approximates /255 with /256 + rounding. */
    vrshrn.u16 d4, q9,  #8
    vrshrn.u16 d5, q10, #8
    vrshrn.u16 d6, q11, #8

    /* Final: result = src_premul + scaled_dst */
    vadd.u8 d0, d0, d4             /* result_B                          */
    vadd.u8 d1, d1, d5             /* result_G                          */
    vadd.u8 d2, d2, d6             /* result_R                          */
    /* d3 = src alpha → passes through. DE2 blender reads this
     * for compositing the UI overlay against the VI layer. */

    /* Re-interleave and store */
    vst4.8  {d0, d1, d2, d3}, [r1]!

    subs    r2, r2, #8
    bge     .Lb_loop8

.Lb_tail_entry:
    adds    r2, r2, #8
    beq     .Lb_done

    /* ---- Scalar tail: 1–7 pixels ----
     * Full premultiplied blend. Runs at most 7× per row. */
.Lb_tail1:
    ldr     r3, [r0], #4           /* src pixel                         */
    ldr     r4, [r1]               /* dst pixel                         */

    lsr     r5, r3, #24            /* src_alpha                         */
    rsb     r6, r5, #255           /* inv_alpha                         */

    /* Blue (bits 7:0) */
    and     r7, r4, #0xFF
    mul     r7, r7, r6
    lsr     r7, r7, #8
    and     r8, r3, #0xFF
    add     r7, r8, r7             /* result_B                          */

    /* Green (bits 15:8) */
    ubfx    r8, r4, #8, #8
    mul     r8, r8, r6
    lsr     r8, r8, #8
    ubfx    r9, r3, #8, #8
    add     r8, r9, r8             /* result_G                          */

    /* Red (bits 23:16) */
    ubfx    r9, r4, #16, #8
    mul     r9, r9, r6
    lsr     r9, r9, #8
    ubfx    ip, r3, #16, #8
    add     r9, ip, r9             /* result_R                          */

    /* Clamp (shouldn't overflow if premul is correct, but defensive) */
    cmp     r7, #255
    movgt   r7, #255
    cmp     r8, #255
    movgt   r8, #255
    cmp     r9, #255
    movgt   r9, #255

    /* Pack ARGB8888 */
    orr     r7, r7, r8, lsl #8
    orr     r7, r7, r9, lsl #16
    orr     r7, r7, r5, lsl #24
    str     r7, [r1], #4

    subs    r2, r2, #1
    bgt     .Lb_tail1

.Lb_done:
    pop     {r4-r9, pc}
    .size   _sprite_row_blend, . - _sprite_row_blend


/* ================================================================
 * _sprite_row_scaled — nearest-neighbor scaled alpha-key blit
 * ================================================================
 *
 * Renders one row of a scaled sprite. Each destination pixel samples
 * a source pixel at a fixed-point U coordinate, stepping by du per
 * destination pixel. Nearest-neighbor: no filtering, preserves pixel
 * art aesthetic.
 *
 * This is a scatter-load problem — every pixel reads a different
 * source address. NEON can't gather. So we stay fully scalar but:
 *   - 4x unrolled to interleave load latency with next texel's index
 *   - ARM conditional store (STRNE) for alpha-key: zero branches
 *   - ~6 cycles per pixel on A7 in-order pipeline
 *
 * void _sprite_row_scaled(const uint32_t *src_row,  // r0
 *                          uint32_t *dst,            // r1
 *                          uint32_t count,           // r2  dest pixels
 *                          uint32_t u_fp,            // r3  U start (16.16)
 *                          uint32_t du_fp);          // [sp] U step (16.16)
 *
 * Fixed point: 16.16. Source X = u_fp >> 16. Fractional bits select
 * the nearest source pixel — no interpolation.
 *
 * The caller (C wrapper) handles:
 *   - Destination-space clipping
 *   - Mapping clipped left edge to u_start offset
 *   - Computing which source row to read (via V stepping)
 * ================================================================ */
    .global _sprite_row_scaled
    .type   _sprite_row_scaled, %function
    .align  4

_sprite_row_scaled:
    push    {r4-r8, lr}

    ldr     r4, [sp, #24]          /* du_fp (push saved 6 regs × 4 = 24) */

    /* 4-wide unrolled main loop */
    subs    r2, r2, #4
    blt     .Lss_tail_entry

.Lss_loop4:
    /* Texel 0: index + load (interleaves with pipeline bubbles) */
    lsr     r5, r3, #16            /* sx0 = u >> 16                     */
    ldr     r5, [r0, r5, lsl #2]   /* px0 = src_row[sx0]               */
    add     r3, r3, r4             /* u += du                           */

    /* Texel 1: overlaps texel 0's load-use latency */
    lsr     r6, r3, #16
    ldr     r6, [r0, r6, lsl #2]
    add     r3, r3, r4

    /* Texel 2 */
    lsr     r7, r3, #16
    ldr     r7, [r0, r7, lsl #2]
    add     r3, r3, r4

    /* Texel 3 */
    lsr     r8, r3, #16
    ldr     r8, [r0, r8, lsl #2]
    add     r3, r3, r4

    /* Alpha-key conditional stores: no branches.
     * TST sets Z flag from alpha byte.
     * STRNE writes only if alpha != 0 (Z clear).
     * Destination advances unconditionally. */
    tst     r5, #0xFF000000
    strne   r5, [r1]
    tst     r6, #0xFF000000
    strne   r6, [r1, #4]
    tst     r7, #0xFF000000
    strne   r7, [r1, #8]
    tst     r8, #0xFF000000
    strne   r8, [r1, #12]
    add     r1, r1, #16            /* dst += 4 pixels                   */

    subs    r2, r2, #4
    bge     .Lss_loop4

.Lss_tail_entry:
    adds    r2, r2, #4
    beq     .Lss_done

    /* Scalar tail: 1–3 pixels */
.Lss_tail1:
    lsr     r5, r3, #16
    ldr     r5, [r0, r5, lsl #2]
    add     r3, r3, r4
    tst     r5, #0xFF000000
    strne   r5, [r1]
    add     r1, r1, #4
    subs    r2, r2, #1
    bgt     .Lss_tail1

.Lss_done:
    pop     {r4-r8, pc}
    .size   _sprite_row_scaled, . - _sprite_row_scaled


/* ================================================================
 * _sprite_row_indexed — palette lookup + alpha-key blit
 * ================================================================
 *
 * Source pixels are 8-bit palette indices. Each index looks up an
 * ARGB8888 color from a 256-entry palette LUT. The resolved color
 * is then alpha-key blitted (same as _sprite_row_keyed).
 *
 * Why palette sprites matter:
 *   - 4x memory savings (1 byte vs 4 bytes per pixel)
 *   - Palette swap = change LUT pointer (damage flash, elemental
 *     variants, silhouettes, day/night — all free)
 *   - 64MB DRAM is shared. 200 frames × 64×64 × 4B = 3.2MB.
 *     Indexed: 800KB. That's an entire second character set saved.
 *
 * Inner loop: 4-wide unrolled. Each iteration loads 4 byte indices,
 * does 4 LUT lookups (scalar — can't gather), then 4 conditional
 * stores. Same TST+STRNE pattern as _sprite_row_scaled.
 *
 * void _sprite_row_indexed(const uint8_t *src_row,   // r0
 *                           uint32_t *dst,            // r1
 *                           const uint32_t *palette,  // r2
 *                           uint32_t count);          // r3
 * ================================================================ */
    .global _sprite_row_indexed
    .type   _sprite_row_indexed, %function
    .align  4

_sprite_row_indexed:
    push    {r4-r8, lr}

    /* 4-wide unrolled main loop */
    subs    r3, r3, #4
    blt     .Lsi_tail_entry

.Lsi_loop4:
    /* Load 4 palette indices (bytes). LDRB has 1-cycle latency on A7,
     * so interleaving 4 of them fills the pipeline nicely. */
    ldrb    r4, [r0], #1           /* idx0                              */
    ldrb    r5, [r0], #1           /* idx1                              */
    ldrb    r6, [r0], #1           /* idx2                              */
    ldrb    r7, [r0], #1           /* idx3                              */

    /* Palette lookups: ARGB color = palette[index] */
    ldr     r4, [r2, r4, lsl #2]  /* color0 = palette[idx0]            */
    ldr     r5, [r2, r5, lsl #2]  /* color1                            */
    ldr     r6, [r2, r6, lsl #2]  /* color2                            */
    ldr     r7, [r2, r7, lsl #2]  /* color3                            */

    /* Alpha-key conditional stores */
    tst     r4, #0xFF000000
    strne   r4, [r1]
    tst     r5, #0xFF000000
    strne   r5, [r1, #4]
    tst     r6, #0xFF000000
    strne   r6, [r1, #8]
    tst     r7, #0xFF000000
    strne   r7, [r1, #12]
    add     r1, r1, #16

    subs    r3, r3, #4
    bge     .Lsi_loop4

.Lsi_tail_entry:
    adds    r3, r3, #4
    beq     .Lsi_done

.Lsi_tail1:
    ldrb    r4, [r0], #1
    ldr     r4, [r2, r4, lsl #2]
    tst     r4, #0xFF000000
    strne   r4, [r1]
    add     r1, r1, #4
    subs    r3, r3, #1
    bgt     .Lsi_tail1

.Lsi_done:
    pop     {r4-r8, pc}
    .size   _sprite_row_indexed, . - _sprite_row_indexed


/* ================================================================
 * _sprite_row_rotscale — affine-transformed alpha-key blit
 * ================================================================
 *
 * Renders one row of a rotation+scale transformed sprite. This is
 * per-sprite Mode 7: each destination pixel maps back through a 2D
 * affine transform to sample the source sprite.
 *
 * Transform: for each dest pixel at (dx, dy):
 *   src_u = u_start + dx * du_x
 *   src_v = v_start + dx * dv_x
 *   sample source at (src_u >> 16, src_v >> 16)
 *
 * du_x and dv_x encode the rotation angle and scale factor.
 * The C wrapper precomputes these from sin/cos LUTs.
 *
 * Bounds checking: pixels that map outside the source rect are
 * skipped (transparent). This lets rotated sprites have clean edges
 * without needing a bounding box of transparent pixels.
 *
 * 4-wide unrolled. Scatter-load bound (same as scaler and Mode 7).
 * ~8 cycles per pixel (extra cost: V stepping + bounds check).
 *
 * void _sprite_row_rotscale(const uint32_t *src,     // r0
 *                            uint32_t *dst,           // r1
 *                            uint32_t count,          // r2  dest pixels
 *                            int32_t u_fp,            // r3  U start (16.16)
 *                            int32_t v_fp,            // [sp+0] V start
 *                            int32_t du,              // [sp+4] U step per px
 *                            int32_t dv,              // [sp+8] V step per px
 *                            uint32_t src_w,          // [sp+12] source width
 *                            uint32_t src_h);         // [sp+16] source height
 * ================================================================ */
    .global _sprite_row_rotscale
    .type   _sprite_row_rotscale, %function
    .align  4

_sprite_row_rotscale:
    push    {r4-r11, lr}

    /* Load stack args. 9 regs pushed × 4 = 36 bytes offset. */
    ldr     r4,  [sp, #36]         /* v_fp                              */
    ldr     r5,  [sp, #40]         /* du (U step per dest pixel)        */
    ldr     r6,  [sp, #44]         /* dv (V step per dest pixel)        */
    ldr     r10, [sp, #48]         /* src_w                             */
    ldr     r11, [sp, #52]         /* src_h                             */

    /*
     * Register allocation:
     *   r0  = src base (constant)
     *   r1  = dst pointer (advances)
     *   r2  = loop counter (counts down)
     *   r3  = u (fixed 16.16, advances by du)
     *   r4  = v (fixed 16.16, advances by dv)
     *   r5  = du (constant)
     *   r6  = dv (constant)
     *   r7  = scratch (pixel value / sx)
     *   r8  = scratch (sy)
     *   r9  = scratch
     *   r10 = src_w (constant)
     *   r11 = src_h (constant)
     *   r12 = scratch (ip)
     *   lr  = scratch
     */

    cmp     r2, #0
    beq     .Lrs_done

.Lrs_loop1:
    /* Extract integer source coordinates */
    asr     r7, r3, #16            /* sx = u >> 16 (signed)             */
    asr     r8, r4, #16            /* sy = v >> 16 (signed)             */

    /* Bounds check: skip if outside source rect.
     * Using unsigned compare trick: if sx < 0, it becomes a huge
     * positive number which fails (sx >= src_w). Same for sy. */
    cmp     r7, r10                /* sx >= src_w? (unsigned catches <0) */
    bhs     .Lrs_skip
    cmp     r8, r11                /* sy >= src_h?                      */
    bhs     .Lrs_skip

    /* Sample source pixel: src[sy * src_w + sx] */
    mla     r9, r8, r10, r7       /* index = sy * src_w + sx           */
    ldr     r9, [r0, r9, lsl #2]  /* pixel = src[index]                */

    /* Alpha-key store */
    tst     r9, #0xFF000000
    strne   r9, [r1]

.Lrs_skip:
    add     r3, r3, r5             /* u += du                           */
    add     r4, r4, r6             /* v += dv                           */
    add     r1, r1, #4             /* dst++ (always, even if skipped)   */
    subs    r2, r2, #1
    bne     .Lrs_loop1

.Lrs_done:
    pop     {r4-r11, pc}
    .size   _sprite_row_rotscale, . - _sprite_row_rotscale


