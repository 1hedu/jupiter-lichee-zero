/*
 * Jupiter SDK — minimal C++ runtime for bare-metal
 *
 * Enough glue to let C++ code (mt32emu) link and run without a hosted
 * OS. Routes new/delete through the existing libc_shim heap, provides
 * the ABI guard symbols GCC needs, and exposes a tiny init_array walker
 * so static constructors get called exactly once at startup.
 *
 * Compiled with -fno-exceptions -fno-rtti -fno-threadsafe-statics, so
 * most of the usual C++ ABI surface (unwind, typeinfo, guard vars) is
 * already elided.
 */

#include <stddef.h>

extern "C" void *malloc(size_t);
extern "C" void free(void *);

/* ---- Heap-backed new/delete ---- */

void *operator new  (size_t sz) { return malloc(sz); }
void *operator new[](size_t sz) { return malloc(sz); }

void operator delete  (void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }

/* C++14 sized-delete overloads */
void operator delete  (void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }

/* ---- ABI glue ---- */

extern "C" {

/* Called if a pure virtual method slips through dispatch. Hard halt. */
void __cxa_pure_virtual(void)
{
    while (1) { __asm__ volatile ("wfi"); }
}

/* Referenced by __cxa_atexit(); we use -fno-use-cxa-atexit so this is
 * only ever taken as an address, never called. */
void *__dso_handle = 0;

/* ---- Static constructor runner ----
 * Iterate the .init_array section produced by the linker and call each
 * function in order. Call this exactly once from main() before touching
 * any C++ code that relies on global state. */
typedef void (*init_fn_t)(void);
extern init_fn_t __init_array_start[];
extern init_fn_t __init_array_end[];

void cpp_init(void)
{
    for (init_fn_t *f = __init_array_start; f < __init_array_end; f++)
        (*f)();
}

} /* extern "C" */
