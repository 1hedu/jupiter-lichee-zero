/*
 * mt32_math_shim.c
 * Runtime-switchable math implementations for munt.
 * Paths:
 *   0 = C-poly   — math_neon pure-C polynomial (_c variants)
 *   1 = NEON-asm — math_neon NEON asm stubs (has known return-value bug)
 *   2 = libm     — true-precision sinf/powf/expf/logf/log10f from libm
 * Switched live via `g_math_path` from main.c.
 */
#include <math.h>

extern float sinf_c(float);
extern float sinf_neon_hfp(float);
extern float powf_c(float, float);
extern float powf_neon_hfp(float, float);
extern float expf_c(float);
extern float expf_neon_hfp(float);
extern float logf_c(float);
extern float logf_neon_hfp(float);
extern float log10f_c(float);
extern float log10f_neon_hfp(float);

int g_math_path = 0;  /* 0=C, 1=ASM, 2=libm */

float mt32_sinf_override(float x) {
    if (g_math_path == 2) return sinf(x);
    if (g_math_path == 1) return sinf_neon_hfp(x);
    return sinf_c(x);
}
float mt32_powf_override(float x, float y) {
    if (g_math_path == 2) return powf(x, y);
    if (g_math_path == 1) return powf_neon_hfp(x, y);
    return powf_c(x, y);
}
float mt32_expf_override(float x) {
    if (g_math_path == 2) return expf(x);
    if (g_math_path == 1) return expf_neon_hfp(x);
    return expf_c(x);
}
float mt32_logf_override(float x) {
    if (g_math_path == 2) return logf(x);
    if (g_math_path == 1) return logf_neon_hfp(x);
    return logf_c(x);
}
float mt32_log10f_override(float x) {
    if (g_math_path == 2) return log10f(x);
    if (g_math_path == 1) return log10f_neon_hfp(x);
    return log10f_c(x);
}
