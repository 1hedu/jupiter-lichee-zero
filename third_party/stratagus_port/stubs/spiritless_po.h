/* Jupiter port: minimal spiritless_po stub.
 *
 * Upstream stratagus/translate.cpp wraps spiritless_po's Catalog to provide
 * gettext-style translations from .po files. On bare-metal we don't ship
 * translations; the Translate()/Plural() helpers just return the input.
 * This stub provides just the Catalog surface translate.cpp calls. */
#ifndef JUPITER_SPIRITLESS_PO_STUB_H
#define JUPITER_SPIRITLESS_PO_STUB_H

#include <string>
#include <istream>

namespace spiritless_po {

class Catalog {
    std::string empty_;
public:
    const std::string &gettext(const std::string &str) const { return str; }
    const std::string &ngettext(const std::string &singular,
                                 const std::string &plural, unsigned long n) const {
        return n == 1 ? singular : plural;
    }
    bool Add(std::istream &) { return true; }
    void Clear() {}
};

} // namespace spiritless_po

#endif
