/* Jupiter port: link-time stubs for all Stratagus symbols referenced by the
 * kept source files but defined in .cpp files we didn't copy (ai/, ui/, video/,
 * sound/, network/, stratagus/stratagus.cpp, script_*.cpp, editor/).
 *
 * Purpose: satisfy the linker. Most bodies are no-ops or return defaults.
 * Real behavior for CGraphic / CVideo / PlayUnitSound / etc. comes later in
 * the dedicated adapter files (war1_video.cpp, war1_sound.cpp, war1_input.cpp).
 *
 * If a function here is ever actually called at runtime with real work, it
 * should either (a) get a real implementation in an adapter file, or (b) get
 * a uart trap so we notice. */

#include "stratagus.h"
#include "player.h"
#include "unit.h"
#include "unittype.h"
#include "unitsound.h"
#include "map.h"
#include "viewport.h"
#include "ui.h"
#include "missile.h"
#include "script.h"
#include "sound.h"
#include "sound_server.h"
#include "network.h"
#include "parameters.h"
#include "video.h"
#include "cursor.h"
#include "font.h"
#include "icons.h"
#include "iolib.h"
#include "particle.h"
#include "interface.h"
#include "settings.h"
#include "trigger.h"
#include "replay.h"
#include "title.h"
#include "results.h"
#include "minimap.h"
#include "commands.h"
#include "actions.h"
#include "spells.h"
#include "upgrade.h"
#include "online_service.h"
#include "ai.h"
#include "editor.h"
#include "ui/popup.h"
#include "ui/contenttype.h"
#include "../stratagus/src/ai/ai_local.h"

extern "C" void uart_puts(const char *);
extern "C" void uart_putdec(uint32_t);
extern "C" void uart_puthex(uint32_t);

/* ================================================================
 *  Globals expected by game-logic code (normally live in the .cpp
 *  files we didn't copy).
 * ================================================================ */
/* EnableAssert / EnableDebugPrint / EnableUnitDebug / IsRestart /
 * IsDebugEnabled / EnableWallsInSinglePlayer now in
 * third_party/stratagus/src/stratagus/stratagus.cpp */

/* GameRunning / GamePaused / GameObserve now in ui/interface.cpp */
/* GameEstablishing / BigMapMode now in ui/interface.cpp
 * GodMode now in ui/interface.cpp
 * FancyBuildings now in ui/ui.cpp */
/* UseHPForXp now in game/game.cpp */
/* SaveGameLoading now in game/loadgame.cpp. */

/* GameResult / GameCycle / FastForwardCycle / GameName / GameSettings / ... —
 * now defined in game/game.cpp (ported). */
/* SkipGameCycle now in ui/interface.cpp
 * KeyModifiers now in ui/mouse.cpp */
unsigned long FrameCounter = 0;
int NetPlayers             = 0;
int NetLocalPlayerNumber   = 0;

/* StratagusLibPath / OriginalArgv now in stratagus.cpp */
/* DamageMissile, Preference: now defined by script_ui.cpp (ported). */
/* std::string DamageMissile; */
std::unique_ptr<OnlineContext> OnlineContextHandler;
/* Damage now defined in script_missile.cpp (restored). */

bool NetworkInSync         = true;
CUDPSocket NetworkFildes;

int ClipX1 = 0, ClipX2 = 0, ClipY1 = 0, ClipY2 = 0;
/* Upstream derives these via SDL_MapRGB against the screen's pixel format.
 * On bare-metal our TheScreen format is ARGB8888 (A in top byte), so color
 * constants are (0xFF << 24) | (R << 16) | (G << 8) | B. Setting them
 * explicitly matches the format and keeps FillRectangleClip(ColorBlack)
 * from painting transparent pixels that let the FB0 menu bleed through. */
Uint32 ColorBlack = 0xFF000000u, ColorDarkGreen = 0xFF004000u,
       ColorDarkGray = 0xFF404040u, ColorBlue = 0xFF0000FFu,
       ColorLightBlue = 0xFF4080FFu, ColorGreen = 0xFF00FF00u,
       ColorYellow = 0xFFFFFF00u, ColorRed = 0xFFFF0000u,
       ColorOrange = 0xFFFF8000u, ColorWhite = 0xFFFFFFFFu,
       ColorGray = 0xFF808080u, ColorLightGray = 0xFFC0C0C0u;

CEditor Editor;
/* UI now in ui/ui.cpp */
CVideo Video;
/* GameSettings in game/game.cpp */
/* GameSounds now in sound/sound.cpp */
/* UnitTypeVar now defined in script_unittype.cpp (restored). */
/* TriggerData in game/trigger.cpp */
/* CPreference Preference; — now in script_ui.cpp */
/* ParticleManager now in particle/particlemanager.cpp */
/* GameCursor / CursorBuilding / CursorState / CursorScreenPos now in
 * third_party/stratagus/src/video/cursor.cpp */
/* UnitUnderCursor now in ui/mouse.cpp */
/* TheScreen: backing SDL_Surface whose pixel buffer IS the Jupiter UI0
 * overlay. We double-buffer: two hardware-visible buffers at OVL_ADDR
 * (0x43B00000) and OVL1_ADDR (0x43C00000). All drawing (pc8 blits,
 * SDL_BlitSurface, FillRectangleClip, fog alpha-composite) targets the
 * back buffer via g_war1_back_fb. Invalidate() at end-of-frame flushes,
 * tells the display engine to swap to the back, waits for vblank, and
 * swaps our software roles — so the next frame draws into what was just
 * shown and the freshly-drawn buffer is now being scanned out.
 *
 * Why: UI0 has a single UI_ADDR register. Drawing straight into the live
 * buffer races the 60 Hz scan-out, producing the flicker that correlates
 * with the game running (Stratagus clears to ColorBlack every frame at
 * mainloop.cpp:189 before repainting — visible tearing during the gap).
 *
 * Hardcoded addresses here match include/v3s.h to avoid pulling jupiter.h
 * up to file scope (its `#define UI` clashes with Stratagus's UI global). */
static constexpr uintptr_t OVL_FRONT_INIT = 0x43B00000u;  /* OVL_ADDR  */
static constexpr uintptr_t OVL_BACK_INIT  = 0x43C00000u;  /* OVL1_ADDR */
extern "C" volatile uint32_t g_war1_back_fb = OVL_BACK_INIT;
static uint32_t g_war1_front_fb = OVL_FRONT_INIT;

static SDL_Surface     s_the_screen;
static SDL_PixelFormat s_the_screen_format;
/* Idempotent — safe to call any number of times. Bare-metal port previously
 * relied on a static-int initializer to populate s_the_screen, but that
 * can be skipped depending on how .init_array runs, leaving pixels=null
 * and w=h=0. Result: every BlitSurfaceAlphaBlending / SDL_BlitSurface
 * targeting TheScreen silently no-ops at the `if (w<=0)` early-return —
 * that's what made fog of war invisible despite FogSurface having
 * correct 0xFF303030 alpha pixels. We now call this explicitly from
 * InitVideo (which stratagusMain runs before any draws). */
extern "C" void war1_init_the_screen(void) {
    constexpr int SCREEN_W = 480;
    constexpr int SCREEN_H = 272;
    s_the_screen_format.BitsPerPixel = 32;
    s_the_screen_format.BytesPerPixel = 4;
    s_the_screen_format.Rmask = 0x00FF0000; s_the_screen_format.Rshift = 16;
    s_the_screen_format.Gmask = 0x0000FF00; s_the_screen_format.Gshift = 8;
    s_the_screen_format.Bmask = 0x000000FF; s_the_screen_format.Bshift = 0;
    s_the_screen_format.Amask = 0xFF000000; s_the_screen_format.Ashift = 24;
    s_the_screen.format = &s_the_screen_format;
    s_the_screen.w      = SCREEN_W;
    s_the_screen.h      = SCREEN_H;
    s_the_screen.pitch  = SCREEN_W * 4;
    s_the_screen.pixels = (void *)g_war1_back_fb;
    s_the_screen.flags  = 0x80000000u;   /* external backing; SDL_FreeSurface skips free() */
    s_the_screen.clip_rect = SDL_Rect{0, 0, SCREEN_W, SCREEN_H};
}
[[maybe_unused]] static int s_the_screen_init = (war1_init_the_screen(), 0);
SDL_Surface *TheScreen = &s_the_screen;
SDL_Window *TheWindow = nullptr;
/* SizeChangeCounter is defined in upstream sdl.cpp; on bare-metal we have
 * a fixed 480x272 screen that never resizes — provide a zero that never
 * changes so CursorOn code skips rebuild paths. */
uint8_t SizeChangeCounter = 0;
/* CursorOn now in ui/mouse.cpp */
#include "interface.h"
/* IsDemoMode now in ui/ui.cpp */
/* PlayerAi *AiPlayer — real definition in ai/ai.cpp */

/* CViewport methods from upstream map/map_draw.cpp which we don't port.
 * Called from cursor.cpp DrawCursor; bare-metal path just returns safely. */
#include "viewport.h"
/* CViewport methods now in map/map_draw.cpp */

/* Lua global defined in script.cpp now. */

/* Parameters::Instance now in third_party/stratagus/src/stratagus/parameters.cpp
 * stratagusMain / Exit / ExitFatal now in stratagus.cpp — but we still need
 * the setjmp rig. Upstream ExitFatal calls exit(err) which doesn't exist on
 * bare-metal. Our bare-metal exit() below longjmp's to the bridge's recovery
 * point so ExitFatal paths unwind cleanly. */
#include <setjmp.h>
jmp_buf g_war1_fatal_jmp;
int g_war1_fatal_armed = 0;

/* Bare-metal exit(): libc's exit is absent. Route to our longjmp recovery
 * if armed, otherwise spin. */
extern "C" [[noreturn]] void exit(int err) {
    uart_puts("[stratagus] exit called\n");
    if (g_war1_fatal_armed) {
        longjmp(g_war1_fatal_jmp, err ? err : 1);
    }
    uart_puts("[stratagus] no setjmp target — freezing\n");
    for(;;);
}

/* Engine teardown helpers called by upstream Exit(). We don't port
 * video/font.cpp (SDL-heavy), sound_server.cpp, online_service.cpp; stub
 * their cleanup entry points. */
/* CleanIcons now in ui/icons.cpp */
/* CleanUserInterface now in ui/ui.cpp */
void CleanFonts() {}
/* QuitSound / FreeSounds now in sound/ ported files */
void FreeGraphics() {}
/* FreeButtonStyles now in ui/ui.cpp */
void DeInitOnlineService() {}
void InitOnlineService() {}
/* Refresh rate target for the game-loop pacing. Desktop SDL chooses based
 * on the display mode; we have a fixed 60 Hz LCD. */
int RefreshRate = 60;
/* Networking is out of scope for the bare-metal port. CNetworkParameter is
 * the singleton ParseCommandLine references for -h/-P flags; provide an
 * empty instance so the linker is happy. The actual network code paths are
 * stubbed elsewhere in this file. */
#include "netconnect.h"
CNetworkParameter::CNetworkParameter() : localPort(defaultPort),
    gameCyclesPerUpdate(1), NetworkLag(10), timeoutInS(45) {}
void CNetworkParameter::FixValues() {}
CNetworkParameter CNetworkParameter::Instance;
/* freeGuichan / initGuichan / DrawGuichanWidgets / handleInput now in
 * third_party/stratagus_port/war1_widgets.cpp — they construct gcn::Gui
 * with the JupiterGraphics + JupiterInput backends. */
void DeInitImageLoaders() {}
unsigned long SlowFrameCounter = 0;
/* CallbackMusicEnable / CallbackMusicTrigger now in sound/music.cpp */
/* CLabel — Stratagus HUD's text drawer (DrawResources, status line,
 * message log, etc). Routes through war1_draw_text_at so the same pc8
 * glyph blits power both gcn-side widget text and HUD text. The font is
 * an instance member set in the ctor; "normal" is yellow, "reverse" is
 * white (war1 default colors via SetDefaultTextColors). */
static void war1_draw_text_at(const CFont *font, int absX, int absY,
                              std::string_view txt,
                              uint32_t normal_argb, uint32_t reverse_argb,
                              int clipL = 0, int clipT = 0, int clipR = 0, int clipB = 0);
