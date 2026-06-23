/* libc_shim.c — bare-metal shims for libvgm and mt32emu (Munt) */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* uart_puts is exported from lib/uart.c; declared here to avoid pulling
 * jupiter.h (which includes v3s.h with GCC extensions we can't easily
 * enable in this shim). */
extern void uart_puts(const char *s);
extern void uart_putdec(uint32_t v);
extern void uart_puthex(uint32_t v);

/* malloc / free / calloc / realloc are provided by dlmalloc 2.8.6 in
 * third_party/dlmalloc/dlmalloc_jupiter.c. The previous bump-and-leak
 * lived here; it silently masked use-after-free bugs by never recycling
 * addresses, which let the port appear to work while real lifetime bugs
 * accumulated. dlmalloc + the quarantine ring exposes those at the
 * source so each one can be ported correctly. */

static uint32_t _rand_state = 12345;
__attribute__((weak)) int rand(void) {
    _rand_state = _rand_state * 1103515245 + 12345;
    return (_rand_state >> 16) & 0x7FFF;
}

/* memset and memcpy now provided by lib/arm_opt/ (ARM optimized-routines) */

int abs(int x) { return x < 0 ? -x : x; }

/* div_t / div — mt32emu's Display uses this for formatting */
typedef struct { int quot; int rem; } div_t;
div_t div(int num, int den) {
    div_t r;
    r.quot = num / den;
    r.rem  = num % den;
    return r;
}

static int _errno_val;
int *__errno(void) { return &_errno_val; }


/* heap_reset / heap_rewind / heap_used were tied to the old bump
 * pointer. dlmalloc owns its own state now; nothing in WC1 calls these.
 * MT-32/SC-55 builds were the only consumers and both reboot before
 * reuse, so they don't need rewind anymore. The vgm_player + mt32_rt
 * examples still reference them; provide no-op stubs so menu/vgm/mt32
 * builds still link. With dlmalloc-CANNOT_TRIM set we don't actually
 * give memory back, but those examples reboot the device on track
 * change so steady-state leak doesn't matter. */
extern unsigned long libc_shim_heap_used(void);
unsigned long heap_used(void) { return libc_shim_heap_used(); }
void heap_reset(void) { /* no-op — dlmalloc keeps its arenas */ }
void heap_rewind(unsigned long /*snapshot*/) { /* no-op */ }
unsigned long heap_checkpoint(void) { return 0; }

