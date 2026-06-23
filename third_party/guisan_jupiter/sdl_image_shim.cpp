/* gcn::SDLImage and gcn::SDLInput shims — Stratagus inherits CGraphic
 * from gcn::SDLImage and references SDLInput in its keyboard converter.
 * We don't compile guisan's sdl/ backend (no SDL2 on bare-metal), so
 * the upstream sdlimage.cpp / sdlinput.cpp aren't built and these
 * vtable symbols would be missing. Provide minimal stubs — the methods
 * are never called at runtime; we use pc8 blits + JupiterInput instead. */

#include "guisan/sdl/sdlimage.hpp"
#include "guisan/sdl/sdlinput.hpp"
#include "guisan/exception.hpp"

namespace gcn {

SDLImage::SDLImage(SDL_Surface *surface, bool autoFree, SDL_Renderer *renderer)
    : mSurface(surface), mTexture(nullptr), mRenderer(renderer), mAutoFree(autoFree) {}

SDLImage::~SDLImage() {}

SDL_Surface *SDLImage::getSurface() const { return mSurface; }
SDL_Texture *SDLImage::getTexture() const { return mTexture; }

int SDLImage::getWidth()  const { return 0; }
int SDLImage::getHeight() const { return 0; }

Color SDLImage::getPixel(int /*x*/, int /*y*/) { return Color(0, 0, 0, 0); }
void  SDLImage::putPixel(int /*x*/, int /*y*/, const Color & /*color*/) {}

void SDLImage::convertToDisplayFormat() {}
void SDLImage::free() {}

void       SDLInput::pushInput(SDL_Event /*event*/) {}
bool       SDLInput::isKeyQueueEmpty()    { return true; }
KeyInput   SDLInput::dequeueKeyInput()    { return KeyInput(); }
bool       SDLInput::isMouseQueueEmpty()  { return true; }
MouseInput SDLInput::dequeueMouseInput()  { return MouseInput(); }
Key        SDLInput::convertSDLEventToGuichanKeyValue(SDL_Event /*event*/) { return Key(0); }

} /* namespace gcn */
