/* Bare-metal stub of upstream utils/files.h.
 * mcu.cpp uses Files::utf8_fopen, Files::dirname etc. for ROM loading
 * via disk paths. We provide our own ROM loader (memory-backed), so
 * these are no-op stubs returning failure values. */
#ifndef JUPITER_FILES_STUB_H
#define JUPITER_FILES_STUB_H

#include <stdio.h>
#include <string>

namespace Files
{
    inline FILE *utf8_fopen(const char *p, const char *m) { (void)p; (void)m; return nullptr; }
    enum Charsets { CHARSET_UTF8, CHARSET_UTF16BE, CHARSET_UTF16LE, CHARSET_UTF32BE, CHARSET_UTF32LE };
    inline int  skipBom(FILE *f, const char **c = nullptr) { (void)f; (void)c; return 0; }
    inline bool fileExists(const std::string &p) { (void)p; return false; }
    inline bool dirExists(const std::string &p)  { (void)p; return false; }
    inline bool deleteFile(const std::string &p) { (void)p; return false; }
    inline bool copyFile(const std::string &t, const std::string &f, bool o = false) { (void)t;(void)f;(void)o; return false; }
    inline bool moveFile(const std::string &t, const std::string &f, bool o = false) { (void)t;(void)f;(void)o; return false; }
    inline bool isAbsolute(const std::string &p) { (void)p; return false; }
    inline std::string basename(std::string p)        { return p; }
    inline std::string basenameNoSuffix(std::string p){ return p; }
    inline std::string dirname(std::string p)         { return p; }
    inline std::string real_dirname(std::string p)    { return p; }
    inline std::string changeSuffix(std::string p, const std::string &s) { (void)s; return p; }
    inline bool hasSuffix(const std::string &p, const std::string &s) { (void)p;(void)s; return false; }
    inline void getGifMask(std::string &m, const std::string &f) { (void)m;(void)f; }
    inline bool dumpFile(const std::string &i, std::string &o) { (void)i;(void)o; return false; }
}

#endif
