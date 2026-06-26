/* Jupiter Image backend — see header. */
#include "jupiter_image.hpp"
#include <cstdlib>
#include <cstring>

/* C diagnostic — live JupiterImage count. The counter operates only
 * on already-constructed objects (post-ctor body, pre-dtor body), so
 * incrementing/decrementing it isn't on any allocation path. The
 * earlier version that placed the counter inside the ctor's
 * member-init region appears to have interacted badly with global
 * static-init order (boot died with std::bad_array_new_length very
 * early), so we expose it via post-ctor/dtor hook functions instead. */
static unsigned int g_live_jupiter_images = 0;
static unsigned long g_live_jupiter_image_bytes = 0;
extern "C" unsigned int war1_live_jupiter_images(void) { return g_live_jupiter_images; }
extern "C" unsigned long war1_live_jupiter_image_bytes(void) { return g_live_jupiter_image_bytes; }

namespace gcn {

JupiterImage::JupiterImage(int width, int height)
    : mWidth(width), mHeight(height), mOwnsPixels(true) {
    if (width > 0 && height > 0) {
        mPixels = (uint32_t *)std::malloc(sizeof(uint32_t) * width * height);
        if (mPixels) std::memset(mPixels, 0, sizeof(uint32_t) * width * height);
    }
    g_live_jupiter_images++;
    if (mPixels) g_live_jupiter_image_bytes += (unsigned long)width * height * 4;
}

JupiterImage::JupiterImage(int width, int height, uint32_t *pixels, bool ownsPixels)
    : mWidth(width), mHeight(height), mPixels(pixels), mOwnsPixels(ownsPixels) {
    g_live_jupiter_images++;
    if (mPixels && mOwnsPixels) g_live_jupiter_image_bytes += (unsigned long)width * height * 4;
}

JupiterImage::~JupiterImage() {
    if (mPixels && mOwnsPixels) g_live_jupiter_image_bytes -= (unsigned long)mWidth * mHeight * 4;
    g_live_jupiter_images--;
    free();
}

void JupiterImage::free() {
    if (mOwnsPixels && mPixels) {
        std::free(mPixels);
    }
    mPixels = nullptr;
    mWidth = 0;
    mHeight = 0;
    mOwnsPixels = false;
}

int JupiterImage::getWidth() const { return mWidth; }
int JupiterImage::getHeight() const { return mHeight; }

Color JupiterImage::getPixel(int x, int y) {
    if (!mPixels || x < 0 || x >= mWidth || y < 0 || y >= mHeight) {
        return Color(0, 0, 0, 0);
    }
    uint32_t p = mPixels[y * mWidth + x];
    return Color((p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF, (p >> 24) & 0xFF);
}

void JupiterImage::putPixel(int x, int y, const Color &color) {
    if (!mPixels || x < 0 || x >= mWidth || y < 0 || y >= mHeight) return;
    mPixels[y * mWidth + x] = ((uint32_t)color.a << 24)
                            | ((uint32_t)color.r << 16)
                            | ((uint32_t)color.g << 8)
                            |  (uint32_t)color.b;
}

/* No-op: we already store in display format (ARGB8888). */
void JupiterImage::convertToDisplayFormat() {}

} /* namespace gcn */
