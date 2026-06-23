/* Bare-metal SDL stub for Nuked-SC55.
 * Provides just enough types/symbols to let mcu.cpp/submcu.cpp/lcd.cpp
 * compile. The functions are no-op stubs — we never actually call the
 * SDL paths because we replace mcu.cpp's main()/work_thread()/audio_callback
 * with our own bare-metal driver. */
#ifndef JUPITER_SDL_STUB_H
#define JUPITER_SDL_STUB_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t   Sint8;
typedef int16_t  Sint16;
typedef int32_t  Sint32;
typedef int64_t  Sint64;
typedef int      SDL_bool;

#define SDL_FALSE 0
#define SDL_TRUE  1
#define SDLCALL

/* ---- atomics ---- */
typedef struct { volatile int value; } SDL_atomic_t;
static inline int  SDL_AtomicGet(SDL_atomic_t *a)             { return a->value; }
static inline void SDL_AtomicSet(SDL_atomic_t *a, int v)      { a->value = v; }
static inline int  SDL_AtomicAdd(SDL_atomic_t *a, int v)      { int o=a->value; a->value+=v; return o; }
static inline int  SDL_AtomicCAS(SDL_atomic_t *a, int o, int n) { if(a->value==o){a->value=n;return 1;} return 0; }

/* ---- threads/mutex ---- */
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_Thread SDL_Thread;
typedef int (SDLCALL *SDL_ThreadFunction)(void *data);

static inline SDL_mutex *SDL_CreateMutex(void)        { return (SDL_mutex *)1; }
static inline int  SDL_LockMutex(SDL_mutex *m)        { (void)m; return 0; }
static inline int  SDL_UnlockMutex(SDL_mutex *m)      { (void)m; return 0; }
static inline void SDL_DestroyMutex(SDL_mutex *m)     { (void)m; }
static inline SDL_Thread *SDL_CreateThread(SDL_ThreadFunction f, const char *n, void *d) { (void)f; (void)n; (void)d; return (SDL_Thread *)1; }
static inline void SDL_WaitThread(SDL_Thread *t, int *s) { (void)t; (void)s; }
static inline void SDL_Delay(Uint32 ms)               { (void)ms; }

/* ---- audio ---- */
typedef Uint32 SDL_AudioDeviceID;
typedef Uint16 SDL_AudioFormat;
typedef void (SDLCALL *SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);

typedef struct {
    int             freq;
    SDL_AudioFormat format;
    Uint8           channels;
    Uint8           silence;
    Uint16          samples;
    Uint16          padding;
    Uint32          size;
    SDL_AudioCallback callback;
    void           *userdata;
} SDL_AudioSpec;

#define AUDIO_S8       0x8008
#define AUDIO_U8       0x0008
#define AUDIO_S16LSB   0x8010
#define AUDIO_S16MSB   0x9010
#define AUDIO_U16LSB   0x0010
#define AUDIO_U16MSB   0x1010
#define AUDIO_S32LSB   0x8020
#define AUDIO_S32MSB   0x9020
#define AUDIO_F32LSB   0x8120
#define AUDIO_F32MSB   0x9120
#define AUDIO_S16SYS   AUDIO_S16LSB

static inline int SDL_GetNumAudioDevices(int iscapture)        { (void)iscapture; return 0; }
static inline const char *SDL_GetAudioDeviceName(int i, int c) { (void)i; (void)c; return ""; }
static inline SDL_AudioDeviceID SDL_OpenAudioDevice(const char *d, int c, const SDL_AudioSpec *s, SDL_AudioSpec *o, int a) {
    (void)d; (void)c; (void)s; (void)o; (void)a; return 0;
}
static inline void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int p) { (void)dev; (void)p; }
static inline void SDL_CloseAudio(void) {}
static inline void SDL_CloseAudioDevice(SDL_AudioDeviceID d) { (void)d; }

/* ---- video/window/render (lcd.cpp) ---- */
typedef struct SDL_Window     SDL_Window;
typedef struct SDL_Renderer   SDL_Renderer;
typedef struct SDL_Texture    SDL_Texture;
typedef struct SDL_GameController SDL_GameController;
typedef struct { int x, y, w, h; } SDL_Rect;
typedef Uint32 SDL_Keycode;

