/*
 * Jupiter SDK — mem.c
 * Memory fill and copy operations: scalar and NEON-accelerated.
 *
 * Performance (with I-cache, without D-cache):
 *   memset32:      179 MB/s
 *   memset32_neon: 1228 MB/s
 */
#include "jupiter.h"

void memset32(uint32_t addr, uint32_t val, uint32_t count)
{
    volatile uint32_t *p = (volatile uint32_t *)addr;
    for (uint32_t i = 0; i < count; i++)
        p[i] = val;
}

void memset32_neon(uint32_t addr, uint32_t val, uint32_t bytes)
{
    __asm__ volatile(
        "vdup.32 q0, %[val]          \n"
        "vmov    q1, q0              \n"
        "vmov    q2, q0              \n"
        "vmov    q3, q0              \n"
        "1:                          \n"
        "vstm    %[dst]!, {q0-q3}   \n"
        "subs    %[len], %[len], #64 \n"
        "bgt     1b                  \n"
        : [dst] "+r"(addr), [len] "+r"(bytes)
        : [val] "r"(val)
        : "q0", "q1", "q2", "q3", "memory"
    );
}

void memcpy_neon(void *dst, const void *src, uint32_t bytes)
{
    /* 64 bytes per iteration: VLD + VST of 4 quad registers.
     * bytes must be a multiple of 64. For row copies (480×4 = 1920)
     * that's 30 iterations — one Mode 7 row copy in ~1.5µs.
     *
     * Handles the tail (bytes not multiple of 64) with a scalar
     * word-copy fallback. */
    uint32_t bulk = bytes & ~63u;   /* round down to 64 */
    uint32_t tail = bytes & 63u;

    if (bulk > 0) {
        __asm__ volatile(
            "1:                              \n"
            "vldm    %[s]!, {q0-q3}         \n"
            "vstm    %[d]!, {q0-q3}         \n"
            "subs    %[len], %[len], #64    \n"
            "bgt     1b                     \n"
            : [d] "+r"(dst), [s] "+r"(src), [len] "+r"(bulk)
            :
            : "q0", "q1", "q2", "q3", "memory"
        );
    }

    /* Scalar tail: 0–60 bytes in 4-byte steps */
    uint32_t *d32 = (uint32_t *)dst;
    const uint32_t *s32 = (const uint32_t *)src;
    uint32_t words = tail >> 2;
    for (uint32_t i = 0; i < words; i++)
        d32[i] = s32[i];
}
