/* Jupiter bare-metal SDL STUB — types and constants only, no implementation.
 * Grows incrementally as the compiler demands. All functions here are declared
 * and defined (as no-ops) in war1_sdl_stub.cpp. We never actually run SDL. */
#ifndef JUPITER_SDL_STUB_H
#define JUPITER_SDL_STUB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Integer typedefs (must precede any struct that uses them) ---- */
typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;
typedef uint64_t Uint64;
typedef int64_t  Sint64;
typedef int      SDL_bool;

/* ---- Opaque-ish types ---- */
typedef struct SDL_Window    SDL_Window;
typedef struct SDL_Renderer  SDL_Renderer;
typedef struct SDL_Texture   SDL_Texture;
typedef struct SDL_Cursor    SDL_Cursor;
typedef struct SDL_BlitMap   SDL_BlitMap;
typedef struct _TTF_Font     TTF_Font;

/* SDL_PixelFormat — has fields referenced by fow.cpp / iolib.cpp */
typedef struct SDL_Palette { int ncolors; void *colors; } SDL_Palette;
typedef struct SDL_PixelFormat {
    Uint32 format;
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 padding[2];
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint8 Rloss, Gloss, Bloss, Aloss;
    Uint8 Rshift, Gshift, Bshift, Ashift;
    int refcount;
    struct SDL_PixelFormat *next;
} SDL_PixelFormat;

/* SDL_RWops — iolib casts to hidden.unknown.data1; keep shape compatible */
typedef struct SDL_RWops {
    Sint64 (*size)(struct SDL_RWops *);
    Sint64 (*seek)(struct SDL_RWops *, Sint64, int);
    size_t (*read)(struct SDL_RWops *, void *, size_t, size_t);
    size_t (*write)(struct SDL_RWops *, const void *, size_t, size_t);
    int    (*close)(struct SDL_RWops *);
    Uint32 type;
    union {
        struct { void *data1, *data2; } unknown;
        struct { void *data1; Sint64 size, pos; } mem;
    } hidden;
} SDL_RWops;
#define SDL_TRUE  1
#define SDL_FALSE 0

/* ---- Value types used by game logic ---- */
typedef struct SDL_Rect  { int x, y, w, h; } SDL_Rect;
typedef struct SDL_Point { int x, y; }       SDL_Point;
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;

/* SDL_Surface — opaque-ish. Game logic reads Width/Height/BytesPerPixel
 * from this via accessor functions, never via field access. Keep as opaque. */
typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    int pitch;
    void *pixels;
    void *userdata;
    int locked;
    void *lock_data;
    SDL_Rect clip_rect;
    SDL_BlitMap *map;
    int refcount;
} SDL_Surface;

/* ---- Key/input types (used only by event headers we stub) ---- */
typedef Sint32 SDL_Keycode;
typedef Uint32 SDL_Scancode;
typedef Uint16 SDL_Keymod;

/* ---- Enum-likes used by game logic / headers ---- */
typedef enum {
    SDL_BLENDMODE_NONE  = 0,
    SDL_BLENDMODE_BLEND = 1,
    SDL_BLENDMODE_ADD   = 2,
    SDL_BLENDMODE_MOD   = 4,
} SDL_BlendMode;

typedef enum { SDL_FLIP_NONE = 0, SDL_FLIP_HORIZONTAL = 1, SDL_FLIP_VERTICAL = 2 } SDL_RendererFlip;

/* ---- A few enums that pop up (keysyms etc.) ---- */
typedef struct SDL_Keysym {
    SDL_Scancode scancode;
    SDL_Keycode  sym;
    Uint16 mod;
    Uint32 unused;
} SDL_Keysym;

