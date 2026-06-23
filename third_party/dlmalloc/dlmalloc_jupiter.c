/* Jupiter wrapper for Doug Lea's dlmalloc 2.8.6.
 *
 * Compile-time options chosen so dlmalloc runs in our bare-metal V3s
 * environment with no kernel, no threads, no syscalls, no errno. The
 * MORECORE callback bumps from a static 24 MB arena (formerly the
 * libc_shim heap region); dlmalloc's own free-list/segregated-fits
 * machinery layers on top so std::list/map/stack node alloc-and-free
 * cycles don't recycle addresses unsafely.
 *
 * Use: link this .c instead of lib/libc_shim.c's malloc/free/realloc/
 * calloc bodies. Everything else in libc_shim (memset, errno, etc.)
 * stays put.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>   /* memset, memcpy declarations (impls from arm_opt) */

extern void uart_puts(const char *);

/* dlmalloc references errno values when it falls back from system calls
 * (mostly ifdef'd out by our LACKS_* flags, but a couple of paths still
 * compile). Define them locally so the source compiles; we don't actually
 * propagate errno to user code. */
#define EINVAL 22
#define ENOMEM 12
#define errno  (*(__errno()))
extern int *__errno(void);

/* Disable USE_BUILTIN_FFS so dlmalloc uses its own bit-twiddling; the
 * ARM toolchain headerless build doesn't expose ffs(). */
#undef USE_BUILTIN_FFS

/* Prefix dlmalloc's exported symbols with `dl` so we can put thin
 * malloc/free/realloc/calloc wrappers in front and capture the caller's
 * link register before entering the allocator. The LR capture lets the
 * corruption-detected abort point to the actual upstream caller (e.g.
 * an offending operator new in widgets.cpp) instead of dlmalloc's own
 * internals. */
#define USE_DL_PREFIX 1

/* ---- dlmalloc compile-time configuration ---- */
#define HAVE_MMAP 0           /* no mmap, no kernel */
#define HAVE_MORECORE 1
#define MORECORE jupiter_morecore
/* The two arenas (s_dl_main in .bss, s_dl_low in .bss_low) are ~30 MB
 * apart in the address space — definitely NOT contiguous. With
 * MORECORE_CONTIGUOUS=1 dlmalloc would assume successive morecore
 * returns are adjacent and write chunk-boundary metadata across the
 * gap, scribbling on whatever .bss objects sit between (including
 * std::list sentinel nodes). Set it to 0 so each morecore return is
 * treated as its own segment. */
#define MORECORE_CONTIGUOUS 0
#define MORECORE_CANNOT_TRIM 1  /* never give memory back */
#define USE_LOCKS 0           /* single-threaded */
#define LACKS_UNISTD_H
#define LACKS_FCNTL_H
#define LACKS_STDLIB_H
#define LACKS_STRING_H
#define LACKS_STRINGS_H
#define LACKS_SYS_TYPES_H
#define LACKS_SYS_PARAM_H
#define LACKS_SYS_MMAN_H
#define LACKS_ERRNO_H
#define LACKS_SCHED_H
#define LACKS_TIME_H
#define NO_MALLOC_STATS 1
#define NO_MALLINFO 1
#define MALLOC_INSPECT_ALL 0
/* FOOTERS=1 adds an 8-byte size-validation footer per chunk. Free/realloc
 * compare the footer against the header — if a payload overflow trampled
 * the footer, the abort fires immediately at free time (with our caller-
 * LR + stack-scan diagnostic) instead of corrupting silently into a
 * later malloc or std-container hang. Costs 8 bytes per chunk; worth it
 * while we hunt the remaining overflow sources. */
#define FOOTERS 1
#define MALLOC_ALIGNMENT ((size_t)8U)
#define ABORT             jupiter_dlmalloc_abort_with_lr(__builtin_return_address(0))
#define ABORT_ON_ASSERT_FAILURE 0
#define MALLOC_FAILURE_ACTION

/* dlmalloc references these libc bits — provide thin shims (the real
 * impls live in lib/libc_shim.c / lib/arm_opt). */
typedef long  ptrdiff_t_jupiter;
typedef unsigned int  jupiter_size_t;

/* dlmalloc uses memset / memcpy / strchr-style ops via internal macros;
 * with LACKS_STRING_H it provides its own fallbacks. We don't need to
 * supply anything extra. */

extern void uart_puthex(unsigned);
extern void uart_putdec(unsigned);

/* Forward decls — definitions further down. */
extern unsigned long libc_shim_heap_used(void);

extern void jupiter_dlmalloc_dump_last_caller(void);

