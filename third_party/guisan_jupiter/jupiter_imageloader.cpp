/* Jupiter ImageLoader backend — see header. */
#include "jupiter_imageloader.hpp"
#include "jupiter_image.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdint>

/* Forward decls — defined in war1_bridge.cpp. */
extern "C" int war1_asset_exists(const char *path);

/* The pc8 catalog struct + lookup live in war1_asset_catalog.h /
 * war1_bridge.cpp. We need the blob bytes for any path. Expose a minimal
 * accessor we can call from here without dragging the bridge headers. */
extern "C" const unsigned char *war1_asset_blob(const char *path);

namespace gcn {

/* pc8 v2 header layout (mirrors examples/war1/war1_pc8.h, kept here so
 * we don't drag the bridge include into this file). */
struct PC8Header {
    uint32_t magic;
    uint16_t image_w, image_h;
    uint16_t version, reserved;
};

Image *JupiterImageLoader::load(const std::string &filename, bool /*convertToDisplayFormat*/) {
    const unsigned char *blob = war1_asset_blob(filename.c_str());
    if (!blob) return nullptr;
    const PC8Header *h = (const PC8Header *)blob;
    if (h->magic != 0x20384350u || h->version != 2) return nullptr;
    int w = h->image_w;
    int H = h->image_h;
    if (w <= 0 || H <= 0) return nullptr;
    const uint8_t  *pixels  = blob + sizeof(PC8Header);
    const uint32_t *palette = (const uint32_t *)(pixels + (uint32_t)w * H);
    uint32_t *out = (uint32_t *)std::malloc((size_t)w * H * 4);
    if (!out) return nullptr;
    for (int i = 0; i < w * H; i++) {
        out[i] = palette[pixels[i]];
    }
    return new JupiterImage(w, H, out, /*ownsPixels=*/true);
}

} /* namespace gcn */
