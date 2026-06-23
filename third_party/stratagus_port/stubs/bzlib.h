/* bzlib stub — same story as zlib. */
#ifndef JUPITER_BZLIB_STUB_H
#define JUPITER_BZLIB_STUB_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void *BZFILE;
BZFILE *BZ2_bzopen(const char *path, const char *mode);
int     BZ2_bzclose(BZFILE *b);
int     BZ2_bzread(BZFILE *b, void *buf, int len);
int     BZ2_bzwrite(BZFILE *b, const void *buf, int len);
int     BZ2_bzflush(BZFILE *b);
#ifdef __cplusplus
}
#endif
#endif