#define SDL_INIT_AUDIO     0x10
#define SDL_INIT_VIDEO     0x20
#define SDL_INIT_TIMER     0x01
#define SDL_INIT_GAMECONTROLLER 0x2000

#define SDL_WINDOWPOS_CENTERED 0x2FFF0000
#define SDL_WINDOW_RESIZABLE 0x20

#define SDL_PIXELFORMAT_ARGB8888  0x16362004
#define SDL_TEXTUREACCESS_STREAMING 1

static inline int   SDL_Init(Uint32 f)                { (void)f; return 0; }
static inline void  SDL_Quit(void)                    {}
static inline const char *SDL_GetError(void)          { return ""; }
static inline SDL_Window *SDL_CreateWindow(const char *t, int x, int y, int w, int h, Uint32 fl) { (void)t;(void)x;(void)y;(void)w;(void)h;(void)fl; return 0; }
static inline void  SDL_DestroyWindow(SDL_Window *w)  { (void)w; }
static inline SDL_Renderer *SDL_CreateRenderer(SDL_Window *w, int i, Uint32 f) { (void)w;(void)i;(void)f; return 0; }
static inline void  SDL_DestroyRenderer(SDL_Renderer *r) { (void)r; }
static inline SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 fmt, int access, int w, int h) { (void)r;(void)fmt;(void)access;(void)w;(void)h; return 0; }
static inline void  SDL_DestroyTexture(SDL_Texture *t) { (void)t; }
static inline int   SDL_RenderClear(SDL_Renderer *r)  { (void)r; return 0; }
static inline int   SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *s, const SDL_Rect *d) { (void)r;(void)t;(void)s;(void)d; return 0; }
static inline void  SDL_RenderPresent(SDL_Renderer *r) { (void)r; }
static inline int   SDL_LockTexture(SDL_Texture *t, const SDL_Rect *r, void **p, int *pp) { (void)t;(void)r;(void)p;(void)pp; return 0; }
static inline void  SDL_UnlockTexture(SDL_Texture *t) { (void)t; }
static inline int   SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *r, const void *p, int pp) { (void)t;(void)r;(void)p;(void)pp; return 0; }
static inline int   SDL_SetWindowTitle(SDL_Window *w, const char *t) { (void)w; (void)t; return 0; }
static inline void  SDL_SetWindowSize(SDL_Window *w, int x, int y) { (void)w;(void)x;(void)y; }
static inline int   SDL_PollEvent(void *e)            { (void)e; return 0; }

/* ---- scancodes (mcu.cpp button table) ---- */
typedef int SDL_Scancode;
#define SDL_SCANCODE_Q 20
#define SDL_SCANCODE_W 26
#define SDL_SCANCODE_E 8
#define SDL_SCANCODE_R 21
#define SDL_SCANCODE_T 23
#define SDL_SCANCODE_Y 28
#define SDL_SCANCODE_U 24
#define SDL_SCANCODE_I 12
#define SDL_SCANCODE_O 18
#define SDL_SCANCODE_P 19
#define SDL_SCANCODE_LEFTBRACKET 47
#define SDL_SCANCODE_A 4
#define SDL_SCANCODE_S 22
#define SDL_SCANCODE_D 7
#define SDL_SCANCODE_F 9
#define SDL_SCANCODE_G 10
#define SDL_SCANCODE_H 11
#define SDL_SCANCODE_J 13
#define SDL_SCANCODE_K 14
#define SDL_SCANCODE_L 15
#define SDL_SCANCODE_RIGHTBRACKET 48
#define SDL_SCANCODE_Z 29
#define SDL_SCANCODE_X 27
#define SDL_SCANCODE_C 6
#define SDL_SCANCODE_V 25
#define SDL_SCANCODE_B 5
#define SDL_SCANCODE_N 17
#define SDL_SCANCODE_M 16
#define SDL_SCANCODE_COMMA 54
#define SDL_SCANCODE_PERIOD 55

#endif /* JUPITER_SDL_STUB_H */
