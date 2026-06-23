/* Jupiter port: no-op bodies for every function prototyped in the SDL/Lua/
 * zlib/bzlib stub headers. These are never actually called at runtime — the
 * real adapter code (war1_video.cpp / war1_sound.cpp / war1_input.cpp / etc.)
 * intercepts game-logic calls well above this layer. These definitions exist
 * only to satisfy the linker.
 *
 * If a function here ever DOES get called at runtime, we trap to UART so we
 * can catch the surprise caller and decide whether to route it properly. */

#include <string.h>
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#include "zlib.h"
#include "bzlib.h"

extern "C" void uart_puts(const char *);
static void trap(const char *fn) { uart_puts("[war1_stub] called "); uart_puts(fn); uart_puts(" — should not happen\n"); }

/* ================================================================
 *  SDL
 * ================================================================ */
extern "C" {
extern void *malloc(size_t);
extern void  free(void *);

void SDL_FreeSurface(SDL_Surface *s) {
    if (!s) return;
    if (s->pixels && !(s->flags & 0x80000000u)) free(s->pixels); /* 0x80000000: external backing */
    if (s->format) free(s->format);
    free(s);
}
void SDL_FreeCursor(SDL_Cursor *)   {}
void SDL_DestroyTexture(SDL_Texture *) {}
void SDL_DestroyRenderer(SDL_Renderer *) {}
void SDL_DestroyWindow(SDL_Window *) {}
extern uint32_t timer_read(void);
Uint32 SDL_GetTicks(void) {
    /* 24MHz down-counter — convert to monotonically-increasing ms. */
    static uint32_t last_raw = 0;
    static uint32_t acc_ticks = 0;
    uint32_t raw = timer_read();
    if (!last_raw) last_raw = raw;
    uint32_t delta = last_raw - raw; /* down-counter */
    last_raw = raw;
    acc_ticks += delta;
    return acc_ticks / 24000;
}
void   SDL_Delay(Uint32)  { trap("SDL_Delay"); }
int    SDL_PollEvent(SDL_Event *) { return 0; }
int    SDL_PushEvent(SDL_Event *) { return 0; }
int    SDL_PeepEvents(SDL_Event *, int, int, Uint32, Uint32) { return 0; }
const char *SDL_GetError(void) { return ""; }
const char *SDL_getenv(const char *) { return nullptr; }
int    SDL_setenv(const char *, const char *, int) { return 0; }
Uint32 SDL_RegisterEvents(int) { static Uint32 n = 0x8000; return n++; }
extern void *malloc(size_t);
void *SDL_calloc(size_t n, size_t s) { void *p = malloc(n * s); if (p) memset(p, 0, n * s); return p; }

/* SDL_mixer stubs — most are no-ops; the SFX path (Mix_LoadWAV /
 * Mix_PlayChannel / Mix_HaltChannel / Mix_Playing / Mix_Volume /
 * Mix_VolumeChunk / Mix_FreeChunk) is implemented for real in
 * examples/war1/war1_bridge.cpp where the sound catalog lives. */
int  Mix_Init(int)                                        { return 0; }
void Mix_Quit(void)                                       {}
int  Mix_OpenAudio(int, Uint16, int, int)                 { return 0; }
void Mix_CloseAudio(void)                                 {}
int  Mix_AllocateChannels(int n)                          { return n; }
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *, int)               { return nullptr; }
int  Mix_SetPanning(int, Uint8, Uint8)                    { return 0; }
Mix_Chunk *Mix_GetChunk(int)                              { return nullptr; }
int  Mix_Resume(int)                                      { return 0; }
int  Mix_Pause(int)                                       { return 0; }
int  Mix_FadeInMusic(Mix_Music *, int, int)               { return -1; }
const char *Mix_GetError(void)                            { return ""; }
/* Mix_LoadMUS / Mix_LoadMUS_RW / Mix_FreeMusic / Mix_PlayMusic
 * / Mix_PlayingMusic / Mix_HaltMusic / Mix_PauseMusic / Mix_ResumeMusic
 * / Mix_VolumeMusic / Mix_FadeOutMusic are real implementations in
 * examples/war1/war1_bridge.cpp — they stream music tracks from raw SD
 * blocks past the boot partition. */