static void war1_clabel_draw_str(const CFont *font, int x, int y, std::string_view s) {
    /* HUD's UI.NormalFontColor = "white" (ui.lua:350); reverse is the
     * accent / "low food" alarm color, which war1 paints as yellow. */
    war1_draw_text_at(font, x, y, s,
        /*normal_argb*/ 0xFFFFFFFFu,    /* white */
        /*reverse_argb*/0xFFFFE055u);   /* yellow — no clip, full screen */
}
static void war1_clabel_draw_int(const CFont *font, int x, int y, int n) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", n);
    war1_clabel_draw_str(font, x, y, buf);
}
int CLabel::DrawCentered(int x, int y, std::string_view s) const {
    int w = font ? font->Width(s) : 0;
    war1_clabel_draw_str(font, x - w / 2, y, s);
    return w;
}
int CLabel::Draw(int x, int y, std::string_view s) const {
    war1_clabel_draw_str(font, x, y, s);
    return font ? font->Width(s) : 0;
}
int CLabel::Draw(int x, int y, int n) const {
    war1_clabel_draw_int(font, x, y, n);
    return 0;
}
extern "C" void war1_blit_via_pc8(const CGraphic *g, unsigned frame, int x, int y);
extern "C" int  war1_blit_via_pc8_scaled(const CGraphic *g, int dstX, int dstY, int dstW, int dstH);
extern "C" int  war1_pc8_image_w_for(const CGraphic *g);
extern "C" int  war1_pc8_image_h_for(const CGraphic *g);
void CGraphic::DrawClip(int x, int y, SDL_Surface *) const {
    /* Two callers in the engine:
     *   - TitleScreen::ShowTitleImage / WC1 splash: did
     *       Resize(Video.Width, Video.Height)
     *     to bump g->Width/Height past the native pc8 sheet, expecting a
     *     scaled blit. Caller-computed (x, y) anchors the resized rect
     *     (= (0, 0) for a screen-sized graphic).
     *   - CContentTypeGraphic::Draw and friends: native blit of the
     *     unmodified pc8 at this->Pos. g->Width/Height equal the native
     *     sheet dims.
     *
     * Distinguish by comparing g->Width/Height against the native pc8
     * sheet. Larger ⇒ resized for display, take the aspect-fit scaled
     * path anchored at (x, y). Equal ⇒ native blit at (x, y). */
    int sheetW = war1_pc8_image_w_for(this);
    int sheetH = war1_pc8_image_h_for(this);
    if (sheetW > 0 && (Width > sheetW || Height > sheetH)) {
        if (war1_blit_via_pc8_scaled(this, x, y, Width, Height)) return;
    }
    war1_blit_via_pc8(this, 0, x, y);
}

/* ================================================================
 *  AI — real bodies in third_party/stratagus/src/ai/ (ported in).
 * ================================================================ */

/* ================================================================
 *  Video / rendering — real bodies come later in war1_video.cpp.
 *  Stubs here just satisfy the linker.
 * ================================================================ */
/* Alpha-blend 32bpp RGBA surface copy — used by fog-of-war to composite
 * its fog texture into TheScreen. Standard "over" blend: dst = src*a + dst*(1-a). */