static void jupiter_dlmalloc_abort_with_lr(void *lr) __attribute__((noreturn));
static void jupiter_dlmalloc_abort_with_lr(void *lr)
{
    uart_puts("\n========================================\n");
    uart_puts("[dlmalloc] ABORT - heap corruption detected\n");
    uart_puts("  detected-at PC (LR) = 0x");
    uart_puthex((unsigned)(uintptr_t)lr);
    uart_puts("\n  heap used = ");
    uart_putdec((unsigned)libc_shim_heap_used());
    uart_puts("\n");
    jupiter_dlmalloc_dump_last_caller();
    uart_puts("  Resolve via:\n");
    uart_puts("    arm-none-eabi-addr2line -e build/jupiter.elf -f -C 0x");
    uart_puthex((unsigned)(uintptr_t)lr);
    uart_puts("\n========================================\n");
    for (;;) {}
}

/* Fallback for any path that still references the old name. */
static void jupiter_dlmalloc_abort(void) __attribute__((noreturn, used));
static void jupiter_dlmalloc_abort(void)
{
    jupiter_dlmalloc_abort_with_lr(__builtin_return_address(0));
}

/* ---- MORECORE: main arena + overflow.
 *
 * dlmalloc calls morecore(incr) to grow the heap. Returns the OLD break
 * (i.e. the bottom of the new region), or (void*)-1 on failure. incr can
 * be negative to release; we ignore negative requests since
 * MORECORE_CANNOT_TRIM is set.
 *
 * WC1 needs a big arena (Stratagus + War1gus pull megabytes through
 * malloc — Lua strings, std::vector growth, audio buffers). The menu /
 * synth-rt builds barely use the heap and their big static synth tables
 * already eat most of bss; baking 24 MB of arena in there too would
 * push past the 40 MB CODE region. WAR1_BUILD is set by the Makefile
 * when GAME path matches `war1`; otherwise stay slim. */
#ifdef WAR1_BUILD
#define DL_MAIN_ARENA_BYTES   (24u * 1024u * 1024u)
#define DL_LOW_ARENA_BYTES    (13u * 1024u * 1024u)
#else
#define DL_MAIN_ARENA_BYTES   (1u  * 1024u * 1024u)
#define DL_LOW_ARENA_BYTES    (1u  * 1024u * 1024u)
#endif

static uint8_t  s_dl_main[DL_MAIN_ARENA_BYTES] __attribute__((aligned(16)));
static uint8_t  s_dl_low[DL_LOW_ARENA_BYTES] __attribute__((section(".bss_low"), aligned(16)));
static uint32_t s_dl_main_used = 0;
static uint32_t s_dl_low_used  = 0;

void *jupiter_morecore(long incr);
void *jupiter_morecore(long incr)
{
    if (incr < 0) {
        /* MORECORE_CANNOT_TRIM is set, so dlmalloc shouldn't request
         * shrinkage. Defensive: ignore. */
        return (void *)(s_dl_main + s_dl_main_used);
    }
    if (incr == 0) {
        return (void *)(s_dl_main + s_dl_main_used);
    }
    uint32_t want = (uint32_t)incr;
    if (s_dl_main_used + want <= DL_MAIN_ARENA_BYTES) {
        void *prev = (void *)(s_dl_main + s_dl_main_used);
        s_dl_main_used += want;
        return prev;
    }
    /* Main arena exhausted — fall to overflow. dlmalloc treats every
     * MORECORE return as a contiguous segment, so once we hand back from
     * s_dl_low we tell dlmalloc that segments are not contiguous (set
     * MORECORE_CONTIGUOUS=0... but that's compile-time). To keep things
     * simple, dlmalloc handles non-contiguous returns by treating each
     * morecore return as its own segment when it sees a gap. We rely on
     * dlmalloc's segment list for that. */
    if (s_dl_low_used + want <= DL_LOW_ARENA_BYTES) {
        void *prev = (void *)(s_dl_low + s_dl_low_used);
        s_dl_low_used += want;
        return prev;
    }
    uart_puts("[dlmalloc] morecore exhausted\n");
    return (void *)-1;
}

/* Diagnostics — kept name-compatible with libc_shim's helpers so
 * existing callers (heap probes in war1_link_stubs) keep working. */
unsigned long libc_shim_heap_used(void);
unsigned long libc_shim_heap_size(void);
unsigned long libc_shim_heap_used(void) { return (unsigned long)(s_dl_main_used + s_dl_low_used); }
unsigned long libc_shim_heap_size(void) { return (unsigned long)(DL_MAIN_ARENA_BYTES + DL_LOW_ARENA_BYTES); }

/* ---- dlmalloc body ---- */
#include "dlmalloc.c"

/* ---- thin wrappers that capture caller LR ---- */
/* When dlmalloc detects corruption, the abort path's LR points into
 * dlmalloc itself (operator new wrapper). The caller we actually want
 * is the one that called malloc/free/realloc — typically C++ operator
 * new from libstdc++ or a direct upstream malloc. Stash that LR here
 * on every entry; abort prints it. */
static void * volatile g_last_alloc_caller   = (void *)0;
static unsigned        g_last_alloc_op       = 0; /* 1=malloc 2=free 3=realloc 4=calloc */
static size_t          g_last_alloc_size     = 0;
static void *          g_last_alloc_ptr      = (void *)0;

