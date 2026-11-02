#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int pine64_test_snprintf(char *, size_t, const char *, ...);
int main(void)
{
    char out[64];
    assert(pine64_test_snprintf(out, sizeof(out), "%*d|%.*s|%s", 4, 7, 2, "ABCDE", "tail") == 12);
    assert(!strcmp(out, "   7|AB|tail"));
    char *raw = malloc(4);
    assert(raw);
    memcpy(raw, "ABCD", 4); /* Explicit precision: no NUL or readable fifth byte. */
    assert(pine64_test_snprintf(out, sizeof(out), "%.*s:%d", 4, raw, 99) == 7);
    assert(!strcmp(out, "ABCD:99"));
    assert(pine64_test_snprintf(out, sizeof(out), "%.*s:%u", 0, raw, 8u) == 2);
    assert(!strcmp(out, ":8"));
    assert(pine64_test_snprintf(out, 3, "%5d", 7) == 5 && !strcmp(out, "  "));
    assert(pine64_test_snprintf(NULL, 0, "%*d %.*s", 4, 42, 2, raw) == 7);
    free(raw);
    puts("libc formatter PASS: width/precision varargs, bounded non-NUL strings, truncation and size queries");
    return 0;
}
