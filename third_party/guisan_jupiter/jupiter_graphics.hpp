/* Jupiter backend for guisan: Graphics implementation.
 *
 * guisan's gcn::Graphics is the abstract drawing surface. Stratagus uses
 * it to render the in-game menu, dialogs, save/load screens, etc. via
 * the widget tree. The SDL backend (sdl/sdl2graphics.cpp) targets an
 * SDL_Renderer; we target the same back framebuffer (g_war1_back_fb)
 * that the rest of the bare-metal port writes to, so guisan widgets
 * composite naturally on top of the map / HUD / fog.
 *
 * Coordinate system: (0,0) top-left, ARGB8888, 480x272.
 */
#ifndef JUPITER_GRAPHICS_HPP
#define JUPITER_GRAPHICS_HPP

#include <guisan/graphics.hpp>
#include <guisan/color.hpp>
#include <guisan/cliprectangle.hpp>

namespace gcn {

class JupiterGraphics : public Graphics {
public:
    using Graphics::drawImage; /* keep base overloads visible */

    JupiterGraphics();
    ~JupiterGraphics() override = default;

    /* Lifecycle. _beginDraw pushes a full-screen clip so widgets can
     * use draw* without having to manually push a clip first. */
    void _beginDraw() override;
    void _endDraw() override;

    /* Pure virtuals from gcn::Graphics. */
    void drawPoint(int x, int y) override;
    void drawLine(int x1, int y1, int x2, int y2) override;
    void drawRectangle(const Rectangle &rect) override;
    void fillRectangle(const Rectangle &rect) override;
    void setColor(const Color &color) override;
    const Color &getColor() override;

    /* Image drawing (sub-rectangle copy). */
    void drawImage(const Image *image, int srcX, int srcY,
                   int dstX, int dstY, int width, int height) override;

private:
    Color mColor{255, 255, 255, 255};
};

} /* namespace gcn */

#endif /* JUPITER_GRAPHICS_HPP */