int  Mix_GetNumChunkDecoders(void)                        { return 0; }
int  Mix_GetNumMusicDecoders(void)                        { return 0; }
const char *Mix_GetChunkDecoder(int)                      { return ""; }
const char *Mix_GetMusicDecoder(int)                      { return ""; }
/* Mix_HookMusicFinished is real in war1_bridge.cpp (it triggers the music
 * pump callback). Removing from here. */
int  Mix_RegisterEffect(int, Mix_EffectFunc_t, Mix_EffectDone_t, void *) { return 0; }
int  Mix_UnregisterAllEffects(int)                        { return 0; }
int  Mix_ChannelFinished(void (*)(int))                   { return 0; }
void   SDL_SetWindowTitle(SDL_Window *, const char *) {}
int    SDL_ShowCursor(int) { return 0; }
void   SDL_SetCursor(SDL_Cursor *) {}
SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *, int, int) { return nullptr; }
int    SDL_BlitScaled(SDL_Surface *, SDL_Rect const *, SDL_Surface *, SDL_Rect *) { return 0; }
void   SDL_GetWindowSize(SDL_Window *, int *w, int *h) { if (w) *w = 480; if (h) *h = 272; }

SDL_RWops *SDL_RWFromFile(const char *, const char *) { trap("SDL_RWFromFile"); return nullptr; }
SDL_RWops *SDL_RWFromMem(void *, int) { trap("SDL_RWFromMem"); return nullptr; }
SDL_RWops *SDL_AllocRW(void) { return nullptr; }
void       SDL_FreeRW(SDL_RWops *) {}
size_t     SDL_RWread(SDL_RWops *, void *, size_t, size_t) { return 0; }
Sint64     SDL_RWseek(SDL_RWops *, Sint64, int) { return 0; }
Sint64     SDL_RWtell(SDL_RWops *) { return 0; }
int        SDL_RWclose(SDL_RWops *) { return 0; }

/* Real 32-bit ARGB SDL_Surface impls.
 * Stratagus's FoW / minimap / hidden-surface code allocates intermediate
 * SDL_Surfaces and blits into them. On bare-metal we skip SDL but keep
 * the surface API — buffers live in the libc_shim heap. */
