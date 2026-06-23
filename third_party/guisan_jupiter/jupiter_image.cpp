/* Jupiter Image backend — see header. */
#include "jupiter_image.hpp"
#include <cstdlib>
#include <cstring>

namespace gcn {

JupiterImage::JupiterImage(int width, int height)
    : mWidth(width), mHeight(height), mOwnsPixels(true) {
    if (width > 0 && height > 0) {
        mPixels = (uint32_t *)std::malloc(sizeof(uint32_t) * width * height);
        if (mPixels) std::memset(mPixels, 0, sizeof(uint32_t) * width * height);
    }
}

JupiterImage::JupiterImage(int width, int height, uint32_t *pixels, bool ownsPixels)
    : mWidth(width), mHeight(height), mPixels(pixels), mOwnsPixels(ownsPixels) {}

JupiterImage::~JupiterImage() {
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
