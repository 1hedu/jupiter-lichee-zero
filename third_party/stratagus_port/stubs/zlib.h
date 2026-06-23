/* zlib stub — iolib.cpp uses gzFile/gzopen/etc for save compression. We
 * don't support save games in v1; provide opaque handle + no-op prototypes. */
#ifndef JUPITER_ZLIB_STUB_H
#define JUPITER_ZLIB_STUB_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void *gzFile;
gzFile gzopen(const char *path, const char *mode);
int    gzclose(gzFile file);
int    gzread(gzFile file, void *buf, unsigned int len);
int    gzwrite(gzFile file, const void *buf, unsigned int len);
int    gzflush(gzFile file, int flush);
#define Z_NO_FLUSH      0
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
int    gzeof(gzFile file);
long   gztell(gzFile file);
long   gzseek(gzFile file, long offset, int whence);
const char *gzerror(gzFile file, int *errnum);
int    gzputs(gzFile file, const char *s);
char  *gzgets(gzFile file, char *buf, int len);
int    gzgetc(gzFile file);
int    gzungetc(int c, gzFile file);
#ifdef __cplusplus
}
#endif
#endif