SDL_Surface *SDL_CreateRGBSurface(Uint32 /*flags*/, int w, int h,
                                  int /*depth*/, Uint32 rm, Uint32 gm,
                                  Uint32 bm, Uint32 am) {
    SDL_Surface *s = (SDL_Surface *)malloc(sizeof(SDL_Surface));
    if (!s) return nullptr;
    memset(s, 0, sizeof(*s));
    s->w = w; s->h = h; s->pitch = w * 4; s->refcount = 1;
    s->pixels = malloc((size_t)w * h * 4);
    if (!s->pixels) { free(s); return nullptr; }
    memset(s->pixels, 0, (size_t)w * h * 4);
    s->format = (SDL_PixelFormat *)malloc(sizeof(SDL_PixelFormat));
    if (s->format) {
        memset(s->format, 0, sizeof(*s->format));
        s->format->BitsPerPixel = 32;
        s->format->BytesPerPixel = 4;
        s->format->Rmask = rm; s->format->Gmask = gm;
        s->format->Bmask = bm; s->format->Amask = am;
        s->format->Rshift = rm ? __builtin_ctz(rm) : 0;
        s->format->Gshift = gm ? __builtin_ctz(gm) : 0;
        s->format->Bshift = bm ? __builtin_ctz(bm) : 0;
        s->format->Ashift = am ? __builtin_ctz(am) : 0;
    }
    s->clip_rect = SDL_Rect{0, 0, w, h};
    return s;
}
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color) {
    if (!dst || !dst->pixels) return -1;
    int x0 = 0, y0 = 0, x1 = dst->w, y1 = dst->h;
    if (rect) { x0 = rect->x; y0 = rect->y; x1 = x0 + rect->w; y1 = y0 + rect->h; }
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > dst->w) x1 = dst->w; if (y1 > dst->h) y1 = dst->h;
    Uint32 *px = (Uint32 *)dst->pixels;
    int stride = dst->pitch / 4;
    for (int y = y0; y < y1; y++) {
        Uint32 *row = px + y * stride;
        for (int x = x0; x < x1; x++) row[x] = color;
    }
    return 0;
}
int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect,
                    SDL_Surface *dst, SDL_Rect *dstrect) {
    if (!src || !dst || !src->pixels || !dst->pixels) return -1;
    int sx = srcrect ? srcrect->x : 0;
    int sy = srcrect ? srcrect->y : 0;
    int w  = srcrect ? srcrect->w : src->w;
    int h  = srcrect ? srcrect->h : src->h;
    int dx = dstrect ? dstrect->x : 0;
    int dy = dstrect ? dstrect->y : 0;
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > dst->w) w = dst->w - dx;
    if (dy + h > dst->h) h = dst->h - dy;
    if (w <= 0 || h <= 0) return 0;
    const int sstride = src->pitch / 4, dstride = dst->pitch / 4;
    const Uint32 *sp = (const Uint32 *)src->pixels;
    Uint32       *dp = (Uint32 *)dst->pixels;
    for (int y = 0; y < h; y++) {
        const Uint32 *srow = sp + (sy + y) * sstride + sx;
        Uint32       *drow = dp + (dy + y) * dstride + dx;
        for (int x = 0; x < w; x++) drow[x] = srow[x];
    }
    return 0;
}
SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *, Uint32, Uint32) { return nullptr; }
Uint32 SDL_MapRGB (const SDL_PixelFormat *, Uint8, Uint8, Uint8) { return 0; }
Uint32 SDL_MapRGBA(const SDL_PixelFormat *, Uint8, Uint8, Uint8, Uint8) { return 0; }
void   SDL_GetRGB (Uint32, const SDL_PixelFormat *, Uint8 *r, Uint8 *g, Uint8 *b) { if (r)*r=0; if (g)*g=0; if (b)*b=0; }
void   SDL_GetRGBA(Uint32, const SDL_PixelFormat *, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a) { if (r)*r=0; if (g)*g=0; if (b)*b=0; if (a)*a=0; }
int    SDL_SetPaletteColors(SDL_Palette *, const SDL_Color *, int, int) { return 0; }
int    SDL_SetSurfaceBlendMode(SDL_Surface *, SDL_BlendMode) { return 0; }
int    SDL_GetColorKey(SDL_Surface *, Uint32 *key) { if (key) *key = 0; return -1; }
int    SDL_SetColorKey(SDL_Surface *, int, Uint32) { return 0; }
int    SDL_SetSurfacePalette(SDL_Surface *, SDL_Palette *) { return 0; }
int    SDL_LockSurface(SDL_Surface *) { return 0; }
void   SDL_UnlockSurface(SDL_Surface *) {}
Uint32 SDL_MasksToPixelFormatEnum(int, Uint32, Uint32, Uint32, Uint32) { return 0; }
void   SDL_free(void *) {}

int    SDL_SetClipboardText(const char *) { return 0; }
char  *SDL_GetClipboardText(void) { return nullptr; }
int    SDL_SetHintWithPriority(const char *, const char *, SDL_HintPriority) { return 0; }

/* SDL_image */
SDL_Surface *IMG_Load(const char *) { trap("IMG_Load"); return nullptr; }
SDL_Surface *IMG_Load_RW(SDL_RWops *, int) { return nullptr; }
int IMG_SavePNG(SDL_Surface *, const char *) { return 0; }

/* SDL_mixer — consolidated with the full stub set above */

