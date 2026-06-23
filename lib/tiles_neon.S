/*
 * Jupiter SDK — tiles_neon.S
 * NEON-accelerated tile row blitter for Cortex-A7.
 *
 * The fast tile renderer's bottleneck is 8 scalar stores per tile.
 * GCC can't auto-vectorize because the color changes per tile (LUT load).
 * This ASM version:
 *   - Processes 2 tiles per iteration (interleaved to hide LUT latency)
 *   - Uses VDUP + VST1 to blast 8 pixels per tile in 2 stores
 *   - Interleaves tile B's lookup with tile A's stores
 *
 * C handles scroll math, partial edge tiles, and row dispatch.
 * ASM handles the full-tile inner loop where 90%+ of pixels live.
 */

    .syntax unified
    .arch   armv7-a
    .fpu    neon-vfpv4
    .arm

/* ================================================================
 * _tile_row_blast — fill full 8-pixel tiles from map+LUT
 * ================================================================
 *
 * void _tile_row_blast(uint32_t *dst,
 *                       const uint8_t *map_row,
 *                       const uint32_t *color_lut,
 *                       uint32_t tx_start,
 *                       uint32_t map_mask,
 *                       uint32_t num_tiles);
 *
 *   r0 = destination pixel pointer (advances by 8 pixels per tile)
 *   r1 = map row base pointer
 *   r2 = color LUT pointer (256 entries × 4 bytes)
 *   r3 = starting tile X index
 *   [sp+16] = map_mask (power-of-2 map width minus 1)
 *   [sp+20] = num_tiles (number of full tiles to draw)
 *
 * Each tile: look up tile index from map, look up color from LUT,
 * write 8 pixels of that color. VDUP splats the color to q0,
 * two VST1.32 of q0 stores 4+4 = 8 pixels.
 */
    .section .text, "ax"
    .global _tile_row_blast
    .type   _tile_row_blast, %function
    .align  4

_tile_row_blast:
    push    {r4-r6, lr}

    ldr     r4, [sp, #16]          /* map_mask                          */
    ldr     r5, [sp, #20]          /* num_tiles                         */

    cmp     r5, #0
    beq     .Ltb_done

    /* ---- Try 2-tile unrolled loop ---- */
    subs    r5, r5, #2
    blt     .Ltb_last_one

.Ltb_loop2:
    /* Tile A: map lookup */
    and     r6, r3, r4             /* tx & map_mask                     */
    ldrb    r6, [r1, r6]           /* tile_id = map_row[wrapped_tx]     */
    add     r3, r3, #1             /* tx++ (tile A done)                */

    /* Tile B: map lookup (interleaved — tile A's ldrb in flight) */
    and     r12, r3, r4
    ldrb    r12, [r1, r12]
    add     r3, r3, #1             /* tx++ (tile B done)                */

    /* Tile A: color LUT → NEON splat → store 8 pixels */
    ldr     r6, [r2, r6, lsl #2]   /* color_a = lut[tile_id]           */
    vdup.32 q0, r6                 /* splat to 4 lanes                  */
    vst1.32 {q0}, [r0]!           /* pixels 0-3                        */
    vst1.32 {q0}, [r0]!           /* pixels 4-7                        */

    /* Tile B: color LUT → NEON splat → store 8 pixels */
    ldr     r12, [r2, r12, lsl #2]
    vdup.32 q1, r12
    vst1.32 {q1}, [r0]!
    vst1.32 {q1}, [r0]!

    subs    r5, r5, #2
    bge     .Ltb_loop2

.Ltb_last_one:
    /* Check if there's one remaining tile (odd count) */
    adds    r5, r5, #2
    beq     .Ltb_done

    /* Single remaining tile */
    and     r6, r3, r4
    ldrb    r6, [r1, r6]
    ldr     r6, [r2, r6, lsl #2]
    vdup.32 q0, r6
    vst1.32 {q0}, [r0]!
    vst1.32 {q0}, [r0]!

.Ltb_done:
    pop     {r4-r6, pc}

    .size _tile_row_blast, . - _tile_row_blast