#define SDLK_ESCAPE   27
#define SDLK_RETURN   13
#define SDLK_SPACE    32
#define SDLK_UP       (1 << 30 | 82)
#define SDLK_DOWN     (1 << 30 | 81)
#define SDLK_LEFT     (1 << 30 | 80)
#define SDLK_RIGHT    (1 << 30 | 79)
#define SDLK_LSHIFT   (1 << 30 | 225)
#define SDLK_RSHIFT   (1 << 30 | 229)
#define SDLK_LCTRL    (1 << 30 | 224)
#define SDLK_RCTRL    (1 << 30 | 228)
#define SDLK_LALT     (1 << 30 | 226)
#define SDLK_RALT     (1 << 30 | 230)
#define SDLK_a        97
#define SDLK_z        122
#define SDLK_0        48
#define SDLK_9        57
#define SDLK_BACKSPACE      8
#define SDLK_TAB            9
#define SDLK_PAUSE          (1 << 30 | 72)
#define SDLK_BACKQUOTE      96
#define SDLK_EQUALS         61
#define SDLK_MINUS          45
#define SDLK_PERIOD         46
#define SDLK_CARET          94
#define SDLK_F1             (1 << 30 | 58)
#define SDLK_F2             (1 << 30 | 59)
#define SDLK_F3             (1 << 30 | 60)
#define SDLK_F4             (1 << 30 | 61)
#define SDLK_F5             (1 << 30 | 62)
#define SDLK_F6             (1 << 30 | 63)
#define SDLK_F7             (1 << 30 | 64)
#define SDLK_F8             (1 << 30 | 65)
#define SDLK_F9             (1 << 30 | 66)
#define SDLK_F10            (1 << 30 | 67)
#define SDLK_F11            (1 << 30 | 68)
#define SDLK_F12            (1 << 30 | 69)
#define SDLK_KP_ENTER       (1 << 30 | 88)
#define SDLK_KP_PLUS        (1 << 30 | 87)
#define SDLK_KP_MINUS       (1 << 30 | 86)
#define SDLK_KP_MULTIPLY    (1 << 30 | 85)
#define SDLK_KP_DIVIDE      (1 << 30 | 84)
#define SDLK_KP_0           (1 << 30 | 98)
#define SDLK_KP_1           (1 << 30 | 89)
#define SDLK_KP_2           (1 << 30 | 90)
#define SDLK_KP_3           (1 << 30 | 91)
#define SDLK_KP_4           (1 << 30 | 92)
#define SDLK_KP_5           (1 << 30 | 93)
#define SDLK_KP_6           (1 << 30 | 94)
#define SDLK_KP_7           (1 << 30 | 95)
#define SDLK_KP_8           (1 << 30 | 96)
#define SDLK_KP_9           (1 << 30 | 97)
#define SDLK_LGUI           (1 << 30 | 227)
#define SDLK_RGUI           (1 << 30 | 231)
#define SDLK_SYSREQ         (1 << 30 | 154)
#define SDLK_PRINTSCREEN    (1 << 30 | 70)
#define SDLK_INSERT         (1 << 30 | 73)
#define SDLK_DELETE         127
#define SDLK_HOME           (1 << 30 | 74)
#define SDLK_END            (1 << 30 | 77)
#define SDLK_PAGEUP         (1 << 30 | 75)
#define SDLK_PAGEDOWN       (1 << 30 | 78)
#define SDLK_CAPSLOCK       (1 << 30 | 57)
#define SDLK_SLASH          47
#define SDLK_BACKSLASH      92
#define SDLK_LEFTBRACKET    91
#define SDLK_RIGHTBRACKET   93
#define SDLK_SEMICOLON      59
#define SDLK_QUOTE          39
#define SDLK_COMMA          44
#define SDLK_ASTERISK       42
#define SDLK_PLUS           43
#define SDLK_1              49
#define SDLK_2              50
#define SDLK_3              51
#define SDLK_4              52
#define SDLK_5              53
#define SDLK_6              54
#define SDLK_7              55
#define SDLK_8              56
#define SDLK_KP_PERIOD      (1 << 30 | 99)
#define SDLK_KP_EQUALS      (1 << 30 | 103)
int strncasecmp(const char *a, const char *b, size_t n);
int strcasecmp(const char *a, const char *b);

#define KMOD_NONE   0x0000
#define KMOD_LSHIFT 0x0001
#define KMOD_RSHIFT 0x0002
#define KMOD_LCTRL  0x0040
#define KMOD_RCTRL  0x0080
#define KMOD_LALT   0x0100
#define KMOD_RALT   0x0200
#define KMOD_SHIFT  (KMOD_LSHIFT|KMOD_RSHIFT)
#define KMOD_CTRL   (KMOD_LCTRL |KMOD_RCTRL)
#define KMOD_ALT    (KMOD_LALT  |KMOD_RALT)

/* ---- Mouse button bits ---- */
#define SDL_BUTTON_LEFT    1
#define SDL_BUTTON_MIDDLE  2
#define SDL_BUTTON_RIGHT   3
#define SDL_BUTTON(X)      (1 << ((X)-1))

/* ---- Event stub — Stratagus has its own EventCallback layer, but some
 * headers reference SDL_Event. Provide a tagged union big enough. ---- */
typedef struct SDL_UserEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    int    code;
    void  *data1;
    void  *data2;
} SDL_UserEvent;
typedef union SDL_Event {
    Uint32 type;
    SDL_UserEvent user;
    Uint8  pad[56];
} SDL_Event;
#define SDL_zero(x) do { memset(&(x), 0, sizeof(x)); } while (0)
#define SDL_ADDEVENT   0
#define SDL_FIRSTEVENT 0
#define SDL_LASTEVENT  0xFFFF