/* zlib / bzlib — all no-op since save/load is out of scope for v1 */
gzFile gzopen(const char *, const char *) { return nullptr; }
int    gzclose(gzFile) { return 0; }
int    gzread(gzFile, void *, unsigned int) { return 0; }
int    gzwrite(gzFile, const void *, unsigned int) { return 0; }
int    gzflush(gzFile, int) { return 0; }
int    gzeof(gzFile) { return 1; }
long   gztell(gzFile) { return 0; }
long   gzseek(gzFile, long, int) { return 0; }
const char *gzerror(gzFile, int *errnum) { if (errnum) *errnum = 0; return ""; }
int    gzputs(gzFile, const char *) { return 0; }
char  *gzgets(gzFile, char *, int) { return nullptr; }
int    gzgetc(gzFile) { return -1; }
int    gzungetc(int, gzFile) { return -1; }
BZFILE *BZ2_bzopen(const char *, const char *) { return nullptr; }
int     BZ2_bzclose(BZFILE *) { return 0; }
int     BZ2_bzread(BZFILE *, void *, int) { return 0; }
int     BZ2_bzwrite(BZFILE *, const void *, int) { return 0; }
int     BZ2_bzflush(BZFILE *) { return 0; }

/* Lua stubs REMOVED — real Lua 5.3 now provides all lua_* / luaL_* symbols.
 * Only a couple Stratagus-specific helpers that weren't in standard Lua
 * remain: */
int lua_isstring_strict(lua_State *, int) { return 0; }
/* luaL_getn was removed in Lua 5.2+ — still provide as a shim. */
size_t luaL_getn(lua_State *L, int idx) { return lua_rawlen(L, idx); }
/* luaL_checkint / luaL_optint — with -DLUA_COMPAT_5_1 in WAR1_CXX_FLAGS,
 * lauxlib.h already provides these as macros. No function stubs needed. */
/* Lua 5.3 has lua_pop / lua_istable as #define macros in lua.h. But they
 * resolve to function calls (lua_settop, lua_type) at the macro expansion.
 * Something (Stratagus's script.h probably) takes their ADDRESS as function
 * pointers — that fails if they're only macros. Provide function stubs. */
void lua_pop_fn(lua_State *L, int n) { lua_settop(L, -(n)-1); }
int  lua_istable_fn(lua_State *L, int n) { return lua_type(L, n) == LUA_TTABLE; }
} /* extern "C" */

/* Custom luaL_openlibs — excludes io/os/package libs that we stripped
 * (they need a hosted OS). Registers only the libs whose .c files we kept. */
extern "C" {
int luaopen_base(lua_State *);
int luaopen_coroutine(lua_State *);
int luaopen_table(lua_State *);
int luaopen_string(lua_State *);
int luaopen_utf8(lua_State *);
int luaopen_bit32(lua_State *);
int luaopen_math(lua_State *);
int luaopen_debug(lua_State *);
/* Stubs for libs whose .c files we didn't compile (loslib.c, loadlib.c,
 * liolib.c — they need a hosted OS). Stratagus's InitLua registers these
 * by name but the bindings are no-ops; Lua scripts that USE os.exit or
 * io.read will just see an empty module. */
int luaopen_os(lua_State *L)      { lua_newtable(L); return 1; }
int luaopen_package(lua_State *L) { lua_newtable(L); return 1; }
int luaopen_io(lua_State *L)      { lua_newtable(L); return 1; }

void luaL_openlibs(lua_State *L) {
    static const luaL_Reg libs[] = {
        { "_G",          luaopen_base      },
        { "coroutine",   luaopen_coroutine },
        { "table",       luaopen_table     },
        { "string",      luaopen_string    },
        { "utf8",        luaopen_utf8      },
        /* bit32 skipped — Lua 5.3 has native bit operators (<<, >>, &, |).
         * lbitlib.c is vestigial 5.2 compat; hangs on our config. */
        { "math",        luaopen_math      },
        { "debug",       luaopen_debug     },
        { NULL, NULL }
    };
    for (const luaL_Reg *lib = libs; lib->func; lib++) {
        uart_puts("[LUA] opening lib: "); uart_puts(lib->name); uart_puts("...\n");
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
        uart_puts("[LUA] opened  lib: "); uart_puts(lib->name); uart_puts(" OK\n");
    }
}
} /* extern "C" */