/* strlen and strcmp now provided by lib/arm_opt/ (ARM optimized-routines) */

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static int _lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int strncasecmp(const char *a, const char *b, size_t n) {
    while (n && *a && _lc(*a) == _lc(*b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return _lc((unsigned char)*a) - _lc((unsigned char)*b);
}
int strcasecmp(const char *a, const char *b) {
    while (*a && _lc(*a) == _lc(*b)) { a++; b++; }
    return _lc((unsigned char)*a) - _lc((unsigned char)*b);
}

extern void *malloc(size_t);
char *strdup(const char *s) {
    if (!s) return NULL;
    size_t n = 0; while (s[n]) n++;
    char *d = (char *)malloc(n + 1);
    if (d) { for (size_t i = 0; i <= n; i++) d[i] = s[i]; }
    return d;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : (char *)0;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = a, *pb = b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

/* AEABI helpers — libm's sinf/powf/expf/logf call these internally.
 * libgcc has them but sometimes link order prevents resolution. Providing
 * them here guarantees libm works in our bare-metal build. */

/* Signed 64-bit → double via manual IEEE 754 construction */
double __aeabi_l2d(long long x) {
    if (x == 0) {
        union { uint64_t u; double d; } r; r.u = 0; return r.d;
    }
    int sign = 0;
    unsigned long long ux;
    if (x < 0) { sign = 1; ux = (unsigned long long)(-x); }
    else       { ux = (unsigned long long)x; }
    /* Find leading bit */
    int bits = 63;
    while (!(ux & (1ULL << 63))) { ux <<= 1; bits--; }
    /* Discard implicit leading 1, shift to 52-bit mantissa */
    unsigned long long mantissa = (ux >> (63 - 52)) & ((1ULL << 52) - 1);
    unsigned long long exponent = (unsigned long long)(bits + 1023);
    union { uint64_t u; double d; } r;
    r.u = ((unsigned long long)sign << 63) | (exponent << 52) | mantissa;
    return r.d;
}

/* Double → unsigned 64-bit (truncate toward zero). Replaces libgcc's
 * __aeabi_d2ulz / __fixunsdfdi: the libgcc version on our toolchain
 * round-trips inconsistently with our __aeabi_ul2d below — for d=62.0
 * (bits 0x404f000000000000) it returned a value where the back-cast
 * (double)result != 62.0, which made emit_double's
 * "is integer-valued" guard fall through into the buggy fractional
 * loop and emit "62.//////" instead of "62". Direct bit extraction
 * matches __aeabi_ul2d's exact mantissa shifts and round-trips
 * cleanly. */
unsigned long long __aeabi_d2ulz(double d) {
    union { double d; uint64_t u; } un; un.d = d;
    uint64_t bits = un.u;
    int sign = (int)((bits >> 63) & 1);
    int exp  = (int)((bits >> 52) & 0x7FF) - 1023;
    uint64_t mant = (bits & 0xFFFFFFFFFFFFFULL) | 0x10000000000000ULL;
    if (sign) return 0;            /* negative truncates to 0 */
    if (exp < 0) return 0;         /* |d| < 1 */
    if (exp >= 64) return 0xFFFFFFFFFFFFFFFFULL;
    if (exp >= 52) return mant << (exp - 52);
    return mant >> (52 - exp);
}

long long __aeabi_d2lz(double d) {
    union { double d; uint64_t u; } un; un.d = d;
    uint64_t bits = un.u;
    int sign = (int)((bits >> 63) & 1);
    int exp  = (int)((bits >> 52) & 0x7FF) - 1023;
    uint64_t mant = (bits & 0xFFFFFFFFFFFFFULL) | 0x10000000000000ULL;
    if (exp < 0) return 0;
    if (exp >= 63) return sign ? (long long)0x8000000000000000ULL
                                : (long long)0x7FFFFFFFFFFFFFFFULL;
    uint64_t r = (exp >= 52) ? (mant << (exp - 52)) : (mant >> (52 - exp));
    return sign ? -(long long)r : (long long)r;
}

/* Unsigned 64-bit → double */
double __aeabi_ul2d(unsigned long long x) {
    if (x == 0) {
        union { uint64_t u; double d; } r; r.u = 0; return r.d;
    }
    int bits = 63;
    while (!(x & (1ULL << 63))) { x <<= 1; bits--; }
    unsigned long long mantissa = (x >> (63 - 52)) & ((1ULL << 52) - 1);
    unsigned long long exponent = (unsigned long long)(bits + 1023);
    union { uint64_t u; double d; } r;
    r.u = (exponent << 52) | mantissa;
    return r.d;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

/* ---- C++ static destructor glue ----
 * Called by GCC with -fno-use-cxa-atexit to register destructors. We
 * never shut down cleanly (the game runs forever), so destructors never
 * fire. The function pointer is silently dropped. */
int atexit(void (*fn)(void)) { (void)fn; return 0; }

/* Pulled in by libgcc unwind support (via ARM optimized-routines
 * .fnstart/.fnend directives). Never actually called. */
void abort(void) { while (1); }

/* Ne10 assert stubs — power-of-2 FFT path never triggers these */
void __assert_func(const char *f, int l, const char *fn, const char *e)
{
    (void)f; (void)l; (void)fn; (void)e;
    uart_puts("ASSERT FAIL\n");
    while (1);
}

/* Ne10 generic butterfly stubs — only needed for non-power-of-2 FFT.
 * We only use 256/512-point power-of-2, so these are never called. */
void ne10_mixed_radix_generic_butterfly_float32_neon(void *a, void *b,
    void *c, void *d, void *e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
void ne10_mixed_radix_generic_butterfly_inverse_float32_neon(void *a, void *b,
    void *c, void *d, void *e) { (void)a;(void)b;(void)c;(void)d;(void)e; }

/* Ne10 int32 FFT — we only use float32, stub the int32 alloc */
void *ne10_fft_alloc_c2c_int32_c(int n) { (void)n; return (void*)0; }

/* ---- printf family — silent stubs ----
 * Both Munt and Nuked-SC55 sprinkle printf() debug calls all over their
 * hot paths. On a slow UART (115200 baud) any per-instruction debug
 * spam will starve real-time emulation. We make every printf-family
 * call a complete no-op. If you need to see what mt32emu / Nuked-SC55
 * are saying for debugging, swap one of these for a real implementation
 * temporarily. */
int putchar(int c) { (void)c; return 0; }
int puts(const char *s) { (void)s; return 0; }

int printf(const char *fmt, ...) {
    (void)fmt;
    return 0;
}

int vprintf(const char *fmt, va_list ap) {
    (void)fmt; (void)ap;
    return 0;
}

int fprintf(void *stream, const char *fmt, ...) {
    (void)stream; (void)fmt;
    return 0;
}

int vfprintf(void *stream, const char *fmt, va_list ap) {
    (void)stream; (void)fmt; (void)ap;
    return 0;
}

int fputs(const char *s, void *stream) {
    (void)s; (void)stream;
    return 0;
}

int fputc(int c, void *stream) {
    (void)c; (void)stream;
    return 0;
}

__attribute__((weak)) int fflush(void *stream) {
    (void)stream;
    return 0;
}

/* Minimal printf family.
 *
 * The default stubs above are no-ops because most libvgm / mt32emu / Nuked-SC55
 * debug prints would kill real-time at 115200 baud. But Lua uses sprintf for
 * number→string conversion (`lua_number2str` → sprintf %d / %.14g) — when
 * those are no-ops, Lua scripts concatenating numbers produce empty strings,
 * which silently breaks data-driven scripts (e.g. War1gus anim.lua generating
 * "if-var v.Speed.Value == 5 speed_5" ends up as "… == speed_" with no number).
 *
 * Handle the format specifiers Lua actually uses: %d, %u, %x, %s, %c, %g, %f.
 * Not a full implementation — keep it tight. Returns #chars written. */
static int emit(char *dst, int cap, int pos, char c) {
    if (dst && pos + 1 < cap) dst[pos] = c;
    return pos + 1;
}
static int emit_str(char *dst, int cap, int pos, const char *s) {
    while (s && *s) pos = emit(dst, cap, pos, *s++);
    return pos;
}
static int emit_udec(char *dst, int cap, int pos, unsigned long long v) {
    char tmp[24]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    while (n--) pos = emit(dst, cap, pos, tmp[n]);
    return pos;
}
static int emit_hex(char *dst, int cap, int pos, unsigned long long v, int upper) {
    char tmp[24]; int n = 0;
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = d[v & 0xF]; v >>= 4; }
    while (n--) pos = emit(dst, cap, pos, tmp[n]);
    return pos;
}
static int emit_double(char *dst, int cap, int pos, double d) {
    /* Simple %g-ish: integer part + up to 6 frac digits. Good enough for Lua
     * number-to-string on integer-valued doubles (the common case). */
    if (d != d) return emit_str(dst, cap, pos, "nan");
    if (d < 0) { pos = emit(dst, cap, pos, '-'); d = -d; }
    unsigned long long int_part = (unsigned long long)d;
    pos = emit_udec(dst, cap, pos, int_part);
    double back = (double)int_part;
    if (back == d) return pos;       /* integer-valued — skip fractional */
    double frac = d - back;
    if (frac > 1e-15) {
        pos = emit(dst, cap, pos, '.');
        for (int i = 0; i < 6 && frac > 1e-15; i++) {
            frac *= 10;
            int digit = (int)frac;
            pos = emit(dst, cap, pos, '0' + digit);
            frac -= digit;
        }
    }
    return pos;
}
int vsnprintf(char *buf, size_t sz, const char *fmt, va_list ap) {
    int cap = (int)sz, pos = 0;
    while (*fmt) {
        if (*fmt != '%') { pos = emit(buf, cap, pos, *fmt++); continue; }
        fmt++;
        /* Skip flags/width/precision — we don't honor them, just consume. */
        while (*fmt && (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#' || *fmt == '0')) fmt++;
        while (*fmt >= '0' && *fmt <= '9') fmt++;
        if (*fmt == '.') { fmt++; while (*fmt >= '0' && *fmt <= '9') fmt++; }
        /* Length modifiers */
        int is_long = 0, is_ll = 0;
        while (*fmt == 'l') { if (is_long) is_ll = 1; is_long = 1; fmt++; }
        if (*fmt == 'z' || *fmt == 'j' || *fmt == 't') { is_long = 1; fmt++; }
        switch (*fmt) {
            case 'd': case 'i': {
                long long v = is_ll ? va_arg(ap, long long) : (is_long ? (long long)va_arg(ap, long) : (long long)va_arg(ap, int));
                if (v < 0) { pos = emit(buf, cap, pos, '-'); v = -v; }
                pos = emit_udec(buf, cap, pos, (unsigned long long)v);
                break;
            }
            case 'u': {
                unsigned long long v = is_ll ? va_arg(ap, unsigned long long) : (is_long ? (unsigned long long)va_arg(ap, unsigned long) : (unsigned long long)va_arg(ap, unsigned int));
                pos = emit_udec(buf, cap, pos, v);
                break;
            }
            case 'x': case 'X': case 'p': {
                unsigned long long v = (*fmt == 'p')
                    ? (unsigned long long)(uintptr_t)va_arg(ap, void *)
                    : (is_ll ? va_arg(ap, unsigned long long) : (is_long ? (unsigned long long)va_arg(ap, unsigned long) : (unsigned long long)va_arg(ap, unsigned int)));
                pos = emit_hex(buf, cap, pos, v, *fmt == 'X');
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) { pos = emit_str(buf, cap, pos, "(null)"); break; }
                /* No length cap — Stratagus's Format() uses
                 * printf("%s", bigstring.c_str()) for multi-MB
                 * SaveGlobal output. emit() bounds writes by `cap`
                 * naturally; we just need to walk the string until
                 * its NUL terminator. (The earlier 4 KB cap was a
                 * defensive guard against pointers into unmapped
                 * memory; in practice every caller's string is a
                 * std::string or a literal, both well-terminated.) */
                while (*s) pos = emit(buf, cap, pos, *s++);
                break;
            }
            case 'c': pos = emit(buf, cap, pos, (char)va_arg(ap, int)); break;
            case 'f': case 'g': case 'e': case 'G': case 'E': {
                double v = va_arg(ap, double);
                pos = emit_double(buf, cap, pos, v);
                break;
            }
            case '%': pos = emit(buf, cap, pos, '%'); break;
            default: pos = emit(buf, cap, pos, '%'); pos = emit(buf, cap, pos, *fmt); break;
        }
        fmt++;
    }
    if (buf && cap > 0) buf[(pos < cap) ? pos : (cap - 1)] = 0;
    return pos;
}
int snprintf(char *buf, size_t sz, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, sz, fmt, ap);
    va_end(ap);
    return r;
}
int sprintf(char *buf, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, 0x7FFFFFFF, fmt, ap);
    va_end(ap);
    return r;
}

/* ctype helpers — guisan focushandler/widget code uses these for hotkey
 * matching, and printf/scanf wouldn't be the right shim either. Match
 * the C standard semantics. */
__attribute__((weak)) int isascii(int c) { return (unsigned)c < 128; }
__attribute__((weak)) int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
__attribute__((weak)) int isdigit(int c) { return c >= '0' && c <= '9'; }
__attribute__((weak)) int isalnum(int c) { return isalpha(c) || isdigit(c); }
__attribute__((weak)) int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
__attribute__((weak)) int isupper(int c) { return c >= 'A' && c <= 'Z'; }
__attribute__((weak)) int islower(int c) { return c >= 'a' && c <= 'z'; }
__attribute__((weak)) int toupper(int c) { return islower(c) ? c - 32 : c; }
__attribute__((weak)) int tolower(int c) { return isupper(c) ? c + 32 : c; }
__attribute__((weak)) int isprint(int c) { return c >= 32 && c < 127; }
__attribute__((weak)) int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
__attribute__((weak)) int ispunct(int c) { return isprint(c) && !isalnum(c) && c != ' '; }
__attribute__((weak)) int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

/* localeconv: Lua's float-to-string path calls lua_getlocaledecpoint()
 * which is `localeconv()->decimal_point[0]`. arm-none-eabi-newlib's
 * localeconv stub returns a struct with uninitialised fields under our
 * minimal startup, and Lua then appends a garbage byte to every float
 * (e.g. "40." followed by 0xC0). Returning a properly-initialised
 * "C"-locale struct fixes that. */
struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};
static char _lc_dot[]   = ".";
static char _lc_empty[] = "";
static struct lconv _c_locale = {
    _lc_dot,   /* decimal_point */
    _lc_empty, /* thousands_sep */
    _lc_empty, /* grouping */
    _lc_empty, /* int_curr_symbol */
    _lc_empty, /* currency_symbol */
    _lc_dot,   /* mon_decimal_point */
    _lc_empty, /* mon_thousands_sep */
    _lc_empty, /* mon_grouping */
    _lc_empty, /* positive_sign */
    _lc_empty, /* negative_sign */
    127, 127, 127, 127, 127, 127, 127, 127
};
/* --wrap=localeconv (set in Makefile) routes every call to ours. The
 * weak attribute alone wasn't enough — newlib's strong def in libc.a
 * still won when something else's chain pulled it in. */
struct lconv *__wrap_localeconv(void) { return &_c_locale; }

/* SN76489 panning stub — centre pan */
#define PANNING_NORMAL 0x4000
void Panning_Centre(int *channels) { channels[0] = channels[1] = PANNING_NORMAL; }
void Panning_Calculate(int *channels, short position) {
    (void)position;
    channels[0] = channels[1] = PANNING_NORMAL;
}
