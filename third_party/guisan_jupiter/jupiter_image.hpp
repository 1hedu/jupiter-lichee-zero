/* Jupiter backend for guisan: Image implementation.
 *
 * gcn::Image is the abstract pixel buffer guisan uses for icons, button
 * faces, font glyph strips, etc. The SDL backend wraps SDL_Surface; ours
 * wraps a plain ARGB8888 row-major buffer (heap-allocated or pointing
 * into an asset blob).
 *
 * Construction paths:
 *   - JupiterImage(width, height): allocate w*h ARGB pixels (zeroed).
 *   - JupiterImage(width, height, pixels, ownsPixels): wrap an existing
 *     buffer (e.g., decoded pc8 palette-applied data). If ownsPixels
 *     is false, we don't free in dtor (asset stays in flash/blob).
 */
#ifndef JUPITER_IMAGE_HPP
#define JUPITER_IMAGE_HPP

#include <guisan/image.hpp>
#include <guisan/color.hpp>
#include <cstdint>

namespace gcn {

class JupiterImage : public Image {
public:
    /* Allocate a fresh w×h ARGB buffer (zero-cleared). */
    JupiterImage(int width, int height);
    /* Wrap an existing buffer. If ownsPixels is true, free() releases it. */
    JupiterImage(int width, int height, uint32_t *pixels, bool ownsPixels);
    ~JupiterImage() override;

    /* gcn::Image interface. */
    void free() override;
    int getWidth() const override;
    int getHeight() const override;
    Color getPixel(int x, int y) override;
    void putPixel(int x, int y, const Color &color) override;
    void convertToDisplayFormat() override;

    /* Backend-specific accessors used by JupiterGraphics::drawImage. */
    const uint32_t *getPixels() const { return mPixels; }
    uint32_t       *getPixels()       { return mPixels; }

    /* For pc8-backed images: remember which CGraphic the cached ARGB came
     * from, plus the palette epoch at bake time. Lets JupiterGraphics::
     * drawImage detect a stale cache after CGraphic::SetPaletteColor and
     * rebuild from the live pc8 palette — same live-palette behavior the
     * title screen has via war1_blit_via_pc8_scaled, without per-frame
     * cost when the palette hasn't moved. */
    void setPC8Source(const void *g, uint32_t epoch) { mPC8Source = g; mPC8Epoch = epoch; }
    const void *getPC8Source() const { return mPC8Source; }
    uint32_t    getPC8Epoch()  const { return mPC8Epoch; }
    void        setPC8Epoch(uint32_t e) { mPC8Epoch = e; }

private:
    int        mWidth = 0;
    int        mHeight = 0;
    uint32_t  *mPixels = nullptr;
    bool       mOwnsPixels = false;
    const void *mPC8Source = nullptr;
    uint32_t   mPC8Epoch = 0;
};

} /* namespace gcn */

#endif /* JUPITER_IMAGE_HPP */