/* ---- Minimal function prototypes — all no-ops in war1_sdl_stub.cpp ---- */
void SDL_FreeSurface(SDL_Surface *s);
void SDL_FreeCursor(SDL_Cursor *c);
void SDL_DestroyTexture(SDL_Texture *t);
void SDL_DestroyRenderer(SDL_Renderer *r);
void SDL_DestroyWindow(SDL_Window *w);
Uint32 SDL_GetTicks(void);
void  SDL_Delay(Uint32 ms);
int   SDL_PollEvent(SDL_Event *event);
int   SDL_PushEvent(SDL_Event *event);
int   SDL_PeepEvents(SDL_Event *events, int numevents, int action, Uint32 minType, Uint32 maxType);
const char *SDL_GetError(void);
const char *SDL_getenv(const char *name);
int   SDL_setenv(const char *name, const char *value, int overwrite);
int   SDL_AtomicIncRef(volatile int *counter);
int   SDL_AtomicDecRef(volatile int *counter);
Uint32 SDL_RegisterEvents(int numevents);
void *SDL_calloc(size_t nmemb, size_t size);
char *strdup(const char *s);
int   strncasecmp(const char *a, const char *b, size_t n);
int   strcasecmp(const char *a, const char *b);
void  SDL_SetWindowTitle(SDL_Window *w, const char *title);
int   SDL_ShowCursor(int toggle);
void  SDL_SetCursor(SDL_Cursor *c);
SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *s, int hotx, int hoty);
int   SDL_BlitScaled(SDL_Surface *src, SDL_Rect const *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
void  SDL_GetWindowSize(SDL_Window *w, int *wout, int *hout);
#define SDL_ENABLE  1
#define SDL_DISABLE 0
#define SDL_QUERY  -1

/* SDL_RWops minimal — iolib.h uses these for file I/O, which we route through
 * the objcopy-blob filesystem stub (war1_fs_stub.cpp). */
SDL_RWops *SDL_RWFromFile(const char *file, const char *mode);
SDL_RWops *SDL_RWFromMem(void *mem, int size);
SDL_RWops *SDL_AllocRW(void);
void       SDL_FreeRW(SDL_RWops *ctx);
size_t     SDL_RWread(SDL_RWops *ctx, void *ptr, size_t size, size_t maxnum);
Sint64     SDL_RWseek(SDL_RWops *ctx, Sint64 offset, int whence);
Sint64     SDL_RWtell(SDL_RWops *ctx);
int        SDL_RWclose(SDL_RWops *ctx);
#define SDL_RWOPS_UNKNOWN 0

/* Pixel / surface helpers referenced by game-logic code. Most never fire at
 * runtime because the adapter bypasses this path; stubs suffice for link. */
#define SDL_SWSURFACE 0
int   SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
int   SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth, Uint32 rm, Uint32 gm, Uint32 bm, Uint32 am);
SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 pixel_format, Uint32 flags);
Uint32 SDL_MapRGB (const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void   SDL_GetRGB (Uint32 px, const SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b);
void   SDL_GetRGBA(Uint32 px, const SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);
int    SDL_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors, int first, int ncolors);
int    SDL_SetSurfaceBlendMode(SDL_Surface *s, SDL_BlendMode mode);
int    SDL_GetColorKey(SDL_Surface *s, Uint32 *key);
int    SDL_SetColorKey(SDL_Surface *s, int flag, Uint32 key);
int    SDL_SetSurfacePalette(SDL_Surface *s, SDL_Palette *palette);
int    SDL_LockSurface(SDL_Surface *s);
void   SDL_UnlockSurface(SDL_Surface *s);
Uint32 SDL_MasksToPixelFormatEnum(int bpp, Uint32 rm, Uint32 gm, Uint32 bm, Uint32 am);
#define SDL_MUSTLOCK(s) (0)
void   SDL_free(void *p);

/* Clipboard (iolib uses it) */
int    SDL_SetClipboardText(const char *text);
char  *SDL_GetClipboardText(void);

/* Hints */
typedef enum { SDL_HINT_DEFAULT, SDL_HINT_NORMAL, SDL_HINT_OVERRIDE } SDL_HintPriority;
int    SDL_SetHintWithPriority(const char *name, const char *value, SDL_HintPriority priority);
#define SDL_HINT_RENDER_DRIVER "SDL_RENDER_DRIVER"

/* Byte order — Cortex-A7 is little endian on all our builds */
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER  SDL_LIL_ENDIAN

/* (SDL_SOUND_FINISHED is a runtime-allocated user event ID in Stratagus,
 * declared in sound_server.h — NOT a real SDL constant. Don't #define here.) */

#ifdef __cplusplus
}
#endif

#endif /* JUPITER_SDL_STUB_H */
