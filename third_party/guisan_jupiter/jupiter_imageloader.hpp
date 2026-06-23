/* Jupiter backend for guisan: ImageLoader.
 *
 * Resolves a filename through the WC1 embedded asset catalog (the same
 * one CGraphic uses), decodes the pc8 blob (8-bit indexed + 256-entry
 * ARGB palette) into a flat ARGB buffer, and wraps it in a JupiterImage.
 * Returns nullptr when the path isn't in the catalog. */
#ifndef JUPITER_IMAGELOADER_HPP
#define JUPITER_IMAGELOADER_HPP

#include <guisan/imageloader.hpp>
#include <string>

namespace gcn {

class JupiterImageLoader : public ImageLoader {
public:
    Image *load(const std::string &filename, bool convertToDisplayFormat = true) override;
};

} /* namespace gcn */

#endif /* JUPITER_IMAGELOADER_HPP */