void *malloc(size_t sz);
void  free(void *p);
void *calloc(size_t n, size_t sz);
void *realloc(void *p, size_t sz);

/* Capture the caller LR AND the next-level-up LR (often operator new
 * inlines/forwards directly to malloc, making (0) point at operator new
 * itself; (1) is then the actual app caller). __builtin_return_address(1)
 * may be unreliable without frame pointers but is best-effort here.
 * The stack dump in jupiter_dlmalloc_dump_last_caller() is the fallback. */
static void * volatile g_last_alloc_caller2  = (void *)0;

void *malloc(size_t sz) {
    g_last_alloc_caller  = __builtin_return_address(0);
    g_last_alloc_caller2 = __builtin_return_address(1);
    g_last_alloc_op = 1;
    g_last_alloc_size = sz;
    g_last_alloc_ptr = (void *)0;
    return dlmalloc(sz);
}
/* ---- Quarantine ring ---------------------------------------------------
 * Frees go through this ring buffer first. Only when the ring fills do
 * the oldest entries get released back to dlmalloc. Effect: a recently-
 * freed pointer stays in the heap untouched for ~N free-cycles instead
 * of being immediately recycled. Buffer overflows still trip dlmalloc's
 * FOOTERS at the next malloc/free that touches the corrupted chunk —
 * but use-after-free patterns where stale-pointer reads still see the
 * old contents now succeed (turning hard hangs into "still works"),
 * which lets us distinguish overflows (real corruption: instant abort)
 * from UAFs (race-sensitive: works with quarantine, hangs without). */
#define WAR1_QUARANTINE_SLOTS 1024
static void * volatile g_quarantine[WAR1_QUARANTINE_SLOTS];
static unsigned        g_quarantine_head = 0;

void free(void *p) {
    g_last_alloc_caller  = __builtin_return_address(0);
    g_last_alloc_caller2 = __builtin_return_address(1);
    g_last_alloc_op = 2;
    g_last_alloc_size = 0;
    g_last_alloc_ptr = p;
    if (!p) return;
    /* Push p onto quarantine; release the oldest slot it overwrites. */
    void *evict = g_quarantine[g_quarantine_head];
    g_quarantine[g_quarantine_head] = p;
    g_quarantine_head = (g_quarantine_head + 1) % WAR1_QUARANTINE_SLOTS;
    if (evict) {
        dlfree(evict);
    }
}
void *calloc(size_t n, size_t sz) {
    g_last_alloc_caller  = __builtin_return_address(0);
    g_last_alloc_caller2 = __builtin_return_address(1);
    g_last_alloc_op = 4;
    g_last_alloc_size = n * sz;
    g_last_alloc_ptr = (void *)0;
    return dlcalloc(n, sz);
}
void *realloc(void *p, size_t sz) {
    g_last_alloc_caller  = __builtin_return_address(0);
    g_last_alloc_caller2 = __builtin_return_address(1);
    g_last_alloc_op = 3;
    g_last_alloc_size = sz;
    g_last_alloc_ptr = p;
    return dlrealloc(p, sz);
}

/* Hook called by jupiter_dlmalloc_abort_with_lr — dumps the most recent
 * allocator-entry call info AND a stack scan for return addresses so
 * the actual upstream caller can be identified even when operator new
 * inlines past __builtin_return_address. */
void jupiter_dlmalloc_dump_last_caller(void);
void jupiter_dlmalloc_dump_last_caller(void)
{
    static const char *opnames[5] = { "?", "malloc", "free", "realloc", "calloc" };
    uart_puts("  last alloc op = ");
    uart_puts((g_last_alloc_op < 5) ? opnames[g_last_alloc_op] : "?");
    uart_puts("  size = ");
    uart_putdec((unsigned)g_last_alloc_size);
    uart_puts("  ptr = 0x");
    uart_puthex((unsigned)(uintptr_t)g_last_alloc_ptr);
    uart_puts("\n  caller LR(0) = 0x");
    uart_puthex((unsigned)(uintptr_t)g_last_alloc_caller);
    uart_puts("\n  caller LR(1) = 0x");
    uart_puthex((unsigned)(uintptr_t)g_last_alloc_caller2);
    uart_puts("\n");
    /* Best-effort stack scan: read the current SP and walk up looking
     * for words that look like .text return addresses (0x41000000 -
     * 0x411E0000 range, thumb bit set i.e. odd). Print the first few
     * candidates so we can hand-identify the call chain. */
    register uint32_t sp __asm__("sp");
    uint32_t sp_now = sp;
    uart_puts("  stack scan (SP=0x");
    uart_puthex(sp_now);
    uart_puts("):\n");
    int found = 0;
    for (uint32_t off = 0; off < 0x800 && found < 16; off += 4) {
        uint32_t w = *(volatile uint32_t *)(sp_now + off);
        if (w >= 0x41000000u && w < 0x411E8000u) {
            uart_puts("    +0x");
            uart_puthex(off);
            uart_puts(" = 0x");
            uart_puthex(w);
            uart_puts("\n");
            found++;
        }
    }
}
