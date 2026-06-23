/* SDL_image stub. */
#ifndef JUPITER_SDL_IMAGE_STUB_H
#define JUPITER_SDL_IMAGE_STUB_H
#include "SDL.h"
#ifdef __cplusplus
extern "C" {
#endif
SDL_Surface *IMG_Load(const char *file);
SDL_Surface *IMG_Load_RW(SDL_RWops *src, int freesrc);
int IMG_SavePNG(SDL_Surface *surface, const char *file);
#ifdef __cplusplus
}
#endif
#endif
