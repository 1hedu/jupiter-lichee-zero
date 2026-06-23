/* Jupiter Graphics backend — see header for design notes. */
#include "jupiter_graphics.hpp"
#include "jupiter_image.hpp"
#include <guisan/exception.hpp>
#include <cstdint>

/* Back-fb target — same one the pc8 blits write to, so the menu
 * composites on top of the in-progress frame. Defined in war1_bridge.cpp. */
extern "C" volatile uint32_t g_war1_back_fb;

/* Live-palette helpers from war1_bridge.cpp; let drawImage detect a
 * CGraphic::SetPaletteColor since the JupiterImage's last bake and
 * re-expand its cached ARGB pixels from the current pc8 palette. */
extern "C" uint32_t war1_pc8_palette_epoch(const void *g);
extern "C" int      war1_pc8_rebake_argb(const void *g, uint32_t *dst, int outW, int outH);

namespace gcn {

constexpr int SCREEN_W = 480;
constexpr int SCREEN_H = 272;

JupiterGraphics::JupiterGraphics() = default;

void JupiterGraphics::_beginDraw() {
    /* Default full-screen clip. Widgets build on this with their own
     * pushClipArea calls; the base Graphics::pushClipArea handles
     * stack/intersection. */
    pushClipArea(Rectangle(0, 0, SCREEN_W, SCREEN_H));
}

void JupiterGraphics::_endDraw() {
    popClipArea();
}

static inline uint32_t pack_argb(const Color &c) {
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* Standard "over" blend. Source alpha controls overlay strength. */
static inline uint32_t blend_pixel(uint32_t dst, uint32_t src) {
    uint32_t a = (src >> 24) & 0xFF;
    if (a == 0)   return dst;
    if (a == 255) return src;
    uint32_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint32_t ia = 255 - a;
    uint32_t r = (sr * a + dr * ia) / 255;
    uint32_t g = (sg * a + dg * ia) / 255;
    uint32_t b = (sb * a + db * ia) / 255;
    return (0xFFu << 24) | (r << 16) | (g << 8) | b;
}

void JupiterGraphics::setColor(const Color &color) {
    mColor = color;
}

const Color &JupiterGraphics::getColor() {
    return mColor;
}

void JupiterGraphics::drawPoint(int x, int y) {
    if (mClipStack.empty()) {
        throw GCN_EXCEPTION("Clip stack empty in drawPoint");
    }
    const ClipRectangle &top = mClipStack.top();
    x += top.xOffset;
    y += top.yOffset;
    if (!top.isContaining(x, y)) return;
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    uint32_t *fb = (uint32_t *)g_war1_back_fb;
    fb[y * SCREEN_W + x] = blend_pixel(fb[y * SCREEN_W + x], pack_argb(mColor));
}

/* Bresenham. Honors clip via per-point check (cheap for a UI line). */
void JupiterGraphics::drawLine(int x1, int y1, int x2, int y2) {
    if (mClipStack.empty()) {
        throw GCN_EXCEPTION("Clip stack empty in drawLine");
    }
    const ClipRectangle &top = mClipStack.top();
    x1 += top.xOffset; y1 += top.yOffset;
    x2 += top.xOffset; y2 += top.yOffset;
    int dx = x2 - x1, dy = y2 - y1;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    int err = adx - ady;
    uint32_t *fb = (uint32_t *)g_war1_back_fb;
    uint32_t pix = pack_argb(mColor);
    while (true) {
        if (top.isContaining(x1, y1)
            && x1 >= 0 && x1 < SCREEN_W && y1 >= 0 && y1 < SCREEN_H) {
            fb[y1 * SCREEN_W + x1] = blend_pixel(fb[y1 * SCREEN_W + x1], pix);
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -ady) { err -= ady; x1 += sx; }
        if (e2 <  adx) { err += adx; y1 += sy; }
    }
}

/* Outlined rectangle (4 lines). */
void JupiterGraphics::drawRectangle(const Rectangle &rect) {
    if (rect.width <= 0 || rect.height <= 0) return;
    drawLine(rect.x,                  rect.y,                  rect.x + rect.width - 1, rect.y);
    drawLine(rect.x,                  rect.y + rect.height - 1,rect.x + rect.width - 1, rect.y + rect.height - 1);
    drawLine(rect.x,                  rect.y,                  rect.x,                  rect.y + rect.height - 1);
    drawLine(rect.x + rect.width - 1, rect.y,                  rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void JupiterGraphics::fillRectangle(const Rectangle &rect) {
    if (mClipStack.empty()) {
        throw GCN_EXCEPTION("Clip stack empty in fillRectangle");
    }
    const ClipRectangle &top = mClipStack.top();
    int x0 = rect.x + top.xOffset;
    int y0 = rect.y + top.yOffset;
    int x1 = x0 + rect.width;
    int y1 = y0 + rect.height;
    /* Intersect with clip rect. */
    if (x0 < top.x) x0 = top.x;
    if (y0 < top.y) y0 = top.y;
    if (x1 > top.x + top.width)  x1 = top.x + top.width;
    if (y1 > top.y + top.height) y1 = top.y + top.height;
    /* Then with screen. */
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t *fb = (uint32_t *)g_war1_back_fb;
    uint32_t pix = pack_argb(mColor);
    bool opaque = (mColor.a == 255);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = fb + y * SCREEN_W;
        if (opaque) {
            for (int x = x0; x < x1; x++) row[x] = pix;
        } else {
            for (int x = x0; x < x1; x++) row[x] = blend_pixel(row[x], pix);
        }
    }
}

/* Image blit — sub-rectangle copy with alpha-blend per pixel.
 * JupiterImage stores ARGB8888 in row-major order. The widget bindings
 * convert CGraphic-backed Lua images into JupiterImage at construction
 * time (see arg_to_image in war1_widget_bindings.cpp), so this path
 * sees a JupiterImage uniformly — no type-erased fallback needed. */
void JupiterGraphics::drawImage(const Image *image, int srcX, int srcY,
                                int dstX, int dstY, int width, int height) {
    if (!image || width <= 0 || height <= 0) return;
    if (mClipStack.empty()) {
        throw GCN_EXCEPTION("Clip stack empty in drawImage");
    }
    const JupiterImage *jimg = dynamic_cast<const JupiterImage *>(image);
    if (!jimg) return;
    /* If the image was baked from a pc8 (CGraphic) source and the pc8's
     * palette has changed since the bake (CGraphic::SetPaletteColor),
     * re-expand into the cached buffer using the live palette. Same
     * live-palette behavior the title screen has via war1_blit_via_pc8;
     * the briefing's bg1:SetPaletteColor(255, ...) recolors the dithered
     * shadow pixels and needed this to take effect. */
    if (jimg->getPC8Source()) {
        const void *src_g = jimg->getPC8Source();
        uint32_t live = war1_pc8_palette_epoch(src_g);
        if (live != jimg->getPC8Epoch()) {
            JupiterImage *mut = const_cast<JupiterImage *>(jimg);
            war1_pc8_rebake_argb(src_g, mut->getPixels(), mut->getWidth(), mut->getHeight());
            mut->setPC8Epoch(live);
        }
    }
    const uint32_t *src = jimg->getPixels();
    if (!src) return;
    int sw = jimg->getWidth();
    int sh = jimg->getHeight();
    const ClipRectangle &top = mClipStack.top();
    dstX += top.xOffset;
    dstY += top.yOffset;
    /* Clip src rect to image bounds, then dst rect to clip + screen. */
    if (srcX < 0)        { dstX -= srcX; width += srcX; srcX = 0; }
    if (srcY < 0)        { dstY -= srcY; height += srcY; srcY = 0; }
    if (srcX + width  > sw) width  = sw - srcX;
    if (srcY + height > sh) height = sh - srcY;
    int x0 = dstX, y0 = dstY, x1 = dstX + width, y1 = dstY + height;
    if (x0 < top.x) { srcX += top.x - x0; x0 = top.x; }
    if (y0 < top.y) { srcY += top.y - y0; y0 = top.y; }
    if (x1 > top.x + top.width)  x1 = top.x + top.width;
    if (y1 > top.y + top.height) y1 = top.y + top.height;
    if (x0 < 0) { srcX += -x0; x0 = 0; }
    if (y0 < 0) { srcY += -y0; y0 = 0; }
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t *fb = (uint32_t *)g_war1_back_fb;
    for (int y = y0; y < y1; y++) {
        const uint32_t *srow = src + (srcY + (y - y0)) * sw + srcX;
        uint32_t *drow = fb + y * SCREEN_W + x0;
        for (int x = 0; x < x1 - x0; x++) {
            drow[x] = blend_pixel(drow[x], srow[x]);
        }
    }
}

} /* namespace gcn */