void BlitSurfaceAlphaBlending_32bpp(SDL_Surface const *src, SDL_Rect const *srcrect,
                                    SDL_Surface *dst, SDL_Rect const *dstrect, bool) {
    if (!src || !dst || !src->pixels || !dst->pixels) return;
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
    if (w <= 0 || h <= 0) return;
    static int blit_probe = 0;
    if (blit_probe < 3) {
        blit_probe++;
        uart_puts("[BLIT] alpha src->w=");
        uart_putdec((unsigned)src->w);
        uart_puts(" dst->w=");
        uart_putdec((unsigned)dst->w);
        uart_puts(" dst->pixels=0x");
        uart_puthex((uint32_t)(uintptr_t)dst->pixels);
        uart_puts(" g_war1_back_fb=0x");
        uart_puthex(g_war1_back_fb);
        uart_puts(" rect=");
        uart_putdec((unsigned)dx); uart_puts(",");
        uart_putdec((unsigned)dy); uart_puts(",");
        uart_putdec((unsigned)w);  uart_puts("x");
        uart_putdec((unsigned)h);
        uart_puts("\n");
    }
    const int sstride = src->pitch / 4, dstride = dst->pitch / 4;
    const uint32_t *sp = (const uint32_t *)src->pixels;
    uint32_t       *dp = (uint32_t *)dst->pixels;
    for (int y = 0; y < h; y++) {
        const uint32_t *srow = sp + (sy + y) * sstride + sx;
        uint32_t       *drow = dp + (dy + y) * dstride + dx;
        for (int x = 0; x < w; x++) {
            uint32_t s = srow[x], d = drow[x];
            uint32_t a = (s >> 24) & 0xFF;
            if (a == 0)      { continue; }
            if (a == 255)    { drow[x] = s; continue; }
            uint32_t sr = (s >> 16) & 0xFF, sg = (s >> 8) & 0xFF, sb = s & 0xFF;
            uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            uint32_t ia = 255 - a;
            uint32_t r = (sr * a + dr * ia) / 255;
            uint32_t g = (sg * a + dg * ia) / 255;
            uint32_t b = (sb * a + db * ia) / 255;
            drow[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

/* CGraphic methods — declared in video.h, normally in graphic.cpp.
 * DrawFrameClip is the PRIMARY render entry point for Stratagus.
 * We route it through the pc8 bridge registered from war1_bridge.cpp.
 * Defining the destructor here also emits CGraphic's vtable. */
extern "C" void war1_blit_via_pc8(const CGraphic *g, unsigned frame, int x, int y);
void CGraphic::DrawFrameClip(unsigned int frame, int x, int y, SDL_Surface *) const {
    war1_blit_via_pc8(this, frame, x, y);
}
CGraphic::~CGraphic() = default;
void CGraphic::DrawFrameClipCustomMod(unsigned int, int, int, unsigned long (*)(unsigned long, unsigned long, unsigned long), unsigned long, SDL_Surface *) const {}
void CGraphic::DrawFrameClipTrans(unsigned int, int, int, int, SDL_Surface *) const {}
extern "C" void war1_blit_via_pc8_flipx(const CGraphic *g, unsigned frame, int x, int y);
/* Mirrored-frame draw — west-facing unit animations. */
void CGraphic::DrawFrameClipX(unsigned int frame, int x, int y, SDL_Surface *) const {
    war1_blit_via_pc8_flipx(this, frame, x, y);
}
/* Mirrored + alpha trans — only spell/ghost effects use this; same flip path,
 * alpha not yet honored (matches the non-flipped Trans variant). */
void CGraphic::DrawFrameClipTransX(unsigned int frame, int x, int y, int /*alpha*/, SDL_Surface *) const {
    war1_blit_via_pc8_flipx(this, frame, x, y);
}
extern "C" void war1_blit_subrect_via_pc8(const CGraphic *g,
                                          int gx, int gy, int w, int h,
                                          int x, int y);
void CGraphic::DrawSubClip(int gx, int gy, int w, int h, int x, int y, SDL_Surface *) const {
    /* One-shot probe: confirms UI.Fillers / button panel / icon paths
     * are actually reaching this entry point. If we never see "[HUD]
     * sub", the HUD just isn't asking for any draws. */
    static int probe_hit = 0;
    if (!probe_hit) {
        probe_hit = 1;
        uart_puts("[HUD] sub file='");
        uart_puts(this->File.c_str());
        uart_puts("' gw=");
        uart_putdec((unsigned)this->Width);
        uart_puts(" gh=");
        uart_putdec((unsigned)this->Height);
        uart_puts(" src=");
        uart_putdec((unsigned)gx); uart_puts(",");
        uart_putdec((unsigned)gy); uart_puts(",");
        uart_putdec((unsigned)w);  uart_puts(",");
        uart_putdec((unsigned)h);
        uart_puts(" dst=");
        uart_putdec((unsigned)x); uart_puts(",");
        uart_putdec((unsigned)y);
        uart_puts("\n");
    }
    war1_blit_subrect_via_pc8(this, gx, gy, w, h, x, y);
}
void CGraphic::Flip() {}
std::shared_ptr<CGraphic> CGraphic::ForceNew(std::string const &file, int w, int h) {
    return CGraphic::New(file, w, h);
}
void CGraphic::GenFramesMap() {}
void CGraphic::Load(bool) {}
void CGraphic::MakeShadow(Vec2T<int>) {}
/* Return a real (empty, not-yet-loaded) CGraphic so callers can dereference
 * ->Load() etc. without null-derefing. ImageFile stays empty; the adapter's
 * war1_blit_via_pc8 is routed by pointer identity in our PC8 registry, not
 * by inspecting the graphic's file field. */
extern "C" int war1_register_graphic_by_path(const CGraphic *g, const char *path);
/* Sentinel used to make CGraphic::IsLoaded() return true on bare-metal.
 * Upstream checks mSurface != nullptr to decide whether the graphic has
 * been loaded. We don't allocate SDL surfaces (rendering goes through
 * the pc8 blit bridge), so without this any CursorByIdent / filler /
 * icon lookup that gates on IsLoaded skips every entry. */
static SDL_Surface s_war1_dummy_surface = {};
std::shared_ptr<CGraphic> CGraphic::New(std::string const &file, int w, int h) {
    auto g = std::make_shared<CGraphic>();
    g->File = file;
    g->Width = w;
    g->Height = h;
    g->mSurface = &s_war1_dummy_surface;
    g->SurfaceFlip = &s_war1_dummy_surface;
    /* Try to bind to an embedded .pc8 blob. If path isn't in the catalog
     * (e.g. editor-only assets we didn't embed), the CGraphic stays empty
     * and DrawFrameClip is a no-op. */
    war1_register_graphic_by_path(g.get(), file.c_str());
    return g;
}
void CGraphic::Resize(int w, int h) {
    /* We don't actually rescale pixels (pc8 blob carries its own raw
     * dims and the blit paths sample from that). But Stratagus's HUD
     * code uses CGraphic::Width/Height as the logical frame size fed
     * to DrawSubClip — ui.lua's AddFiller does CGraphic:New(file) then
     * Resize(resize_x, resize_y), so Width/Height stay 0 without this. */
    this->Width = w;
    this->Height = h;
}

void CPlayerColorGraphic::DrawPlayerColorFrameClip(int /*colorIdx*/, unsigned int frame, int x, int y, SDL_Surface *) {
    /* Ignore player color for now — just route through the CGraphic path.
     * Team colors can later be implemented by palette-row swap. */
    war1_blit_via_pc8(this, frame, x, y);
}
void CPlayerColorGraphic::DrawPlayerColorFrameClipX(int /*colorIdx*/, unsigned int frame, int x, int y, SDL_Surface *) {
    /* Mirrored variant for west-facing unit animations. War1gus sprite
     * sheets only contain east-facing frames; the engine flips for W/NW/SW. */
    war1_blit_via_pc8_flipx(this, frame, x, y);
}
std::shared_ptr<CPlayerColorGraphic> CPlayerColorGraphic::New(std::string const &file, int w, int h) {
    auto g = std::make_shared<CPlayerColorGraphic>();
    g->File = file;
    g->Width = w;
    g->Height = h;
    g->mSurface = &s_war1_dummy_surface;
    g->SurfaceFlip = &s_war1_dummy_surface;
    war1_register_graphic_by_path(g.get(), file.c_str());
    return g;
}

void CVideo::DrawCircleClip(unsigned long, int, int, int) {}
void CVideo::DrawEllipseClip(unsigned long, int, int, int, int) {}
/* Horizontal/vertical line clipped to screen, painted to current back fb.
 * Used by DrawRectangleClip below and by various HUD frame ornaments. */
void CVideo::DrawHLineClip(unsigned long color, int x, int y, int w) {
    constexpr int SCREEN_W = 480;
    constexpr int SCREEN_H = 272;
    if (y < 0 || y >= SCREEN_H || w <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int x1 = x + w;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (x0 >= x1) return;
    uint32_t *row = (uint32_t *)g_war1_back_fb + y * SCREEN_W;
    uint32_t fill = (uint32_t)color;
    for (int i = x0; i < x1; i++) row[i] = fill;
}
void CVideo::DrawVLineClip(unsigned long color, int x, int y, int h) {
    constexpr int SCREEN_W = 480;
    constexpr int SCREEN_H = 272;
    if (x < 0 || x >= SCREEN_W || h <= 0) return;
    int y0 = y < 0 ? 0 : y;
    int y1 = y + h;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (y0 >= y1) return;
    uint32_t *fb = (uint32_t *)g_war1_back_fb;
    uint32_t fill = (uint32_t)color;
    for (int i = y0; i < y1; i++) fb[i * SCREEN_W + x] = fill;
}
void CVideo::DrawLineClip(unsigned long, Vec2T<int> const &, Vec2T<int> const &) {}
/* Draw an unfilled rectangle outline. DrawSelectionRectangle (the
 * default selection style set by stratagus.lua's `SetSelectionStyle("rectangle")`)
 * calls this to outline selected units in green. Empty stub previously
 * meant selection was invisible — Selected[] populates correctly, but
 * the user has no visual feedback so they think nothing was selected. */
void CVideo::DrawRectangleClip(unsigned long color, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    DrawHLineClip(color, x,         y,         w);
    DrawHLineClip(color, x,         y + h - 1, w);
    DrawVLineClip(color, x,         y,         h);
    DrawVLineClip(color, x + w - 1, y,         h);
}
void CVideo::DrawTransCircleClip(unsigned long, int, int, int, unsigned char) {}
/* Outlined rectangle with alpha blend onto the back fb. Used by
 * CMinimap::DrawViewportArea (the white box on the minimap that shows
 * where the camera is currently looking) — empty stub meant the
 * viewport indicator never appeared. Alpha-blends each border pixel
 * against the existing fb content so the minimap terrain underneath
 * still shows through faintly. */
void CVideo::DrawTransRectangle(unsigned long color, int x, int y, int w, int h, unsigned char alpha) {
    if (w <= 0 || h <= 0) return;
    constexpr int SCREEN_W = 480;
    constexpr int SCREEN_H = 272;
    auto blend_at = [&](int px, int py) {
        if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) return;
        uint32_t *p = (uint32_t *)g_war1_back_fb + py * SCREEN_W + px;
        uint32_t dst = *p;
        uint32_t a = alpha;
        uint32_t ia = 255 - a;
        uint32_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
        uint32_t dr = (dst   >> 16) & 0xFF, dg = (dst   >> 8) & 0xFF, db = dst   & 0xFF;
        uint32_t r = (sr * a + dr * ia) / 255;
        uint32_t g = (sg * a + dg * ia) / 255;
        uint32_t b = (sb * a + db * ia) / 255;
        *p = 0xFF000000u | (r << 16) | (g << 8) | b;
    };
    /* top + bottom edges */
    for (int i = 0; i < w; i++) {
        blend_at(x + i, y);
        if (h > 1) blend_at(x + i, y + h - 1);
    }
    /* left + right edges (skip the corners we already wrote) */
    for (int j = 1; j < h - 1; j++) {
        blend_at(x,         y + j);
        if (w > 1) blend_at(x + w - 1, y + j);
    }
}
void CVideo::FillCircleClip(unsigned long, Vec2T<int> const &, int) {}
/* Fill a rectangle of TheScreen (the current back buffer) with an ARGB
 * color. Stratagus calls this each frame at (0,0,Width,Height) with
 * ColorBlack to clear before redraw. dcache flush happens once per frame
 * in Invalidate() — no per-call flush needed now that we own the buffer
 * exclusively until flip. */
extern "C" void dcache_clean_range(uint32_t addr, uint32_t len);
void CVideo::FillRectangleClip(unsigned long color, int x, int y, int w, int h) {
    constexpr int SCREEN_W = 480;
    constexpr int SCREEN_H = 272;
    uint32_t *px = (uint32_t *)g_war1_back_fb;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W; if (y1 > SCREEN_H) y1 = SCREEN_H;
    uint32_t fill = (uint32_t)color;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = px + py * SCREEN_W;
        for (int dx = x0; dx < x1; dx++) row[dx] = fill;
    }
}
void CVideo::FillTransCircleClip(unsigned long, int, int, int, unsigned char) {}
void CVideo::FillTransRectangleClip(unsigned long, int, int, int, int, unsigned char) {}

/* remaining CViewport methods now in map/map_draw.cpp */

/* CButtonPanel / CInfoPanel / CStatusLine now in ported ui/*.cpp */
/* CUnitColors / CColor now provided by upstream video/color.cpp. */
/* CFont::Height / Width now in font-registry block below */
/* The header has the 3-arg CLabel inline (sets font + normal + reverse), but
 * the 1-arg ctor is declared out-of-line. Stratagus call sites (HUD content
 * types, message log, status line) construct CLabel with just a font and
 * expect default fg/bg colors. Without storing &f, font is uninitialized
 * garbage and label.Draw on empty text trips an indirect call through it. */
CLabel::CLabel(CFont const &f) : normal(nullptr), reverse(nullptr), font(&f) {}
int  CLabel::DrawClip(int x, int y, int n) const {
    war1_clabel_draw_int(font, x, y, n);
    return 0;
}

/* CParticleManager methods now in particle/particlemanager.cpp */

/* IconConfig::Load now in ui/icons.cpp */

/* Clipping */
void PushClipping() {}
void PopClipping() {}
void SetClipping(int, int, int, int) {}

/* Invalidate: called at the end of UpdateDisplay once the frame is fully
 * painted into the back buffer. Push the buffer to the UI0 layer, wait
 * for the scan-out to commit it, then swap front/back so the next frame's
 * draws land in the (now hidden) previous front.
 *
 * TheScreen->pixels re-points each flip so every upstream path that reads
 * dst->pixels (SDL_BlitSurface, BlitSurfaceAlphaBlending_32bpp used by the
 * fog composite, etc.) naturally targets the new back without any extra
 * per-call-site routing. */
extern "C" void video_set_overlay(uint32_t ovl_addr);
extern "C" void video_wait_vblank(void);
extern "C" void audio_mix(uint32_t num_samples);
extern "C" volatile uint32_t mix_wr, mix_rd;
extern "C" volatile uint32_t audio_underruns;
extern "C" int audio_pcm_channel_busy(uint32_t ch);
extern unsigned long FrameCounter;
/* mix_buf is 4096 slots. With the DAC now running at 11161 Hz (4.3×
 * lower than 48 kHz), each slot represents ~4.3× more audio time, so
 * 3800 slots = ~170 ms of headroom — plenty even for slow WC1 frames. */
#define WAR1_MIX_TARGET_DEPTH 3800
void Invalidate() {
    constexpr int SCREEN_W = 480;
    constexpr int SCREEN_H = 272;
    /* One flush per frame covers every write since the last flip. */
    dcache_clean_range(g_war1_back_fb, SCREEN_W * SCREEN_H * 4);
    /* Publish the buffer. The DE latches UI_ADDR at next vblank; wait so
     * we don't start writing this buffer again before hardware swap. */
    video_set_overlay(g_war1_back_fb);
    video_wait_vblank();
    /* Top up the PCM mixer until mix_buf has TARGET_DEPTH samples queued
     * (matches opn2_jupiter / mt32_rt). Fixed audio_mix(800) per frame
     * underruns whenever the displayed-frame rate dips below 60 Hz; the
     * depth-driven loop self-paces to whatever the ISR is actually
     * draining. Each iteration produces 32 stereo frames = 64 mix_buf
     * slots so we don't overshoot mix_buf in a single Invalidate. */
    while ((mix_wr - mix_rd) < WAR1_MIX_TARGET_DEPTH) {
        audio_mix(32);
    }
    static int aprobe = 0;
    if ((aprobe++ & 0x3F) == 0) {  /* once per ~64 frames ≈ 1 sec */
        int active = 0;
        for (int i = 0; i < 4; i++) if (audio_pcm_channel_busy(i)) active++;
        uart_puts("[AUDIO] mix tick under="); uart_putdec((unsigned)audio_underruns);
        uart_puts(" depth=");                 uart_putdec((unsigned)(mix_wr - mix_rd));
        uart_puts(" active=");                uart_putdec((unsigned)active);
        uart_puts("\n");
    }
    uint32_t tmp = g_war1_front_fb;
    g_war1_front_fb = g_war1_back_fb;
    g_war1_back_fb  = tmp;
    s_the_screen.pixels = (void *)g_war1_back_fb;
    /* Tick FrameCounter so SimpleChooseSample (sound.cpp:84) rotates
     * through sound-group variants — without this, FrameCounter stays
     * 0 forever and every sound group always returns its [0] entry
     * (e.g. footman always says "Yes", never "Reporting!" / etc.) */
    FrameCounter++;
}
void RealizeVideoMemory() {}
void SetVideoSync() {}
void VideoPaletteListRemove(SDL_Surface *) {}
/* Linear blend of two ARGB8888 colors. fraction=0 → color1, fraction=1 → color2.
 * Used by DrawUnitSelection (unit_draw.cpp:173) to color-grade the selection
 * rectangle by HP percentage when SelectionRectangleIndicatesDamage is on
 * (war1gus default): red at 0% HP, green at full. Empty stub returning 0
 * was making every selection rectangle fully-transparent black — the
 * drag-rect cursor passed ColorGreen directly so it stayed visibly green,
 * but the per-unit committed-selection rect hit this and disappeared. */
unsigned long InterpolateColor(unsigned long c1, unsigned long c2, float frac) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    const uint32_t a1 = (c1 >> 24) & 0xFF, r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    const uint32_t a2 = (c2 >> 24) & 0xFF, r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    const uint32_t a = (uint32_t)(a1 * (1.0f - frac) + a2 * frac);
    const uint32_t r = (uint32_t)(r1 * (1.0f - frac) + r2 * frac);
    const uint32_t g = (uint32_t)(g1 * (1.0f - frac) + g2 * frac);
    const uint32_t b = (uint32_t)(b1 * (1.0f - frac) + b2 * frac);
    return (a << 24) | (r << 16) | (g << 8) | b;
}
void ColorCycle() {}

CFont &GetGameFont() {
    /* MUST return a real registered CFont — the comment that said the
     * reference "is never read in game-logic paths we care about" was
     * wrong once CLabel::Draw started actually using the font (HUD
     * text rendering). The old placeholder was a 4-byte zero buffer
     * reinterpreted as CFont; calling any method on it dereferenced
     * a zero vtable and jumped to garbage — hung the game on first
     * DrawResources / status-line paint. Lazily wire to the "game"
     * font scripts/fonts.lua registers (CFont:New("game", ...) →
     * CFont::Get("game")). If fonts.lua hasn't run yet, fall back to
     * an empty CFont so the methods short-circuit cleanly. */
    auto *f = CFont::Get("game");
    if (!f) f = CFont::New("game", nullptr);
    return *f;
}

/* ================================================================
 *  Input / callbacks
 * ================================================================ */
/* Handle* now in ui/interface.cpp and ui/mouse.cpp */
/* WaitEventsOneFrame: per-frame input pump + pacing.
 * Upstream polls SDL events and dispatches to EventCallback methods.
 * On Jupiter we poll the N64 stick/buttons and map:
 *   - stick → cursor pos (accumulated, clamped to LCD)
 *   - A     → left mouse button
 *   - B     → right mouse button
 *   - 16 ms frame cap. */
extern "C" {
#include "input.h"
#include "jupiter.h"
}
/* jupiter.h's v3s.h defines `UI` as the GPIO base for the User Interface
 * controller — clashes with Stratagus's `UI` global (CUserInterface).
 * Drop the macro here so we can reach UI.MenuButton / UI.StatusLine. */
#undef UI
#include "widgets.h"  /* full LuaActionListener for ->action() */
#include "jupiter_input.hpp"  /* push mouse events into the guisan queue */
extern "C" uint32_t timer_read(void);
extern "C" uint32_t timer_elapsed(uint32_t start, uint32_t end);
extern "C" uint32_t ticks_to_us(uint32_t ticks);
extern "C" void war1_close_active_menu(void);
extern "C" int  war1_menu_click(int absX, int absY);
extern "C" volatile uint32_t g_war1_frame_counter = 0;
#include <guisan/gui.hpp>
extern std::unique_ptr<gcn::Gui> Gui;
extern "C" void war1_music_pump(void);

extern "C" void war1_music_pump_drain(void);
extern "C" int  war1_menu_stack_active(void);
void WaitEventsOneFrame() {
    g_war1_frame_counter++;
    war1_music_pump_drain();   /* fire any deferred music-finished callback first */
    war1_music_pump();          /* detect new natural end-of-track */
    static int cx = LCD_W / 2, cy = LCD_H / 2;
    static uint32_t last_held = 0;
    uint32_t t0 = timer_read();

    input_state_t st = input_poll();
    /* Stick: ±80 raw. Deadzone of 6, scale /4 → ~20 px/frame at full
     * deflection at 60 Hz = ~1.2 s across the 480-wide screen. Y is
     * inverted so up on the stick = up on screen. */
    if (st.stick_x > 6 || st.stick_x < -6) cx += st.stick_x / 4;
    if (st.stick_y > 6 || st.stick_y < -6) cy -= st.stick_y / 4;

    /* DPad fallback — same buttons the launcher menu uses successfully.
     * 4 px/frame, accelerated to 8 with L/R held. */
    uint32_t held_now = input_held();
    int step = (held_now & (BTN_L | BTN_R)) ? 8 : 4;
    if (held_now & BTN_LEFT)  cx -= step;
    if (held_now & BTN_RIGHT) cx += step;
    if (held_now & BTN_UP)    cy -= step;
    if (held_now & BTN_DOWN)  cy += step;

    if (cx < 0) cx = 0; if (cx >= (int)LCD_W) cx = LCD_W - 1;
    if (cy < 0) cy = 0; if (cy >= (int)LCD_H) cy = LCD_H - 1;

    /* When a guisan menu is active, route input ONLY through the guisan
     * queue. Upstream's MenuScreen::run swaps to GuichanCallbacks for
     * exactly this reason: the game's UIHandleMouseMove path does
     * viewport / button-area / unit-under-cursor work that isn't safe
     * with GamePaused=true + Gui top set, and was the source of the
     * apparent "cursor freeze" the moment the menu opened.
     *
     * BUT: HandleCursorMove (which updates CursorScreenPos so DrawCursor
     * paints the cursor at the new spot) is what upstream's
     * MenuHandleMouseMove (the GuichanCallbacks variant) calls too —
     * so we have to keep doing the equivalent. Inline it: just assign
     * CursorScreenPos. The cursor sprite then follows the stick/dpad
     * during a menu, while the heavy UIHandleMouseMove path is skipped. */
    /* StartMap (game.cpp:129) legitimately sets Gui->setTop(container) to
     * a placeholder Container so Gui->draw() has a top to walk during
     * gameplay (in-game HUD is drawn by Stratagus's own UpdateDisplay,
     * not via guichan). That placeholder is NEVER pushed onto MenuStack —
     * only real MenuScreen::run blocking calls push. So MenuStack.empty()
     * cleanly distinguishes real menu activity from the placeholder, and
     * keeps unit clicks routing to the game during a mission. */
    const bool menu_active = (::Gui && war1_menu_stack_active());
    const EventCallback *cb = GetCallbacks();
    if (cb && !menu_active) {
        /* Same MouseButtons-state-machine reasoning as for buttons: must
         * go through the InputMouse* wrappers so LastMousePos and the
         * "did we drag?" timing stay correct (interface.cpp:1396). */
        InputMouseMove(*cb, SDL_GetTicks(), cx, cy);
    } else if (menu_active) {
        /* Cursor-only update so the sprite tracks during a menu. */
        CursorScreenPos.x = cx;
        CursorScreenPos.y = cy;
    }
    /* TEMPORARILY DISABLED: pushing mouse moves to JupiterInput causes
     * the second Gui->logic() to hang. Bisecting whether it's in
     * handleMouseInput → handleMouseMoved → distributeMouseEvent
     * (which is what processes our queued moves) or in mTop->_logic
     * (widget-tree walk). If menu+cursor work after this disable,
     * the hang is in handleMouseMoved's dispatch chain. */
    /*
    if (gcn::JupiterInput *jin = gcn::getJupiterInput()) {
        jin->pushMouseMove(cx, cy);
    }
    */

    /* Button mapping — user request: B = left-click (select / move-target),
     * A = right-click (cancel / context). Stratagus uses SDL button IDs:
     *   1 = LMB, 2 = MMB, 3 = RMB.
     *
     * Critical: must go through InputMouseButtonPress / Release, NOT call
     * cb->ButtonPressed directly. The Input* helpers maintain the global
     * MouseButtons mask, LastMouseButton, MouseState, double-click timing,
     * etc. that UIHandleButtonDown reads (interface.cpp:1900,
     *   `MouseButtons & ((LeftButton << MouseHoldShift))`). Going around
     * them leaves MouseButtons=0, so UI sees "no button held" even though
     * the callback fired and the click does nothing.
     *
     * When a menu is active, click events go to guisan only; the game's
     * Input* wrappers stay quiet (otherwise widgets and game UI both
     * react to the same click). */
    uint32_t changed = held_now ^ last_held;
    /* Menu-active clicks bypass the cb-gated InputMouse* path entirely —
     * they go straight to war1_menu_click. The canonical title menu is
     * reached via stratagusMain → MenuLoop without anything having
     * SetCallbacks(&GuichanCallbacks) first (we don't compile upstream
     * widgets.cpp where that global lives), so cb is null and the cb-gated
     * branch below would never see BTN_B. Pull the menu-click path out so
     * the title menu's buttons fire regardless of cb state.
     *
     * Critical: write last_held BEFORE invoking the click callback. The
     * callback may recurse via Lua → menu:run() → INNER WaitEventsOneFrame.
     * If the user is still holding B when the inner loop starts, the
     * inner's `changed = held_now ^ last_held` would see last_held without
     * BTN_B (this function never reached the bottom-of-frame `last_held =
     * held_now` write before recursing) → it'd fire war1_menu_click again
     * on whatever button the cursor happens to be over in the new menu,
     * cascading menus open. Sync last_held now so the inner sees `changed
     * & BTN_B == 0` while B stays held. */
    if (menu_active && (changed & BTN_B) && (held_now & BTN_B)) {
        last_held = held_now;
        war1_menu_click(cx, cy);
    }
    if (cb) {
        const unsigned ticks = SDL_GetTicks();
        if (changed & BTN_B) {
            if (!menu_active) {
                if (held_now & BTN_B) InputMouseButtonPress(*cb,   ticks, 1);
                else                  InputMouseButtonRelease(*cb, ticks, 1);
                if (gcn::JupiterInput *jin = gcn::getJupiterInput()) {
                    jin->pushMouseButton(cx, cy, 1, (held_now & BTN_B) != 0);
                }
            }
            /* Menu-active branch handled above the `if (cb)` gate. */
        }
        if (changed & BTN_A) {
            if (!menu_active) {
                if (held_now & BTN_A) InputMouseButtonPress(*cb,   ticks, 3);
                else                  InputMouseButtonRelease(*cb, ticks, 3);
                if (gcn::JupiterInput *jin = gcn::getJupiterInput()) {
                    jin->pushMouseButton(cx, cy, 3, (held_now & BTN_A) != 0);
                }
            }
        }
        /* In-game menu trigger. The launcher confirms BTN_A is the only
         * non-DPad button reliably mapped in the user's QEMU. Anything
         * we'd dedicate to BTN_START / SELECT / shoulders just won't
         * fire. So accept ANY of these as the open-menu signal — the
         * mapping that exists in your setup will be the one that works:
         *   START, SELECT, L, R, Z, C_UP+C_DOWN simultaneously
         *   (last one is a deliberate two-button combo that won't
         *    accidentally fire from cursor scrolling). */
        const uint32_t menuBtnMask = BTN_START | BTN_SELECT | BTN_L | BTN_R | BTN_Z;
        bool menuTrig = (changed & menuBtnMask) && (held_now & menuBtnMask);
        bool comboTrig = ((held_now & (BTN_C_UP | BTN_C_DOWN)) == (BTN_C_UP | BTN_C_DOWN))
                      && !((last_held & (BTN_C_UP | BTN_C_DOWN)) == (BTN_C_UP | BTN_C_DOWN));
        if (menuTrig || comboTrig) {
            /* TOGGLE pause — until the menu actually renders something
             * the user can interact with, the only way out of the
             * paused state is pressing the menu button again. Don't
             * leave units frozen mid-move with no way back. */
            if (!IsNetworkGame()) {
                GamePaused = !GamePaused;
                UI.StatusLine.Set(GamePaused ? "Game Paused" : "Game Running");
            }
            uart_puts(GamePaused ? "[MENU] paused\n" : "[MENU] resumed\n");
            /* Only fire the menu callback on the PAUSE edge — resuming
             * via this same key shouldn't re-open it. Once real widget
             * rendering lands, this opens the visible menu. */
            if (GamePaused) {
                if (UI.MenuButton.Callback) {
                    UI.MenuButton.Callback->action(gcn::ActionEvent{nullptr, ""});
                } else if (Lua) {
                    if (luaL_dostring(Lua,
                            "local ok, err = pcall(RunGameMenu)\n"
                            "if not ok then print('[MENU] RunGameMenu err: ' .. tostring(err)) end\n"
                            ) != LUA_OK) {
                        uart_puts("[MENU] RunGameMenu dostring failed: ");
                        uart_puts(lua_tostring(Lua, -1));
                        uart_puts("\n");
                        lua_pop(Lua, 1);
                    }
                }
            } else {
                /* Resume edge: tear the menu down so it stops rendering
                 * over the game. Otherwise Gui->getTop() stays on the
                 * MenuScreen and DrawGuichanWidgets keeps painting it. */
                war1_close_active_menu();
            }
        }
    }
    /* C-buttons → secondary map scroll. The N64 stick scrolls the cursor
     * (which can drag-edge-scroll the map at the screen edges) but for
     * deliberate map panning the C-cluster is more natural. We set the
     * KeyScrollState bits each frame they're held; DoScrollArea (called
     * from DisplayLoop) reads `MouseScrollState | KeyScrollState` and
     * advances the viewport accordingly. */
    KeyScrollState = ScrollNone;
    if (held_now & BTN_C_UP)    KeyScrollState |= ScrollUp;
    if (held_now & BTN_C_DOWN)  KeyScrollState |= ScrollDown;
    if (held_now & BTN_C_LEFT)  KeyScrollState |= ScrollLeft;
    if (held_now & BTN_C_RIGHT) KeyScrollState |= ScrollRight;
    last_held = held_now;

    /* 60 fps cap — without this, GameMainLoop spins flat-out. */
    while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667);
}
/* DrawCursor now in video/cursor.cpp */

static EventCallback *g_callbacks = nullptr;
void SetCallbacks(const EventCallback *cb) { g_callbacks = const_cast<EventCallback *>(cb); }
const EventCallback *GetCallbacks() { return g_callbacks; }

/* ================================================================
 *  UI / HUD — no-ops until we have a real HUD
 * ================================================================ */
/* CancelBuildingMode / DrawPieMenu now in ui/mouse.cpp
 * CheckViewportMode now in ui/ui.cpp
 * DrawMenuButtonArea / DrawMessages / DrawResources / DrawTimer /
 * DrawUserDefinedButtons / UpdateMessages / UpdateTimer now in ui/mainscr.cpp
 * IsButtonAllowed now in ui/botpanel.cpp
 * DrawGuichanWidgets now in war1_widgets.cpp */
/* SelectedUnitChanged / SelectionChanged now defined by script_ui.cpp */
/* SetMessage / SetMessageEvent now in ui/mainscr.cpp
 * ShowLoadProgress now in ui/ui.cpp
 * UiToggleBigMap now in ui/interface.cpp */
/* Header declares `const std::string &Translate(const std::string &)`. The
 * no-op identity translator just returns the argument's reference — the
 * caller's string outlives the full expression, so .c_str() on our return
 * value is safe. */
/* Translate now in third_party/stratagus/src/stratagus/translate.cpp */

/* ================================================================
 *  Sound — minimal adapter. A real CSound→PCM blob map isn't wired
 *  yet, so every play call fires a short placeholder blip. Replacing
 *  the blob with real .wav PCM will happen alongside the asset
 *  pipeline extension (war1_convert.py WAV support).
 * ================================================================ */
extern "C" void audio_pcm_play(uint32_t channel, const int16_t *samples,
                               uint32_t length, uint8_t volume, uint8_t loop);

/* Short placeholder beep: zero-initialized, will sound like nothing until
 * we bind real sample data. Proves the plumbing without needing assets. */
[[maybe_unused]] static const int16_t s_beep_pcm[512] = {0};

/* PlayGameSound / PlayMissileSound / PlayUnitSound overloads /
 * SoundConfig::MapSound / VolumeForDistance now in sound/ ported files */

/* ================================================================
 *  Network — skipped. Replay/Save/Trigger now in game/ dir.
 * ================================================================ */
void NetworkCommands() {}
void NetworkEvent() {}
void NetworkQuitGame() {}
void NetworkRecover() {}
void NetworkSendSelection(CUnit **, int) {}
void SendCommandDismiss(CUnit &) {}

/* Lua helpers are now defined in the restored script_*.cpp files. */

/* ================================================================
 *  Newlib syscall stubs. Cortex-A7 bare-metal has no OS, so every
 *  syscall returns an error. Jupiter libc_shim may already cover some
 *  of these — the ones it provides will win the link.
 * ================================================================ */
extern "C" {
/* ================================================================
 *  In-memory filesystem backed by objcopy'd Lua script blobs.
 *  newlib's fopen/fread/fclose/fseek/fstat route here via _open/...
 *  so Stratagus's normal Load()/dofile() paths find embedded scripts.
 * ================================================================ */
} /* extern "C" */

#include "../../examples/war1/war1_script_catalog.h"
#include "ff.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
extern "C" void uart_puts(const char *);
extern "C" void uart_putdec(uint32_t);
#define WAR1_MAX_FDS 16

enum war1_fd_kind {
    WAR1_FD_NONE = 0,
    WAR1_FD_BLOB,    /* objcopy'd Lua/asset blob (catalog) */
    WAR1_FD_FATFS,   /* file open on the FAT partition via FatFs */
};

struct war1_fd_state {
    int used;
    war1_fd_kind kind;
    /* Blob-backed fields */
    const uint8_t *start;
    const uint8_t *end;
    unsigned long pos;
    /* FatFs-backed fields */
    FIL *fil;
};
static war1_fd_state g_war1_fds[WAR1_MAX_FDS];
/* Reserve fd 0/1/2 for stdin/stdout/stderr — never hand those out. */

/* The 0:/ FAT partition is mounted lazily on first fopen. f_mount(force=1)
 * triggers init right then; subsequent calls are no-ops. */
static int war1_fatfs_mounted = 0;
static FATFS war1_fatfs;

static int war1_fatfs_ensure_mounted(void) {
    if (war1_fatfs_mounted) return 0;
    FRESULT r = f_mount(&war1_fatfs, "0:", 1);
    if (r != FR_OK) {
        uart_puts("[fatfs] mount failed rc="); uart_putdec((uint32_t)r); uart_puts("\n");
        return -1;
    }
    war1_fatfs_mounted = 1;
    return 0;
}

/* Is this a path FatFs should handle?
 *  - "0:..." is the explicit FatFs drive prefix
 *  - "/..." is a POSIX absolute path; we route those to FatFs under
 *    drive 0 (libstdc++'s fs::* doesn't understand drive letters, so
 *    Stratagus's user-dir comes in as "/wc1" not "0:/wc1").
 * Catalog (embedded blobs) is for relative paths like "scripts/X.lua". */
static int war1_is_fatfs_path(const char *p) {
    if (!p) return 0;
    if (p[0] == '0' && p[1] == ':') return 1;
    if (p[0] == '/') return 1;
    return 0;
}

/* Translate a path to a FatFs path under drive "0:". POSIX-rooted paths
 * "/X" become "0:/X"; "0:/..." paths pass through. Returns a pointer
 * into a static buffer good until the next call. */
static const char *war1_fatfs_path(const char *p) {
    if (!p) return nullptr;
    if (p[0] == '0' && p[1] == ':') return p;  /* already FatFs */
    static char buf[260];
    /* "/X" → "0:/X" — same length + 2 prefix bytes. */
    int n = (int)strlen(p);
    if (n + 3 >= (int)sizeof(buf)) return nullptr;  /* too long */
    buf[0] = '0'; buf[1] = ':';
    memcpy(buf + 2, p, (size_t)n + 1);
    return buf;
}

static int war1_alloc_fd_blob(const uint8_t *s, const uint8_t *e) {
    for (int i = 3; i < WAR1_MAX_FDS; i++) {
        if (!g_war1_fds[i].used) {
            g_war1_fds[i].used  = 1;
            g_war1_fds[i].kind  = WAR1_FD_BLOB;
            g_war1_fds[i].start = s;
            g_war1_fds[i].end   = e;
            g_war1_fds[i].pos   = 0;
            g_war1_fds[i].fil   = nullptr;
            return i;
        }
    }
    return -1;
}

static int war1_alloc_fd_fatfs(FIL *fp) {
    for (int i = 3; i < WAR1_MAX_FDS; i++) {
        if (!g_war1_fds[i].used) {
            g_war1_fds[i].used  = 1;
            g_war1_fds[i].kind  = WAR1_FD_FATFS;
            g_war1_fds[i].start = nullptr;
            g_war1_fds[i].end   = nullptr;
            g_war1_fds[i].pos   = 0;
            g_war1_fds[i].fil   = fp;
            return i;
        }
    }
    return -1;
}

/* Match a requested path against the catalog. Tries variant with and without
 * the "scripts/" prefix so Stratagus callers with either shape find it. */
static const war1_script_entry_t *war1_script_lookup(const char *path) {
    if (!path) return nullptr;
    for (int i = 0; i < WAR1_SCRIPT_COUNT; i++) {
        if (strcmp(WAR1_SCRIPTS[i].path, path) == 0) return &WAR1_SCRIPTS[i];
        /* also try without "scripts/" prefix */
        if (strncmp(WAR1_SCRIPTS[i].path, "scripts/", 8) == 0 &&
            strcmp(WAR1_SCRIPTS[i].path + 8, path) == 0) return &WAR1_SCRIPTS[i];
    }
    return nullptr;
}

extern "C" {

int _close(int fd) {
    if (fd < 3 || fd >= WAR1_MAX_FDS) return 0;
    war1_fd_state &s = g_war1_fds[fd];
    if (s.kind == WAR1_FD_FATFS && s.fil) {
        f_close(s.fil);
        free(s.fil);
        s.fil = nullptr;
    }
    s.used = 0;
    s.kind = WAR1_FD_NONE;
    return 0;
}
void _exit(int)                                    { for(;;); }
int _fstat(int fd, struct stat *st) {
    if (!st) return -1;
    memset(st, 0, sizeof(*st));
    if (fd >= 3 && fd < WAR1_MAX_FDS && g_war1_fds[fd].used) {
        const war1_fd_state &s = g_war1_fds[fd];
        if (s.kind == WAR1_FD_BLOB) {
            st->st_mode = S_IFREG;
            st->st_size = (off_t)(s.end - s.start);
            return 0;
        }
        if (s.kind == WAR1_FD_FATFS && s.fil) {
            st->st_mode = S_IFREG;
            st->st_size = (off_t)f_size(s.fil);
            return 0;
        }
    }
    st->st_mode = S_IFCHR;  /* pretend stdin/out/err are char devs */
    return 0;
}
int _getpid(void)                                  { return 1; }
int _gettimeofday(void *, void *)                  { return 0; }
int _isatty(int fd)                                { return fd < 3 ? 1 : 0; }
int _kill(int, int)                                { return -1; }
int _link(const char *, const char *)              { return -1; }
long _lseek(int fd, long offset, int whence) {
    if (fd < 3 || fd >= WAR1_MAX_FDS || !g_war1_fds[fd].used) return -1;
    war1_fd_state &s = g_war1_fds[fd];
    if (s.kind == WAR1_FD_BLOB) {
        unsigned long size = (unsigned long)(s.end - s.start);
        unsigned long newpos;
        switch (whence) {
            case 0: newpos = (unsigned long)offset; break;
            case 1: newpos = s.pos + (unsigned long)offset; break;
            case 2: newpos = size + (unsigned long)offset; break;
            default: return -1;
        }
        if (newpos > size) newpos = size;
        s.pos = newpos;
        return (long)newpos;
    }
    if (s.kind == WAR1_FD_FATFS && s.fil) {
        FSIZE_t cur  = f_tell(s.fil);
        FSIZE_t size = f_size(s.fil);
        FSIZE_t target;
        switch (whence) {
            case 0: target = (FSIZE_t)offset; break;
            case 1: target = cur + (FSIZE_t)offset; break;
            case 2: target = size + (FSIZE_t)offset; break;
            default: return -1;
        }
        FRESULT r = f_lseek(s.fil, target);
        if (r != FR_OK) return -1;
        return (long)f_tell(s.fil);
    }
    return -1;
}
int _open(const char *path, int flags, ...) {
    if (war1_is_fatfs_path(path)) {
        if (war1_fatfs_ensure_mounted() != 0) return -1;
        const char *fpath = war1_fatfs_path(path);
        if (!fpath) { errno = ENAMETOOLONG; return -1; }
        BYTE mode = 0;
        int access = flags & O_ACCMODE;
        if (access == O_RDONLY) {
            mode = FA_READ | FA_OPEN_EXISTING;
        } else {
            /* WRONLY / RDWR — newlib's fopen("wb"/"w+b") sets O_CREAT|O_TRUNC. */
            mode = (access == O_WRONLY) ? FA_WRITE : (FA_READ | FA_WRITE);
            if (flags & O_TRUNC)  mode |= FA_CREATE_ALWAYS;
            else if (flags & O_CREAT) mode |= FA_OPEN_ALWAYS;
            else                  mode |= FA_OPEN_EXISTING;
            if (flags & O_APPEND) mode |= FA_OPEN_APPEND;
        }
        FIL *fp = (FIL *)malloc(sizeof(FIL));
        if (!fp) return -1;
        FRESULT r = f_open(fp, fpath, mode);
        if (r != FR_OK) {
            free(fp);
            errno = (r == FR_NO_FILE || r == FR_NO_PATH) ? ENOENT : EIO;
            return -1;
        }
        int fd = war1_alloc_fd_fatfs(fp);
        if (fd < 0) {
            f_close(fp); free(fp);
            return -1;
        }
        return fd;
    }

    const war1_script_entry_t *e = war1_script_lookup(path);
    if (!e) {
        uart_puts("[fs] _open MISS: "); uart_puts(path ? path : "(null)"); uart_puts("\n");
        errno = ENOENT;
        return -1;
    }
    int fd = war1_alloc_fd_blob(e->data, e->data_end);
    if (fd < 0) { uart_puts("[fs] _open: no free fd\n"); return -1; }
    uart_puts("[fs] _open HIT: "); uart_puts(path);
    uart_puts(" fd="); uart_putdec((uint32_t)fd); uart_puts("\n");
    return fd;
}
long _read(int fd, void *buf, unsigned long len) {
    if (fd < 3 || fd >= WAR1_MAX_FDS || !g_war1_fds[fd].used) return -1;
    war1_fd_state &s = g_war1_fds[fd];
    if (s.kind == WAR1_FD_BLOB) {
        unsigned long size = (unsigned long)(s.end - s.start);
        unsigned long avail = size > s.pos ? size - s.pos : 0;
        if (len > avail) len = avail;
        memcpy(buf, s.start + s.pos, len);
        s.pos += len;
        return (long)len;
    }
    if (s.kind == WAR1_FD_FATFS && s.fil) {
        UINT got = 0;
        FRESULT r = f_read(s.fil, buf, (UINT)len, &got);
        if (r != FR_OK) { errno = EIO; return -1; }
        return (long)got;
    }
    return -1;
}

/* _sbrk: our libc_shim's malloc wins over newlib's, so _sbrk never actually
 * fires at runtime. Provide a small 64 KB fallback arena just in case some
 * newlib code path bypasses us. Keeps BSS lean. */
#define WAR1_HEAP_SIZE (64 * 1024)
static unsigned char war1_heap[WAR1_HEAP_SIZE] __attribute__((aligned(16)));
static unsigned long war1_heap_used = 0;
void *_sbrk(int incr) {
    if (incr < 0) return (void *)-1;
    if (war1_heap_used + (unsigned long)incr > WAR1_HEAP_SIZE) return (void *)-1;
    void *prev = &war1_heap[war1_heap_used];
    war1_heap_used += (unsigned long)incr;
    return prev;
}

/* _stat: return the real size/mode for files in our embedded catalog,
 * pretend-directory-exists for paths without a file-like extension (so
 * fs::create_directories("wc1") / "foo" succeeds without looping into
 * _mkdir), and ENOENT for everything else (so Stratagus's
 * LoadStratagusMap candidate loop finds the right one instead of
 * picking the first — every candidate returning "exists" was the bug). */
static int path_has_file_extension(const char *p) {
    const char *dot = nullptr;
    for (const char *s = p; *s; s++) {
        if (*s == '.') dot = s;
        if (*s == '/' || *s == '\\') dot = nullptr;  /* reset at dir separator */
    }
    if (!dot || !dot[1]) return 0;
    /* extensions we treat as files; anything else is a directory candidate */
    static const char *exts[] = { ".lua", ".sms", ".smp", ".png", ".wav",
                                  ".gz", ".bz2", ".txt", ".ogv", ".sf2", nullptr };
    for (int i = 0; exts[i]; i++) {
        if (strcmp(dot, exts[i]) == 0) return 1;
    }
    return 0;
}
/* Treat `path` as a directory iff some catalog entry lives below it. This
 * has to be a real check — fs::is_directory("./videos") returning true on
 * an empty platform sends ReadDataDirectory into directory_iterator which
 * then throws when _opendir misses. The cinematics/briefings load chain
 * (file_exists("videos", ...) in stratagus.lua) relies on the lookup
 * failing cleanly when the assets aren't shipped. */
static int war1_dir_has_entries(const char *path) {
    if (!path || !*path) return 0;
    size_t plen = strlen(path);
    /* Trim trailing slash so "videos" and "videos/" both match. */
    while (plen > 0 && (path[plen - 1] == '/' || path[plen - 1] == '\\')) plen--;
    if (plen == 0) return 1;  /* root of our virtual FS — always a dir */
    for (int i = 0; i < WAR1_SCRIPT_COUNT; i++) {
        const char *e = WAR1_SCRIPTS[i].path;
        if (strncmp(e, path, plen) == 0 && (e[plen] == '/' || e[plen] == '\\')) return 1;
        /* also accept catalog entries that start with "scripts/" prefix
         * stripped — the lookup function handles this duality too. */
        if (strncmp(e, "scripts/", 8) == 0) {
            const char *r = e + 8;
            if (strncmp(r, path, plen) == 0 && (r[plen] == '/' || r[plen] == '\\')) return 1;
        }
    }
    return 0;
}
int _stat(const char *path, struct stat *st) {
    if (!st || !path) { if (st) memset(st, 0, sizeof(*st)); errno = ENOENT; return -1; }
    memset(st, 0, sizeof(*st));
    if (war1_is_fatfs_path(path)) {
        if (war1_fatfs_ensure_mounted() != 0) { errno = ENOENT; return -1; }
        const char *fpath = war1_fatfs_path(path);
        if (!fpath) { errno = ENAMETOOLONG; return -1; }
        FILINFO fi;
        FRESULT r = f_stat(fpath, &fi);
        if (r == FR_OK) {
            st->st_mode = (fi.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
            st->st_size = (off_t)fi.fsize;
            return 0;
        }
        /* Volume root: "0:", "0:/" — treat as existing directory. FatFs's
         * f_stat doesn't return an entry for the root itself; for
         * fs::create_directories which calls stat on each parent
         * component, we need this to succeed. */
        size_t plen = strlen(fpath);
        if ((plen == 2 && fpath[0] == '0' && fpath[1] == ':') ||
            (plen == 3 && fpath[0] == '0' && fpath[1] == ':' && fpath[2] == '/')) {
            st->st_mode = S_IFDIR;
            return 0;
        }
        errno = ENOENT;
        return -1;
    }
    const war1_script_entry_t *e = war1_script_lookup(path);
    if (e) {
        st->st_mode = S_IFREG;
        st->st_size = (off_t)(e->data_end - e->data);
        return 0;
    }
    if (path_has_file_extension(path)) {
        errno = ENOENT;
        return -1;
    }
    if (war1_dir_has_entries(path)) {
        st->st_mode = S_IFDIR;
        return 0;
    }
    /* Path the user-directory machinery created (e.g. "/wc1") — pretend
     * it exists as a dir so create_directories / freopen-target paths
     * stay quiet, but it has no enumerable entries. */
    if (path[0] == '/') {
        st->st_mode = S_IFDIR;
        return 0;
    }
    errno = ENOENT;
    return -1;
}
int _unlink(const char *path) {
    if (war1_is_fatfs_path(path)) {
        if (war1_fatfs_ensure_mounted() != 0) return -1;
        const char *fpath = war1_fatfs_path(path);
        if (!fpath) return -1;
        FRESULT r = f_unlink(fpath);
        return (r == FR_OK) ? 0 : -1;
    }
    return -1;
}

/* Stratagus's CclFilteredListDirectory("~save") asks for the contents of
 * the save dir. Walk 0:/save/ with FatFs and return up to `max` entries
 * (".gz" suffix stripped so menu names round-trip with what was saved).
 *
 * Auto-creates 0:/save/ if it doesn't exist yet. */
int war1_list_save_dir(char (*out)[64], int max)
{
    if (war1_fatfs_ensure_mounted() != 0) return 0;
    f_mkdir("0:/save");

    DIR dir;
    FILINFO fi;
    FRESULT r = f_opendir(&dir, "0:/save");
    if (r != FR_OK) return 0;
    int n = 0;
    while (n < max) {
        r = f_readdir(&dir, &fi);
        if (r != FR_OK || fi.fname[0] == 0) break;
        if (fi.fattrib & AM_DIR) continue;
        const char *name = fi.fname;
        size_t len = strlen(name);
        size_t copy_len = len;
        if (copy_len > 3 && strcmp(name + copy_len - 3, ".gz") == 0) {
            copy_len -= 3;
        }
        if (copy_len >= 64) copy_len = 63;
        memcpy(out[n], name, copy_len);
        out[n][copy_len] = 0;
        n++;
    }
    f_closedir(&dir);
    return n;
}
/* _write: Lua's default io may fallthrough here; newlib calls it with
 * fd=1 (stdout) or fd=2 (stderr). Route those to UART so any late println
 * still surfaces. FatFs-backed fds go through f_write. Everything else
 * (catalog blobs are read-only) drops silently. */
long _write(int fd, const void *buf, unsigned long len) {
    if ((fd == 1 || fd == 2) && buf && len) {
        const char *p = (const char *)buf;
        for (unsigned long i = 0; i < len; i++) {
            char c = p[i];
            char tmp[2] = { c, 0 };
            uart_puts(tmp);
        }
        return (long)len;
    }
    if (fd >= 3 && fd < WAR1_MAX_FDS && g_war1_fds[fd].used) {
        war1_fd_state &s = g_war1_fds[fd];
        if (s.kind == WAR1_FD_FATFS && s.fil) {
            UINT wrote = 0;
            FRESULT r = f_write(s.fil, buf, (UINT)len, &wrote);
            if (r != FR_OK) { errno = EIO; return -1; }
            return (long)wrote;
        }
    }
    return -1;
}
int truncate(const char *, long)                   { return -1; }
int _mkdir(const char *path, unsigned int)
{
    if (war1_is_fatfs_path(path)) {
        if (war1_fatfs_ensure_mounted() != 0) { errno = EIO; return -1; }
        const char *fpath = war1_fatfs_path(path);
        if (!fpath) { errno = ENAMETOOLONG; return -1; }
        /* Volume root ("0:", "0:/") always exists; report success so
         * fs::create_directories can walk past the root component. */
        size_t plen = strlen(fpath);
        if ((plen == 2 && fpath[0] == '0' && fpath[1] == ':') ||
            (plen == 3 && fpath[0] == '0' && fpath[1] == ':' && fpath[2] == '/')) {
            return 0;
        }
        FRESULT r = f_mkdir(fpath);
        if (r == FR_OK || r == FR_EXIST) return 0;
        errno = EIO;
        return -1;
    }
    return 0;  /* pretend success for in-memory tree (catalog blobs) */
}
int mkdir(const char *path, mode_t)
{
    return _mkdir(path, 0);
}
long _times(void *)                                 { return 0; }
int execvp(const char *, char *const [])            { return -1; /* no process restart on bare-metal */ }
/* newlib's crt calls _fini to run .fini_array destructors. We don't have any
 * meaningful cleanup at shutdown. */
void _fini(void)                                    { }
void _init(void)                                    { }

} /* extern "C" */

/* libc: now linking newlib (-lc) for WAR1 builds. All the hand-rolled stubs
 * that used to live here (strtoul, wmemcpy, locale, wcslen, mbrtowc, etc.)
 * are REMOVED — newlib provides them. The handful of things newlib doesn't
 * cover: */
extern "C" {
/* __dso_handle is a C++-ABI symbol usually emitted by crt1.o, which we
 * don't link. Providing a dummy keeps the __cxa_atexit-style registrations
 * happy without side effects. */
void *__dso_handle = nullptr;
int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
int __xpg_strerror_r(int errnum, char *, size_t) { return errnum; }

/* Intercept std::terminate — on -fno-exceptions libstdc++ abort path this
 * ends up being called for any throw. Log heap + freeze so we can see the
 * state instead of the uninformative "terminate called ...". */
extern "C" uint32_t libc_shim_heap_used(void);
extern "C" uint32_t libc_shim_heap_size(void);
} /* close prior extern "C" so the includes can compile */
#include <system_error>
#include <cstring>
extern "C" {
/* --wrap=__cxa_throw: log + forward to the real __cxa_throw. Freezing here
 * breaks static-init ctors that throw catchable exceptions internally. */
struct war1_type_info_peek { const char *name; };
extern "C" [[noreturn]] void __real___cxa_throw(void *, void *, void (*)(void *));
void __wrap___cxa_throw(void *thrown, void *tinfo, void (*dtor)(void *)) {
    uart_puts("[CXA] throw type=");
    const char *tn = "?";
    if (tinfo) {
        war1_type_info_peek *ti = (war1_type_info_peek *)((char *)tinfo + 4);
        if (ti->name) tn = ti->name;
    }
    uart_puts(tn);
    /* If this is an fs::filesystem_error, peek at .what() and the two paths
     * the constructor stored. The class layout (libstdc++, GCC ARM ABI):
     *   [vptr][std::string what][error_code][fs::path p1][fs::path p2]
     * what() is a std::string SSO buffer; tolerate either local or heap. */
    if (thrown && tn && strstr(tn, "filesystem_error")) {
        std::system_error *se = reinterpret_cast<std::system_error *>(thrown);
        const char *msg = se->what();
        uart_puts(" what=");
        uart_puts(msg ? msg : "(null)");
    }
    uart_puts(" heap_used=");
    uart_putdec(libc_shim_heap_used());
    uart_puts("\n");
    __real___cxa_throw(thrown, tinfo, dtor);
}
} /* extern "C" */

/* Wrap libstdc++'s fs::absolute. arm-none-eabi's impl has no getcwd
 * syscall, so relative paths come back with ec=ENOTRECOVERABLE — which the
 * throwing overload then turns into a filesystem_error throw. Stratagus's
 * script.cpp LuaLoadFile calls fs::absolute(file).generic_u8string() on
 * every Load() just to set __file__ for diagnostics. We don't need a real
 * cwd — return the input path unchanged. */
#include <filesystem>
namespace fs = std::filesystem;
/* Forward decls: original symbols renamed to __real_* by the linker. */
extern "C" fs::path *__real__ZNSt10filesystem8absoluteERKNS_7__cxx114pathE(fs::path *, const fs::path &);
extern "C" fs::path *__real__ZNSt10filesystem8absoluteERKNS_7__cxx114pathERSt10error_code(fs::path *, const fs::path &, std::error_code &);
/* GCC ABI: return-by-value through hidden pointer — first arg is the
 * out-ptr, rest are the declared args. We just copy `p` into *ret. */
extern "C" fs::path *__wrap__ZNSt10filesystem8absoluteERKNS_7__cxx114pathE(fs::path *ret, const fs::path &p) {
    new (ret) fs::path(p);
    return ret;
}
extern "C" fs::path *__wrap__ZNSt10filesystem8absoluteERKNS_7__cxx114pathERSt10error_code(fs::path *ret, const fs::path &p, std::error_code &ec) {
    ec.clear();
    new (ret) fs::path(p);
    return ret;
}

/* Wrap fs::create_directories. arm-none-eabi libstdc++'s implementation
 * fails on our build before even reaching our _stat/_mkdir syscalls
 * (likely calls some posix syscall variant we don't provide). Walk the
 * path ourselves and call our _mkdir per component. */
extern "C" int _mkdir(const char *path, unsigned int mode);
static bool war1_create_directories(const std::string &s) {
    if (s.empty()) return false;
    char buf[260];
    int len = 0;
    int i = 0;
    /* Preserve leading '/' or "0:/" prefix without mkdir-ing empty prefix. */
    if (s[0] == '/') { buf[len++] = '/'; i = 1; }
    else if (s.size() >= 2 && s[0] == '0' && s[1] == ':') {
        buf[len++] = '0'; buf[len++] = ':';
        i = 2;
        if (i < (int)s.size() && s[i] == '/') { buf[len++] = '/'; i++; }
    }
    while (i < (int)s.size() && len < (int)sizeof(buf) - 2) {
        char c = s[i++];
        if (c == '/') {
            buf[len] = 0;
            (void)_mkdir(buf, 0);   /* ignore intermediate failures */
            buf[len++] = '/';
        } else {
            buf[len++] = c;
        }
    }
    buf[len] = 0;
    if (len == 0) return false;
    return _mkdir(buf, 0) == 0;
}
extern "C" bool __wrap__ZNSt10filesystem18create_directoriesERKNS_7__cxx114pathE(const fs::path &p) {
    return war1_create_directories(p.string());
}
extern "C" bool __wrap__ZNSt10filesystem18create_directoriesERKNS_7__cxx114pathERSt10error_code(const fs::path &p, std::error_code &ec) {
    ec.clear();
    return war1_create_directories(p.string());
}
/* __aeabi_d2lz lives in lib/libc_shim.c next to __aeabi_l2d / __aeabi_ul2d
 * with a real IEEE 754 bit-extraction implementation. The previous stub
 * here was `return (long long)d` which compiles to a recursive call to
 * itself (the cast IS __aeabi_d2lz on this toolchain) → stack overflow
 * if it's ever hit, or the compiler silently uses libgcc's broken
 * version. */

/* vtable for CAnimation_LuaCallback — the class is declared in
 * include/animation/animation_luacallback.h but we deleted its .cpp.
 * Provide the virtual methods to emit its vtable. */
/* CAnimation_LuaCallback methods now in animation_luacallback.cpp. */

/* CBoolKeys/CVariableKeys/CAnimation_LuaCallback ctors now in script_unittype.cpp. */
/* CUserInterface ctor/dtor now in ui/ui.cpp */

/* ================================================================
 *  Stubs for files we didn't port (video/color, video/graphic,
 *  video/font, ui/icons, sound/sound, stratagus/stratagus) — the game
 *  module registers Lua bindings that reference these at link time
 *  but the bindings are no-ops at runtime (scripts that actually use
 *  them will see nil/default values).
 * ================================================================ */
#include "color.h"
#include "font.h"
#include "icons.h"
#include "sound.h"
#include "unittype.h"  /* for EnumVariable */
#include "commands.h"  /* for EDiplomacy */
#include "network.h"   /* for Hosts */
#include "netconnect.h"
/* CColor::Parse / operator SDL_Color now provided by upstream video/color.cpp. */

/* CFont / CFontColor — real-enough bodies (upstream font.cpp is SDL-heavy
 * and brings libttf etc., so we port just the registry-map parts). Scripts
 * only need CFontColor::New to return a non-null usable object whose
 * .Colors array they can fill. */
static std::map<std::string, std::unique_ptr<CFont>, std::less<>> &war1_font_map() {
    static std::map<std::string, std::unique_ptr<CFont>, std::less<>> m;
    return m;
}
static std::map<std::string, std::unique_ptr<CFontColor>, std::less<>> &war1_fontcolor_map() {
    static std::map<std::string, std::unique_ptr<CFontColor>, std::less<>> m;
    return m;
}
CFont *CFont::Get(std::string_view ident) {
    auto it = war1_font_map().find(std::string(ident));
    return it == war1_font_map().end() ? nullptr : it->second.get();
}
CFont *CFont::New(std::string const &ident, std::shared_ptr<CGraphic> g) {
    auto &slot = war1_font_map()[ident];
    if (!slot) slot = std::unique_ptr<CFont>(new CFont(ident));
    slot->G = g;
    return slot.get();
}
/* CFont — bitmap-font rendering against the pc8-backed CGraphic font
 * sheet. Glyphs laid out in row-major order starting at ASCII 32 (space):
 * c = utf8 - 32; cell at (c % cols, c / cols) where cols = sheet_w / G->Width.
 * We paint only the non-transparent pixels of each glyph cell, recolored
 * to the gcn::Graphics's current foreground color — which the caller set
 * via setColor() before calling drawString (gcn::Button::draw etc. all
 * follow that protocol). */
extern "C" void war1_close_active_menu(void);
extern "C" void war1_blit_glyph_via_pc8(const CGraphic *g,
                                        int gx, int gy, int w, int h,
                                        int x, int y, uint32_t fg_argb);
extern "C" int war1_pc8_image_w_for(const CGraphic *g);

#include <guisan/graphics.hpp>

/* Glyph-blit core, shared between CFont::drawString (gcn-side, used by
 * widgets) and CLabel::Draw* (Stratagus HUD side, used by DrawResources
 * etc). Renders txt at absolute screen (absX, absY) using `font`'s pc8
 * sheet; honors ~< … ~> color escapes (reverse_argb for hotkey hint
 * runs, normal_argb otherwise). Optional clip rect (clip_r > clip_l,
 * etc.) constrains glyph blits — used by CFont::drawString so text
 * inside a gcn::ScrollArea (briefing scroller, in particular) doesn't
 * bleed out of its viewport. Pass clip_r==clip_l to disable clipping. */
static void blit_glyph_clipped(const CGraphic *G, int gx, int gy, int gw, int gh,
                               int x, int y, uint32_t fg,
                               int cl, int ct, int cr, int cb)
{
    if (cr > cl) {
        if (x >= cr || x + gw <= cl || y >= cb || y + gh <= ct) return;
        if (x < cl) { int d = cl - x; gx += d; gw -= d; x = cl; }
        if (y < ct) { int d = ct - y; gy += d; gh -= d; y = ct; }
        if (x + gw > cr) gw = cr - x;
        if (y + gh > cb) gh = cb - y;
        if (gw <= 0 || gh <= 0) return;
    }
    war1_blit_glyph_via_pc8(G, gx, gy, gw, gh, x, y, fg);
}

static void war1_draw_text_at(const CFont *font, int absX, int absY,
                              std::string_view txt,
                              uint32_t normal_argb, uint32_t reverse_argb,
                              int clipL, int clipT, int clipR, int clipB)
{
    if (!font) return;
    if (txt.empty() || txt.size() > 4096) return;  /* sanity cap */
    auto G = const_cast<CFont *>(font)->GetGraphic();
    if (!G) return;
    int sheet_w = war1_pc8_image_w_for(G.get());
    if (sheet_w <= 0) return;
    int gw = G->Width  > 0 ? G->Width  : 5;
    int gh = G->Height > 0 ? G->Height : 8;
    if (gw <= 0 || gh <= 0) return;
    int cols = sheet_w / gw;
    if (cols <= 0) return;
    uint32_t cur_argb = normal_argb;
    bool one_shot_reverse = false;
    int penX = absX;
    for (size_t i = 0; i < txt.size(); i++) {
        unsigned ch = (unsigned char)txt[i];
        if (ch == '~' && i + 1 < txt.size()) {
            char next = txt[i + 1];
            if (next == '~') {              /* literal '~' */
                ch = '~';
                i++;
            } else if (next == '!') {       /* ~!  — next char in reverse */
                cur_argb = reverse_argb;
                one_shot_reverse = true;
                i++; continue;
            } else if (next == '<') {       /* ~<  — start reverse run */
                cur_argb = reverse_argb;
                i++; continue;
            } else if (next == '>') {       /* ~>  — end reverse run */
                cur_argb = normal_argb;
                i++; continue;
            } else if (next == '|') {       /* ~|  — internal toggle */
                i++; continue;
            } else {
                size_t end = txt.find('~', i + 1);
                if (end != std::string_view::npos) { i = end; continue; }
            }
        }
        if (ch < 32) continue;
        int cell = (int)ch - 32;
        int srcX = (cell % cols) * gw;
        int srcY = (cell / cols) * gh;
        blit_glyph_clipped(G.get(), srcX, srcY, gw, gh, penX, absY, cur_argb,
                           clipL, clipT, clipR, clipB);
        penX += gw;
        if (one_shot_reverse) {
            cur_argb = normal_argb;
            one_shot_reverse = false;
        }
    }
}

void CFont::drawString(gcn::Graphics *graphics, const std::string &txt,
                       int x, int y, bool /*enabled*/)
{
    if (!graphics) return;
    const gcn::ClipRectangle &r = graphics->getCurrentClipArea();
    int absX = x + r.xOffset;
    int absY = y + r.yOffset;
    const gcn::Color &fg = graphics->getColor();
    uint32_t normal_argb;
    if (fg.r == 0 && fg.g == 0 && fg.b == 0) {
        normal_argb = 0xFFFFE055u;     /* war1 menu yellow */
    } else {
        normal_argb = ((uint32_t)fg.a << 24) | ((uint32_t)fg.r << 16)
                    | ((uint32_t)fg.g << 8)  | (uint32_t)fg.b;
    }
    /* gcn::ClipRectangle is in absolute screen coords: glyphs outside
     * [r.x .. r.x+r.width] × [r.y .. r.y+r.height] must not draw. The
     * scrolling briefing label depended on this — without clipping the
     * scrolled-up text bled all the way to screen y=0. */
    war1_draw_text_at(this, absX, absY, txt, normal_argb, /*reverse=*/0xFFFFFFFFu,
                      r.x, r.y, r.x + r.width, r.y + r.height);
}
int  CFont::getStringIndexAt(const std::string &, int) const { return 0; }
int  CFont::Height() const { return G ? G->Height : 0; }
int  CFont::Width(int) const { return G ? G->Width : 0; }
bool CFont::IsLoaded() const { return G != nullptr; }
void CFont::Load() {}
std::shared_ptr<CGraphic> CFont::GetGraphic() const { return G; }
void CFont::DynamicLoad() const {}
void CFont::MeasureWidths() {}
CFontColor::CFontColor(std::string ident) : Ident(std::move(ident)) {
    /* upstream sets alpha=SDL_ALPHA_OPAQUE (255); match that */
    for (auto &c : Colors) { c.a = 255; }
}
CFontColor *CFontColor::New(std::string const &ident) {
    auto &slot = war1_fontcolor_map()[ident];
    if (!slot) slot = std::make_unique<CFontColor>(ident);
    return slot.get();
}
CFontColor *CFontColor::Get(std::string_view ident) {
    auto it = war1_fontcolor_map().find(std::string(ident));
    return it == war1_fontcolor_map().end() ? nullptr : it->second.get();
}
int  CFont::Width(std::string_view sv) const {
    /* Strip Stratagus color escapes (~X), then count visible chars × glyph
     * width. Fixed-pitch — variable-width font widths aren't worth
     * porting yet. */
    if (!G) return 0;
    int gw = G->Width > 0 ? G->Width : 5;
    int n = 0;
    for (size_t i = 0; i < sv.size(); i++) {
        char ch = sv[i];
        if (ch == '~' && i + 1 < sv.size()) {
            char next = sv[i + 1];
            if (next == '~') { n++; i++; continue; }
            if (next == '!' || next == '<' || next == '>' || next == '|') {
                i++; continue;
            }
            size_t end = sv.find('~', i + 1);
            if (end != std::string_view::npos) { i = end; continue; }
        }
        if ((unsigned char)ch >= 32) n++;
    }
    return n * gw;
}
std::string_view GetLineFont(unsigned, std::string_view, unsigned, CFont const *) { return {}; }

/* CGraphic extra overloads — DrawFrame routes through the same pc8 bridge
 * as DrawFrameClip so Stratagus-initiated draws still produce pixels. */
extern "C" void war1_blit_via_pc8(const CGraphic *g, unsigned frame, int x, int y);
void CGraphic::DrawFrame(unsigned frame, int x, int y, SDL_Surface *) const {
    war1_blit_via_pc8(this, frame, x, y);
}
void CGraphic::AppendFrames(std::vector<std::unique_ptr<SDL_Surface, sdl2::SDL_Deleter>> const &) {}

/* CIcon ctor / PaletteSwap ctor now in ui/icons.cpp */

/* Viewport toggles (normally in ui/viewport.cpp). */
/* CViewport::ShowAStarPassability / ShowGrid now in map/map_draw.cpp */

/* Sound — all backing now in sound/ ported files:
 * - DistanceSilent / SoundForName / MakeSound / MakeSoundGroup / MapSound:
 *   sound/sound_id.cpp
 * - FreeSample / SetChannelVolume / StopAllChannels / StopChannel /
 *   StopMusic / PlayMusic / SoundEnabled / Set*Enabled / Get/Set*Volume:
 *   sound/sound_server.cpp
 * - PlayFile / PlayGameSound / PlayMissileSound / PlayUnitSound:
 *   sound/sound.cpp
 * - LoadUnitSounds / MapUnitSounds / SoundConfig::MapSound:
 *   sound/unitsound.cpp */

/* ==== StartMap / CreateGame / GameMainLoop supporting stubs ==== */
/* Most of these are frontend/UI layer code we haven't ported yet. Safe
 * no-ops unblock the engine boot — when real UI is wired up later, we
 * swap real implementations in. */
/* CleanMessages now in ui/mainscr.cpp
 * SaveUserInterface now in ui/ui.cpp
 * LoadIcons now in ui/icons.cpp
 * InitUserInterface / CUserInterface::Load now in ui/ui.cpp
 * InitButtons now in ui/botpanel.cpp */
void RestoreColorCyclingSurface() {}
void NetworkOnStartGame() {}
/* LoadCursors now in video/cursor.cpp */
/* HandleCheats now in ui/interface.cpp */
void CVideo::ClearScreen() {}
std::pair<std::string, std::string> GetDefaultTextColors() { return {}; }
void SetDefaultTextColors(std::string const &, std::string const &) {}
/* NameLine now in stratagus.cpp */
int NetConnectRunning = 0;
/* InterfaceState now in ui/interface.cpp */

/* SendCommand* — upstream these marshal commands over the network in
 * multiplayer. In single-player they dispatch straight to Command*
 * (defined in action/command.cpp, which we do have). For now stub:
 * this means user clicks won't drive the unit via Stratagus's input
 * path yet. Once the map boots and renders, we can implement these as
 * thin wrappers over the Command* calls. */
/* Single-player single-machine port — no network layer. Upstream's
 * network/commands.cpp wraps these with `if (replay) Command*(); else
 * NetworkSendCommand(...)`; for our config the network branch never
 * actually runs (no peers), so we just call the local Command* directly.
 * Each function is a one-line forward to the real Command* impl that
 * lives in third_party/stratagus/src/action/command.cpp. */
void SendCommandStopUnit(CUnit &unit) { CommandStopUnit(unit); }
void SendCommandMove(CUnit &unit, Vec2T<short> const &pos, EFlushMode flush) { CommandMove(unit, pos, flush); }
void SendCommandAttack(CUnit &unit, Vec2T<short> const &pos, CUnit *target, EFlushMode flush) { CommandAttack(unit, pos, target, flush); }
void SendCommandAttackGround(CUnit &unit, Vec2T<short> const &pos, EFlushMode flush) { CommandAttackGround(unit, pos, flush); }
void SendCommandAutoRepair(CUnit &unit, int on) { CommandAutoRepair(unit, on); }
void SendCommandAutoSpellCast(CUnit &unit, int spellid, int on) { CommandAutoSpellCast(unit, spellid, on); }
void SendCommandBoard(CUnit &unit, CUnit &dest, EFlushMode flush) { CommandBoard(unit, dest, flush); }
void SendCommandBuildBuilding(CUnit &unit, Vec2T<short> const &pos, CUnitType &what, EFlushMode flush) { CommandBuildBuilding(unit, pos, what, flush); }
void SendCommandCancelResearch(CUnit &unit) { CommandCancelResearch(unit); }
void SendCommandCancelTraining(CUnit &unit, int slot, CUnitType const *type) { CommandCancelTraining(unit, slot, type); }
void SendCommandCancelUpgradeTo(CUnit &unit) { CommandCancelUpgradeTo(unit); }
void SendCommandDefend(CUnit &unit, CUnit &dest, EFlushMode flush) { CommandDefend(unit, dest, flush); }
void SendCommandExplore(CUnit &unit, EFlushMode flush) { CommandExplore(unit, flush); }
void SendCommandFollow(CUnit &unit, CUnit &dest, EFlushMode flush) { CommandFollow(unit, dest, flush); }
void SendCommandPatrol(CUnit &unit, Vec2T<short> const &pos, EFlushMode flush) { CommandPatrolUnit(unit, pos, flush); }
void SendCommandRepair(CUnit &unit, Vec2T<short> const &pos, CUnit *dest, EFlushMode flush) { CommandRepair(unit, pos, dest, flush); }
void SendCommandResearch(CUnit &unit, CUpgrade &what, EFlushMode flush) { CommandResearch(unit, what, flush); }
void SendCommandResource(CUnit &unit, CUnit &dest, EFlushMode flush) { CommandResource(unit, dest, flush); }
void SendCommandResourceLoc(CUnit &unit, Vec2T<short> const &pos, EFlushMode flush) { CommandResourceLoc(unit, pos, flush); }
void SendCommandReturnGoods(CUnit &unit, CUnit *depot, EFlushMode flush) { CommandReturnGoods(unit, depot, flush); }
extern void CommandSpellCast(CUnit &, Vec2T<short> const &, CUnit *, SpellType const &, EFlushMode, bool isAutocast);
void SendCommandSpellCast(CUnit &unit, Vec2T<short> const &pos, CUnit *dest, int spellId, EFlushMode flush) {
    if (spellId >= 0 && spellId < (int)SpellTypeTable.size()) {
        CommandSpellCast(unit, pos, dest, *SpellTypeTable[spellId], flush, false);
    }
}
void SendCommandStandGround(CUnit &unit, EFlushMode flush) { CommandStandGround(unit, flush); }
void SendCommandTrainUnit(CUnit &unit, CUnitType &type, EFlushMode flush) { CommandTrainUnit(unit, type, flush); }
void SendCommandUnload(CUnit &unit, Vec2T<short> const &pos, CUnit *what, EFlushMode flush) { CommandUnload(unit, pos, what, flush); }
void SendCommandUpgradeTo(CUnit &unit, CUnitType &type, EFlushMode flush) { CommandUpgradeTo(unit, type, flush, false); }


/* Network globals + functions. Hosts is a fixed-size array per netconnect.h. */
CNetworkHost Hosts[PlayerMax];
int NetLocalHostsSlot = 0;
void ExitNetwork1() {}
void SendCommandDiplomacy(int, EDiplomacy, int) {}
void SendCommandSharedVision(int, bool, int) {}

/* MenuRace now in stratagus.cpp */

/* OnlineService CclRegister — the file path is online_service.cpp which
 * depends on networking. Stub no-op. */
void OnlineServiceCclRegister() {}

/* trigger.cpp variable/component helpers — normally in stratagus/trigger.cpp
 * (which now is in game/ dir but may not define these). */
/* GetComponent now in ui/mainscr.cpp */
/* Str2EnumVariable now in script_ui.cpp */

/* tolua-generated binding registrar. Upstream Stratagus uses tolua++ to
 * auto-generate Lua bindings for its C++ API. Stub as a no-op. */
/* Real tolua_stratagus_open is now provided by war1_tolua_bindings.cpp
 * (generated from third_party/stratagus/src/tolua/*.pkg at build time).
 * The generated bindings reference ~40 support-system globals and functions
 * we haven't ported (AI, network, cursors, fonts, translations, sound
 * volume). Stub them as no-ops / dummies so linking succeeds. */
int CyclesPerSecond = 30;
/* CliMapName now in stratagus.cpp */
std::string NetworkMapName;
std::string NetworkMapFragmentName;
void CServerSetup::Clear() {}  /* minimal Clear so the default ctor links */
size_t CServerSetup::Serialize(unsigned char *) const { return 0; }
size_t CServerSetup::Deserialize(const unsigned char *) { return 0; }
CServerSetup LocalSetupState;
CServerSetup ServerSetupState;
void LoadFonts() {}
/* InitVideoCursors now in video/cursor.cpp */
/* InitAiModule / AiAttackWithForce* now in third_party/stratagus/src/ai/ */
void InitNetwork1() {}
/* GetUnitUnderCursor now in ui/mouse.cpp */
int GetNetworkState() { return 0; }
/* GetEffectsVolume / GetMusicVolume / SetEffectsVolume / SetMusicVolume /
 * IsEffectsEnabled / IsMusicEnabled / IsMusicPlaying / SetChannelStereo
 * now in sound/sound_server.cpp */
/* SetChannelVolume / StopAllChannels / StopChannel / StopMusic /
 * PlayFile / PlayMusic now in sound/ ported files */
/* LoadPO / SetTranslationsFiles now in translate.cpp */
void NetworkDetachFromServer() {}
void NetworkGamePrepareGameSettings() {}
void NetworkInitClientConnect() {}
void NetworkInitServerConnect(int) {}
void NetworkProcessClientRequest() {}
void NetworkServerResyncClients() {}
void NetworkServerStartGame() {}
int NetworkSetupServerAddress(std::string const &, int) { return 0; }

/* video.pkg bindings: no-op stubs for real-engine rendering paths. */
void AddColorCyclingRange(unsigned int, unsigned int) {}
unsigned int SetColorCycleSpeed(unsigned int s) { return s; }
void SetColorCycleAll(bool) {}
void ClearAllColorCyclingRange() {}
void InitVideo() {
    /* Upstream sdl.cpp:InitVideo creates an SDL window + GL context + a
     * 32-bit RGBA8888 SDL_Surface for TheScreen, and writes the resolution
     * into the global CVideo. Bare-metal Jupiter has a fixed 480×272 LCD
     * driven by DE/TCON — no window, no SDL — but Stratagus reads
     * Video.Width/Height/Depth directly (SetClipping, viewport sizing,
     * cursor clamping, message wrapping) so we still need them populated.
     * TheScreen is the SDL surface that BlitSurfaceAlphaBlending and friends
     * write to; war1_init_the_screen points its pixels at the OVL back fb. */
    Video.Width  = 480;
    Video.Height = 272;
    Video.Depth  = 32;
    war1_init_the_screen();
}
/* PlayMovie's return contract (movie.cpp:520): 0 = movie played
 * successfully, non-zero = "this file is not a supported movie format"
 * — caller (TitleScreen::ShowTitleImage et al.) then falls through to
 * the static-image branch. Route through our cedar-driven vbin player:
 * /videos/<name>.vbin on the FatFs partition. Returns 0 if the vbin
 * was found and played (so ShowTitleScreens treats the title screen
 * as fully handled), -1 otherwise (static image fallback runs).
 *
 * This is what plays the boot-time campaign intros — Lua's
 * SetTitleScreens lists videos/hintro.ogv / ointro.ogv / cave.ogv /
 * title.ogv (stratagus.lua:486-494) and Stratagus walks them through
 * PlayMovie one by one. */
extern "C" int war1_movie_play(const char *path);
int  PlayMovie(std::string const &name) { return war1_movie_play(name.c_str()); }
/* ShowFullImage now in title.cpp */
void SaveMapPNG(char const *) {}
void ToggleFullScreen() {}
bool CVideo::ResizeScreen(int, int) { return false; }
void CGraphic::ResizeKeepRatio(int, int) {}
extern "C" int war1_pc8_set_palette_argb(const CGraphic *g, int idx, uint32_t argb);
void CGraphic::SetPaletteColor(int idx, int r, int g, int b) {
    /* Briefing Lua calls bg1:SetPaletteColor(255, 48, 56, 56) to recolor
     * the dithered shadow palette index. Without this, palette[255] keeps
     * the source PNG's value and shows up as scattered bright pixels on
     * the briefing-room stone wall. */
    uint32_t argb = 0xFF000000u
                  | ((uint32_t)(r & 0xFF) << 16)
                  | ((uint32_t)(g & 0xFF) << 8)
                  |  (uint32_t)(b & 0xFF);
    war1_pc8_set_palette_argb(this, idx, argb);
}
void CGraphic::OverlayGraphic(CGraphic *, bool) {}
std::shared_ptr<CGraphic> CGraphic::Get(std::string const &) { return nullptr; }
std::shared_ptr<CPlayerColorGraphic> CPlayerColorGraphic::Get(std::string const &) { return nullptr; }
std::shared_ptr<CPlayerColorGraphic> CPlayerColorGraphic::ForceNew(std::string const &file, int w, int h) {
    return CPlayerColorGraphic::New(file, w, h);
}

/* ==== UI bindings support (script_ui.cpp pulls these in) ==== */
#include "ui/contenttype.h"
#include "ui/popup.h"
/* CleanButtons / UnitButtonTable / LastDrawnButtonPopup / CurrentButtonLevel /
 *   AddButton now in ui/botpanel.cpp
 * CStatusLine::Clear / ClearCosts now in ui/statusline.cpp
 * UiGroupKeys / Get/SetDoubleClickDelay / Get/SetHoldClickDelay /
 *   SetScrollMargins now in ui/interface.cpp
 * RightButtonAttacks / Get/SetMouseScroll / Get/SetKeyScroll /
 *   Get/SetGrabMouse / Get/SetLeaveStops now in ui/ui.cpp
 * ParsePopupContent now in ui/popup.cpp */
int  FontCodePage = 0;
char VideoForceFullScreen = 0;
int GetHotKey(std::string const &) { return 0; }
ButtonStyle *FindButtonStyle(std::string) { return nullptr; }

/* CVideo drawing primitives — upstream video/linedraw.cpp impls. We render
 * via sprite_blit_indexed on the Jupiter side, so these stubs suffice
 * until a proper video backend is ported. */
void CVideo::FillTransRectangle(unsigned long, int, int, int, int, unsigned char) {}
void CVideo::DrawRectangle(unsigned long, int, int, int, int) {}
void CVideo::DrawVLine(unsigned long, int, int, int) {}
void CVideo::DrawHLine(unsigned long, int, int, int) {}
/* Upstream video/shaders or sdl.cpp — SDL keypress → guichan key mapping. */
int convertSDLKeyCharacterToGuichanKey(int k) { return k; }
/* CLabel Draw / DrawClip / DrawReverse all in upstream video/font.cpp now. */
/* PaletteSwap real impl now in upstream video/color.cpp. */
/* CPlayerColorGraphic::Clone — deep copy, unused on bare-metal path. */
std::shared_ptr<CPlayerColorGraphic> CPlayerColorGraphic::Clone(bool) const { return nullptr; }

/* Misc UI support — SDL/engine plumbing that's SDL- or network-tied. */
const char *SdlKey2Str(int) { return ""; }
void NetworkSendChatMessage(std::string const &) {}
void SaveScreenshotPNG(char const *) {}
void ToggleGrabMouse(int) {}
CFont &GetSmallFont() {
    /* Lazily ensure a registered "small" font exists; CFont::New uses its
     * private ctor internally. */
    auto *f = CFont::Get("small");
    if (!f) f = CFont::New("small", nullptr);
    return *f;
}
int CLabel::DrawReverse(int x, int y, std::string_view s) const {
    /* "Reverse" run paints fully in the reverse (yellow) color. */
    war1_draw_text_at(font, x, y, s, 0xFFFFE055u, 0xFFFFFFFFu);
    return font ? font->Width(s) : 0;
}
int CLabel::DrawClip(int x, int y, std::string_view s, bool /*is_normal*/) const {
    war1_clabel_draw_str(font, x, y, s);
    return font ? font->Width(s) : 0;
}
unsigned long GetTicks() { return 0; }
/* CViewport::IsInsideMapArea / ScreenToMapPixelPos / Contains now in map/map_draw.cpp */
void CVideo::FillRectangle(unsigned long, int, int, int, int) {}
void InvalidateArea(int, int, int, int) {}
bool IsGameFontReady() { return false; }
void CFiller::Load() {}
bool SdlGetGrabMouse() { return false; }

/* Upstream defines in video/sdl.cpp (not compiled). Used by GetHotKey
 * in font.cpp to map a label like "f10" to a key code. We don't have
 * hotkeys wired yet — return 0 so widgets just have no hotkey assigned. */
int Str2SdlKey(const char * /*str*/) { return 0; }

/* CVideo::LockScreen / UnlockScreen — upstream wraps SDL_LockSurface
 * around per-pixel writes. On bare-metal we write to the back fb
 * directly with no locking required. No-op stubs satisfy the linker
 * for video/linedraw.cpp transparent fills. */
void CVideo::LockScreen() {}
void CVideo::UnlockScreen() {}
/* PaletteSwap ctor real impl in upstream video/color.cpp. The empty stub
 * that lived here silently dropped every argument and left the object
 * uninitialized — surfaced as a DABORT in ApplyPaletteSwaps the first
 * time a unit was selected and the side panel rendered the icon. */

/* CContentType* — base has pure-virtual Draw + Parse; every subclass must
 * override BOTH so the vtable emits. */
/* ContentType subclasses + ButtonCheck predicates now in ported
 * ui/contenttype.cpp and ui/button_checks.cpp. */

/* Stubs for CclRegister functions belonging to modules we didn't port
 * (network/, editor/). Each no-op skips that module's Lua binding
 * registration — scripts that use those bindings will see nil and
 * handle it (or error). */
void EditorCclRegister()         {}
void NetworkCclRegister()        {}
/* ReplayCclRegister and TriggerCclRegister now in game/ dir. */
/* UserInterfaceCclRegister now in script_ui.cpp */
void VideoCclRegister()          {}
/* CViewport dtor now in map/map_draw.cpp */
CFiller::bits_map::~bits_map() = default;
