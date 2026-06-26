/* Real C++ tolua bindings for guisan widgets.
 *
 * Replaces the earlier permissive Lua-stub approach (every method a no-op)
 * with bindings that wrap real gcn:: instances. JupiterGraphics actually
 * paints them; events route to real listeners; menus run.
 *
 * Coverage is a subset chosen for what war1gus's guichan.lua + the
 * in-game menu (scripts/menus/game.lua) actually invoke. Less-used widget
 * classes get a permissive class-table fallback so their script load
 * doesn't error, even if the methods don't render anything yet.
 *
 * Memory model: each widget constructor `new`s a gcn:: object and pushes
 * it as a tolua usertype. Lifetime is tied to the Lua userdata's GC; we
 * delete in the .collector callback when registered. Containers DO NOT
 * own children — guichan.lua keeps strong refs in `menu._addedWidgets`,
 * so children outlive the menu only as long as the menu table itself
 * does. Color is fully GC'd (allocated, owned, freed by Lua).
 *
 * Naming convention follows upstream Stratagus's ui.pkg: classes are
 * exposed under their short name (Color, Widget, Container, MenuScreen,
 * Label, ImageWidget, ButtonWidget, ImageButton, ...) — these become
 * Lua globals with __call constructors so guichan.lua's idiomatic
 * `dark = Color(38,38,78)` works without `:new()`. */

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "tolua++.h"

#include <new>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <vector>

#include <guisan.hpp>
#include <guisan/widgets/button.hpp>
#include <guisan/widgets/checkbox.hpp>
#include <guisan/widgets/container.hpp>
#include <guisan/widgets/dropdown.hpp>
#include <guisan/widgets/icon.hpp>
#include <guisan/widgets/label.hpp>
#include <guisan/widgets/listbox.hpp>
#include <guisan/widgets/radiobutton.hpp>
#include <guisan/widgets/scrollarea.hpp>
#include <guisan/widgets/slider.hpp>
#include <guisan/widgets/textbox.hpp>
#include <guisan/stringlistmodel.hpp>
#include <guisan/widgets/textfield.hpp>
#include <guisan/widgets/window.hpp>
#include "jupiter_image.hpp"

#include "stratagus.h"
#include "video.h"
#include "font.h"
#include "ui.h"
#include "widgets.h"

extern "C" void uart_puts(const char *);
extern "C" void uart_puthex(unsigned);
extern "C" void uart_putdec(unsigned);

namespace {

/* ---------------- helpers ---------------- */

template <typename T>
T *self(lua_State *L, int idx) {
    return reinterpret_cast<T *>(tolua_tousertype(L, idx, nullptr));
}

/* When called with `Color(...)` Lua passes the class table as arg 1.
 * When called with `Color:new(...)` same. Otherwise plain args start
 * at 1. Returns the index of the first real arg. */
static int arg_base(lua_State *L) {
    return (lua_gettop(L) >= 1 && (lua_istable(L, 1) || lua_isuserdata(L, 1)))
        ? 2 : 1;
}

static const char *opt_string(lua_State *L, int idx, const char *def = "") {
    if (lua_isstring(L, idx)) return lua_tostring(L, idx);
    return def;
}

static int opt_int(lua_State *L, int idx, int def = 0) {
    /* lua_tointeger returns 0 for non-integer-representable floats — which
     * includes any Lua expression that does division, like
     * `x * Video.Width / 320`. The briefing/menu Lua code passes positions
     * that way. Use lua_tonumber and round-half-away-from-zero so e.g.
     * 124.5 lands at 125 instead of being truncated to 124 (a half-pixel
     * drift that the briefing animations made visible). */
    if (lua_isnumber(L, idx)) {
        double v = lua_tonumber(L, idx);
        return (int)(v + (v >= 0.0 ? 0.5 : -0.5));
    }
    return def;
}

static double opt_num(lua_State *L, int idx, double def = 0.0) {
    if (lua_isnumber(L, idx)) return lua_tonumber(L, idx);
    return def;
}

/* Recover a CGraphic from a Lua arg and wrap its pc8 pixels in a
 * JupiterImage so guisan's drawImage path (which dynamic_cast's to
 * JupiterImage) handles it uniformly — no void* type-erased fallback
 * needed in JupiterGraphics. The pc8 is 8-bit indexed; we expand to
 * ARGB8888 once into a freshly-allocated buffer that the JupiterImage
 * owns and frees on destruction. */
extern "C" const uint8_t *war1_pc8_pixels_for(const CGraphic *g, int *outW, int *outH);
extern "C" const uint32_t *war1_pc8_palette_for(const CGraphic *g);
extern "C" uint32_t        war1_pc8_palette_epoch(const void *g);
using CGraphicPtr = std::shared_ptr<CGraphic>;
/* C2: JupiterImage cache keyed by (CGraphic*, outW, outH). Paired with
 * B1's CGraphic dedup, repeated ImageWidget(sameGraphic) constructions
 * (every briefing's bg1, every animation strip, repeated mission loads,
 * etc.) reuse the same ARGB buffer instead of malloc'ing a fresh
 * Video.W*Video.H*4 = 510 KB blob per construction. Widgets still
 * never destruct (Hazard C structural), but with B1+C2 the same image
 * across many loads costs one buffer total instead of N.
 *
 * The cache holds shared_ptr<gcn::Image> with strong refs; when this
 * fires, the SAME JupiterImage object backs every ImageWidget that
 * asked for that source. ownsPixels=true is still safe — the ARGB is
 * freed exactly when the cache entry itself drops (which is never on
 * the device, so effectively a bounded per-image leak — same shape as
 * the widget leak but a small fraction of the volume). */
#include <map>
#include <tuple>
#include <cstring>
/* B1-bisect lesson: std::unordered_map's first-insert bucket
 * allocation triggers std::bad_array_new_length under our
 * arm-none-eabi-gcc 14.2 + libstdc++ build, before any cache entry
 * is ever stored. std::map (tree, no bucket arrays) works fine and
 * is what upstream Stratagus uses for similar string→smart-pointer
 * caches. Key is (CGraphic*, outW, outH) as a std::tuple, which has
 * a natural lexicographic ordering. */
using JImageCacheKey = std::tuple<const CGraphic *, int, int>;
static std::map<JImageCacheKey, std::shared_ptr<gcn::Image>> s_jimage_cache;
/* C1: drop cache entries that no live widget references anymore
 * (use_count == 1 means we're the sole holder). Called from the menu
 * subtree sweep after widget deletions, so any briefing-bg or
 * animation-strip image whose owning widgets just got destroyed
 * actually frees its ARGB buffer. Bounded work; map is small. */
extern "C" void war1_jimg_cache_evict_orphans(void) {
    unsigned evicted = 0;
    for (auto it = s_jimage_cache.begin(); it != s_jimage_cache.end(); ) {
        if (it->second.use_count() == 1) {
            it = s_jimage_cache.erase(it);
            evicted++;
        } else {
            ++it;
        }
    }
    if (evicted) {
        uart_puts("[JC] evicted "); uart_putdec(evicted);
        uart_puts(" jimg cache entries\n");
    }
}
static std::shared_ptr<gcn::Image> arg_to_image(lua_State *L, int idx) {
    void *p = tolua_tousertype(L, idx, nullptr);
    if (!p) return nullptr;
    auto *spptr = reinterpret_cast<CGraphicPtr *>(p);
    if (!*spptr) return nullptr;
    CGraphic *g = spptr->get();
    int sheetW = 0, sheetH = 0;
    const uint8_t *src = war1_pc8_pixels_for(g, &sheetW, &sheetH);
    const uint32_t *pal = war1_pc8_palette_for(g);
    if (!src || !pal || sheetW <= 0 || sheetH <= 0) {
        /* No pc8 backing — return nullptr; ImageWidget ctor handles null. */
        return nullptr;
    }
    /* CGraphic::Resize on the pc8 path doesn't actually rescale pixels —
     * it just updates Width/Height as the *logical* size callers (HUD
     * fillers, ImageWidget bg) expect. When the script Resize'd a small
     * native sheet up to a larger target (guichan.lua resizes the
     * 320×200 title_screen to Video.Width × Video.Height so the menu's
     * full-screen MenuScreen has a backdrop that fills the LCD), build
     * the JupiterImage at the target size and aspect-fit-scale the
     * native art into it: scale to vertical fill, pillarbox the
     * horizontal slack with transparent margin. Nearest-neighbor expand
     * folded into the indexed→ARGB pass — single write per output pixel,
     * no temp buffer. Icon::draw then sees an image at widget-size and
     * renders it at (0,0), so the scaled artwork lands centered on the
     * LCD with the original aspect preserved. */
    int outW = sheetW, outH = sheetH;
    if (g->Width  > sheetW) outW = g->Width;
    if (g->Height > sheetH) outH = g->Height;
    /* C2: cache hit short-circuits the malloc + ARGB bake. */
    JImageCacheKey k(g, outW, outH);
    auto cit = s_jimage_cache.find(k);
    if (cit != s_jimage_cache.end()) return cit->second;
    auto *argb = static_cast<uint32_t *>(std::malloc(outW * outH * sizeof(uint32_t)));
    if (!argb) return nullptr;
    if (outW == sheetW && outH == sheetH) {
        for (int i = 0, n = sheetW * sheetH; i < n; i++) argb[i] = pal[src[i]];
    } else {
        /* Stretch sheet to fill outW × outH (non-aspect-preserving). The
         * Lua side does `bg1:Resize(Video.Width, Video.Height)` and the
         * briefing's SetupAnimation positions heads at
         * `x * Video.Width / 320`, which is also full-stretch. The
         * earlier aspect-preserving letterbox math here put bg pixels
         * 22 px to the right of where the animations expected, causing
         * every animated head/torch to look horizontally shifted. */
        for (int y = 0; y < outH; y++) {
            const int srcY = y * sheetH / outH;
            const uint8_t *s = src + srcY * sheetW;
            uint32_t *dst = argb + y * outW;
            for (int x = 0; x < outW; x++) {
                const int srcX = x * sheetW / outW;
                dst[x] = pal[s[srcX]];
            }
        }
    }
    auto *jimg = new gcn::JupiterImage(outW, outH, argb, /*ownsPixels=*/true);
    /* Tag the source CGraphic + current palette epoch so drawImage can
     * detect a SetPaletteColor since bake and re-expand from live pc8 */
    jimg->setPC8Source(g, war1_pc8_palette_epoch(g));
    auto sp = std::shared_ptr<gcn::Image>(jimg);
    s_jimage_cache.emplace(k, sp);
    return sp;
}

/* ---------------- Color ---------------- */

static int Color_collect(lua_State *L) {
    auto *c = self<gcn::Color>(L, 1);
    delete c;
    return 0;
}

static int Color_new(lua_State *L) {
    int b = arg_base(L);
    int r = opt_int(L, b + 0, 0);
    int g = opt_int(L, b + 1, 0);
    int bl = opt_int(L, b + 2, 0);
    int a = opt_int(L, b + 3, 255);
    auto *c = new gcn::Color(r, g, bl, a);
    tolua_pushusertype_and_takeownership(L, c, "Color");
    return 1;
}

static int Color_get_r(lua_State *L) { auto *c = self<gcn::Color>(L, 1); lua_pushinteger(L, c ? c->r : 0); return 1; }
static int Color_get_g(lua_State *L) { auto *c = self<gcn::Color>(L, 1); lua_pushinteger(L, c ? c->g : 0); return 1; }
static int Color_get_b(lua_State *L) { auto *c = self<gcn::Color>(L, 1); lua_pushinteger(L, c ? c->b : 0); return 1; }
static int Color_get_a(lua_State *L) { auto *c = self<gcn::Color>(L, 1); lua_pushinteger(L, c ? c->a : 0); return 1; }
static int Color_set_r(lua_State *L) { auto *c = self<gcn::Color>(L, 1); if (c) c->r = (int)luaL_checkinteger(L, 3); return 0; }
static int Color_set_g(lua_State *L) { auto *c = self<gcn::Color>(L, 1); if (c) c->g = (int)luaL_checkinteger(L, 3); return 0; }
static int Color_set_b(lua_State *L) { auto *c = self<gcn::Color>(L, 1); if (c) c->b = (int)luaL_checkinteger(L, 3); return 0; }
static int Color_set_a(lua_State *L) { auto *c = self<gcn::Color>(L, 1); if (c) c->a = (int)luaL_checkinteger(L, 3); return 0; }

/* ---------------- Widget base ---------------- */

static int Widget_setSize(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setSize(opt_int(L, 2), opt_int(L, 3));
    return 0;
}
static int Widget_setWidth(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setWidth(opt_int(L, 2));
    return 0;
}
static int Widget_setHeight(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setHeight(opt_int(L, 2));
    return 0;
}
static int Widget_getWidth(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    lua_pushinteger(L, w ? w->getWidth() : 0);
    return 1;
}
static int Widget_getHeight(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    lua_pushinteger(L, w ? w->getHeight() : 0);
    return 1;
}
static int Widget_setPosition(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setPosition(opt_int(L, 2), opt_int(L, 3));
    return 0;
}
static int Widget_setX(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setX(opt_int(L, 2));
    return 0;
}
static int Widget_setY(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setY(opt_int(L, 2));
    return 0;
}
static int Widget_getX(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    lua_pushinteger(L, w ? w->getX() : 0);
    return 1;
}
static int Widget_getY(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    lua_pushinteger(L, w ? w->getY() : 0);
    return 1;
}
static int Widget_setBaseColor(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    auto *c = self<gcn::Color>(L, 2);
    if (w && c) w->setBaseColor(*c);
    return 0;
}
static int Widget_setForegroundColor(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    auto *c = self<gcn::Color>(L, 2);
    if (w && c) w->setForegroundColor(*c);
    return 0;
}
static int Widget_setBackgroundColor(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    auto *c = self<gcn::Color>(L, 2);
    if (w && c) w->setBackgroundColor(*c);
    return 0;
}
static int Widget_setBorderSize(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setFrameSize(opt_int(L, 2));
    return 0;
}
static int Widget_setEnabled(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setEnabled(lua_toboolean(L, 2));
    return 0;
}
static int Widget_setVisible(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->setVisible(lua_toboolean(L, 2));
    return 0;
}
static int Widget_isVisible(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    lua_pushboolean(L, w ? w->isVisible() : false);
    return 1;
}
static int Widget_setFont(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    auto *f = self<gcn::Font>(L, 2);  /* CFont* IS-A gcn::Font* */
    if (w && f) w->setFont(f);
    return 0;
}
static int Widget_setGlobalFont(lua_State *L) {
    /* Static method: Widget:setGlobalFont(font) — class table at arg 1,
     * font at arg 2. Set the static field directly, skipping upstream's
     * mWidgetInstances iteration (which calls fontChanged() — a virtual
     * no-op for every widget class we use, so the iteration is dead
     * weight that was the suspect for the earlier hang). */
    auto *f = self<gcn::Font>(L, 2);
    if (!f) return 0;
    struct GlobalFontPoke : public gcn::Widget {
        static void set(gcn::Font *font) { mGlobalFont = font; }
    };
    GlobalFontPoke::set(f);
    return 0;
}
static int Widget_setHotKey(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (!w) return 0;
    if (lua_isnumber(L, 2)) {
        w->setHotKey((int)lua_tointeger(L, 2));
    } else if (lua_isstring(L, 2)) {
        gcn::Key k = GetHotKey(lua_tostring(L, 2));
        w->setHotKey(k.getValue());
    }
    return 0;
}
static int Widget_requestFocus(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) w->requestFocus();
    return 0;
}
static int Widget_addActionListener(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    auto *al = self<gcn::ActionListener>(L, 2);
    if (w && al) w->addActionListener(al);
    return 0;
}
static int Widget_addMouseListener(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    auto *al = reinterpret_cast<gcn::MouseListener *>(
        tolua_tousertype(L, 2, nullptr));
    if (w && al) w->addMouseListener(al);
    return 0;
}
static int Widget_addKeyListener(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    auto *al = reinterpret_cast<gcn::KeyListener *>(
        tolua_tousertype(L, 2, nullptr));
    if (w && al) w->addKeyListener(al);
    return 0;
}
static int Widget_setOpaque(lua_State *L) {
    /* gcn::Widget itself doesn't have setOpaque — only Container/Window do.
     * Dispatch via dynamic_cast. */
    auto *w = self<gcn::Widget>(L, 1);
    if (auto *c = dynamic_cast<gcn::Container *>(w)) c->setOpaque(lua_toboolean(L, 2));
    else if (auto *win = dynamic_cast<gcn::Window *>(w)) win->setOpaque(lua_toboolean(L, 2));
    return 0;
}
static int Widget_setDirty(lua_State *L) {
    /* No-op — upstream's setDirty itself errors out; safe to ignore. */
    (void)L;
    return 0;
}
static int Widget_setDisabledColor(lua_State *L) {
    /* No-op — upstream removed disabled-color support. */
    (void)L;
    return 0;
}

/* ---------------- Container ---------------- */

static int Container_new(lua_State *L) {
    (void)L;
    auto *c = new gcn::Container();
    tolua_pushusertype(L, c, "Container");
    return 1;
}
static int Container_add(lua_State *L) {
    auto *c = self<gcn::Container>(L, 1);
    auto *w = self<gcn::Widget>(L, 2);
    int x = opt_int(L, 3, 0);
    int y = opt_int(L, 4, 0);
    static int trace = 0;
    if (trace < 60) {
        trace++;
        uart_puts("[CA] enter c=");
        uart_puthex((unsigned)(uintptr_t)c);
        uart_puts(" w=");
        uart_puthex((unsigned)(uintptr_t)w);
        uart_puts(" x=");
        uart_putdec((unsigned)x);
        uart_puts(" y=");
        uart_putdec((unsigned)y);
        uart_puts("\n");
    }
    if (c && w) c->add(w, x, y);
    if (trace <= 60) uart_puts("[CA] gcn add ok\n");
    /* Keep a strong Lua ref to the child so it isn't GC'd while still
     * mounted in the parent. We do NOT touch the userdata's tolua peer
     * table (it points at LUA_REGISTRYINDEX by default) — instead store
     * child refs in a registry-anchored table keyed by parent userdata
     * pointer. The table holds child userdata as values so they stay
     * reachable until the parent is GC'd. */
    /* STRONG-keyed registry table. Original design used weak keys so
     * dead-menu entries could be reaped — but that triggered a chain
     * GC that ultimately deleted LuaActionListener userdata while the
     * underlying button still pointed at it from its mActionListeners
     * list. Result: dangling listener → next frame's distributeMouseEvent
     * hung/crashed. Strong keys leak each menu-open's entries, but
     * menus are rare enough that the small per-open leak is acceptable
     * vs. losing the menu entirely on first hover. */
    static const char *kChildRegistry = "war1_widget_children";
    lua_getfield(L, LUA_REGISTRYINDEX, kChildRegistry);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, kChildRegistry);
    }
    /* registry[parent] = registry[parent] or {} */
    lua_pushvalue(L, 1);  /* parent ud */
    lua_rawget(L, -2);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, 1);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
    }
    /* children_table[child] = true (keys: child ud) */
    lua_pushvalue(L, 2);
    lua_pushboolean(L, 1);
    lua_rawset(L, -3);
    lua_pop(L, 2);
    if (trace <= 60) uart_puts("[CA] registry done\n");
    return 0;
}
static int Container_remove(lua_State *L) {
    auto *c = self<gcn::Container>(L, 1);
    auto *w = self<gcn::Widget>(L, 2);
    if (c && w) c->remove(w);
    return 0;
}
static int Container_clear(lua_State *L) {
    auto *c = self<gcn::Container>(L, 1);
    if (c) c->clear();
    return 0;
}

/* ---------------- MenuScreen ---------------- */

static void log_lua_site(lua_State *L, const char *tag, void *ptr) {
    lua_Debug ar;
    /* Dump ALL frames at every level so we can see what's actually
     * on the Lua stack at the time of this binding call. */
    uart_puts(tag); uart_puts(" ptr=");
    uart_puthex((unsigned)(uintptr_t)ptr);
    int level = 0;
    int found = 0;
    while (lua_getstack(L, level, &ar)) {
        found = 1;
        if (lua_getinfo(L, "Sln", &ar)) {
            uart_puts(" L"); uart_putdec((unsigned)level);
            uart_puts("=");
            uart_puts(ar.source ? ar.source : "(null)");
            uart_puts(":");
            uart_putdec((unsigned)ar.currentline);
            if (ar.name) { uart_puts("("); uart_puts(ar.name); uart_puts(")"); }
        }
        level++;
        if (level > 8) { uart_puts(" ..."); break; }
    }
    if (!found) uart_puts(" <empty-stack>");
    uart_puts("\n");
}
extern "C" unsigned int libc_shim_heap_used(void);
extern "C" unsigned int war1_live_jupiter_images(void);
extern "C" unsigned long war1_live_jupiter_image_bytes(void);
/* Drop registry[<menu>] from war1_widget_children so the menu's child
 * widgets become Lua-GC-eligible. Without this, every briefing's bg1
 * / bg2 / animation widgets stay pinned (strong-keyed registry),
 * keeping their CGraphic shared_ptrs alive forever — visible as
 * accumulating heap + visual corruption + video-pipeline hang after
 * many mission loads. */
static void clear_widget_registry_for(lua_State *L, int menu_idx) {
    static const char *kChildRegistry = "war1_widget_children";
    lua_getfield(L, LUA_REGISTRYINDEX, kChildRegistry);
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, menu_idx);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }
    lua_pop(L, 1);
}
static int MenuScreen_new(lua_State *L) {
    auto *m = new ::MenuScreen();
    /* C diagnostic: heap_used is monotonic high-water (blind to live
     * leaks); jimg=N bytes=B is the honest in-use ARGB-buffer count. */
    uart_puts("[MS] new heap="); uart_putdec(libc_shim_heap_used());
    uart_puts(" jimg="); uart_putdec(war1_live_jupiter_images());
    uart_puts(" jimg_bytes="); uart_putdec((unsigned)war1_live_jupiter_image_bytes());
    uart_puts("\n");
    log_lua_site(L, "[MS] new from", m);
    tolua_pushusertype(L, m, "MenuScreen");
    return 1;
}
static int MenuScreen_run(lua_State *L) {
    auto *m = self<::MenuScreen>(L, 1);
    bool loop = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;
    log_lua_site(L, "[MS] run from", m);
    int r = m ? m->run(loop) : 0;
    lua_pushinteger(L, r);
    return 1;
}
static int MenuScreen_stop(lua_State *L) {
    auto *m = self<::MenuScreen>(L, 1);
    int r = opt_int(L, 2, 0);
    log_lua_site(L, "[MS] stop from", m);
    clear_widget_registry_for(L, 1);
    if (m) m->stop(r);
    uart_puts("[MS] stop heap="); uart_putdec(libc_shim_heap_used()); uart_puts("\n");
    return 0;
}
static int MenuScreen_stopAll(lua_State *L) {
    auto *m = self<::MenuScreen>(L, 1);
    int r = opt_int(L, 2, 0);
    log_lua_site(L, "[MS] stopAll from", m);
    clear_widget_registry_for(L, 1);
    if (m) m->stopAll(r);
    uart_puts("[MS] stopAll heap="); uart_putdec(libc_shim_heap_used()); uart_puts("\n");
    return 0;
}
static int MenuScreen_addLogicCallback(lua_State *L) {
    auto *m = self<::MenuScreen>(L, 1);
    auto *al = self<LuaActionListener>(L, 2);
    if (m && al) m->addLogicCallback(al);
    return 0;
}
static int MenuScreen_setDrawMenusUnder(lua_State *L) {
    auto *m = self<::MenuScreen>(L, 1);
    if (m) m->setDrawMenusUnder(lua_toboolean(L, 2));
    return 0;
}
static int MenuScreen_getDrawMenusUnder(lua_State *L) {
    auto *m = self<::MenuScreen>(L, 1);
    lua_pushboolean(L, m ? m->getDrawMenusUnder() : false);
    return 1;
}

/* ---------------- MultiLineLabel (custom war1 impl, see war1_widgets.cpp) ---------------- */

extern "C" gcn::Widget *war1_new_multilinelabel(const char *caption);
extern "C" void war1_mll_set_caption(gcn::Widget *w, const char *caption);
extern "C" const char *war1_mll_get_caption(gcn::Widget *w);
extern "C" void war1_mll_set_alignment(gcn::Widget *w, int a);
extern "C" void war1_mll_set_vertical_alignment(gcn::Widget *w, int a);
extern "C" void war1_mll_set_line_width(gcn::Widget *w, int width);
extern "C" void war1_mll_adjust_size(gcn::Widget *w);

static int MultiLineLabel_new(lua_State *L) {
    int b = arg_base(L);
    const char *cap = opt_string(L, b, "");
    auto *w = war1_new_multilinelabel(cap);
    tolua_pushusertype(L, w, "MultiLineLabel");
    return 1;
}
static int MultiLineLabel_setCaption(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) war1_mll_set_caption(w, opt_string(L, 2));
    return 0;
}
static int MultiLineLabel_getCaption(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    lua_pushstring(L, w ? war1_mll_get_caption(w) : "");
    return 1;
}
static int MultiLineLabel_setAlignment(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) war1_mll_set_alignment(w, opt_int(L, 2));
    return 0;
}
static int MultiLineLabel_setVerticalAlignment(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) war1_mll_set_vertical_alignment(w, opt_int(L, 2));
    return 0;
}
static int MultiLineLabel_setLineWidth(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) war1_mll_set_line_width(w, opt_int(L, 2));
    return 0;
}
static int MultiLineLabel_adjustSize(lua_State *L) {
    auto *w = self<gcn::Widget>(L, 1);
    if (w) war1_mll_adjust_size(w);
    return 0;
}

/* ---------------- ScrollingWidget (custom war1 impl, see war1_widgets.cpp) ---------------- */

extern "C" gcn::Widget *war1_new_scrolling_widget(int w, int h);
extern "C" void war1_sw_add(gcn::Widget *sw, gcn::Widget *child, int x, int y);
extern "C" void war1_sw_set_speed(gcn::Widget *sw, float speed);
extern "C" void war1_sw_restart(gcn::Widget *sw);

static int ScrollingWidget_new(lua_State *L) {
    int b = arg_base(L);
    int w = opt_int(L, b);
    int h = opt_int(L, b + 1);
    gcn::Widget *sw = war1_new_scrolling_widget(w, h);
    tolua_pushusertype(L, sw, "ScrollingWidget");
    return 1;
}
static int ScrollingWidget_add(lua_State *L) {
    auto *sw = self<gcn::Widget>(L, 1);
    auto *child = (gcn::Widget *)tolua_tousertype(L, 2, nullptr);
    int x = opt_int(L, 3);
    int y = opt_int(L, 4);
    if (sw && child) war1_sw_add(sw, child, x, y);
    return 0;
}
static int ScrollingWidget_setSpeed(lua_State *L) {
    auto *sw = self<gcn::Widget>(L, 1);
    if (sw) war1_sw_set_speed(sw, (float)lua_tonumber(L, 2));
    return 0;
}
static int ScrollingWidget_restart(lua_State *L) {
    auto *sw = self<gcn::Widget>(L, 1);
    if (sw) war1_sw_restart(sw);
    return 0;
}

/* ---------------- Label ---------------- */

static int Label_new(lua_State *L) {
    int b = arg_base(L);
    const char *cap = opt_string(L, b, "");
    auto *l = new gcn::Label(cap);
    tolua_pushusertype(L, l, "Label");
    return 1;
}
static int Label_setCaption(lua_State *L) {
    auto *l = self<gcn::Label>(L, 1);
    if (l) l->setCaption(opt_string(L, 2));
    return 0;
}
static int Label_getCaption(lua_State *L) {
    auto *l = self<gcn::Label>(L, 1);
    lua_pushstring(L, l ? l->getCaption().c_str() : "");
    return 1;
}
static int Label_adjustSize(lua_State *L) {
    auto *l = self<gcn::Label>(L, 1);
    if (l) l->adjustSize();
    return 0;
}
static int Label_setAlignment(lua_State *L) {
    auto *l = self<gcn::Label>(L, 1);
    if (l) l->setAlignment(static_cast<gcn::Graphics::Alignment>(opt_int(L, 2)));
    return 0;
}

/* ---------------- ImageWidget (Stratagus's ::ImageWidget wraps gcn::Icon) ---------------- */
/* Stratagus's ::ImageWidget is header-only (just calls gcn::Icon's ctor),
 * so we use the typedef without needing a separate .cpp body. The
 * shared_ptr<gcn::Image> overload keeps a strong ref alive for the
 * widget's lifetime. */

static int ImageWidget_new(lua_State *L) {
    int b = arg_base(L);
    auto img = arg_to_image(L, b);
    auto *w = new ::ImageWidget(img);
    tolua_pushusertype(L, w, "ImageWidget");
    return 1;
}

/* ---------------- Button family ---------------- */

static int Button_setCaption(lua_State *L) {
    auto *bn = self<gcn::Button>(L, 1);
    if (bn) bn->setCaption(opt_string(L, 2));
    return 0;
}
static int Button_getCaption(lua_State *L) {
    auto *bn = self<gcn::Button>(L, 1);
    lua_pushstring(L, bn ? bn->getCaption().c_str() : "");
    return 1;
}
static int Button_adjustSize(lua_State *L) {
    auto *bn = self<gcn::Button>(L, 1);
    if (bn) bn->adjustSize();
    return 0;
}

static int ButtonWidget_new(lua_State *L) {
    int b = arg_base(L);
    const char *cap = opt_string(L, b, "");
    auto *w = new ::ButtonWidget(cap);
    tolua_pushusertype(L, w, "ButtonWidget");
    return 1;
}

/* ImageButton: backed by upstream CImageButton (verbatim port of
 * src/ui/widgets.cpp:CImageButton::draw). Lua passes a CGraphic to
 * setNormalImage; arg_to_image wraps it in a JupiterImage shared_ptr
 * which CImageButton then drives via its real state-aware draw path. */
extern "C" gcn::Widget *war1_new_image_button(const char *caption);
extern "C" void war1_imgbtn_set_normal_image_shared(gcn::Widget *w, std::shared_ptr<gcn::Image> img);
static int ImageButton_new(lua_State *L) {
    int b = arg_base(L);
    const char *cap = opt_string(L, b, "");
    gcn::Widget *w = war1_new_image_button(cap);
    tolua_pushusertype(L, w, "ImageButton");
    return 1;
}
static int ImageButton_setNormalImage(lua_State *L) {
    auto *btn = self<gcn::Widget>(L, 1);
    auto img  = arg_to_image(L, 2);
    if (btn && img) war1_imgbtn_set_normal_image_shared(btn, img);
    return 0;
}
static int ImageButton_setImage_noop(lua_State *L) { (void)L; return 0; }

/* ---------------- CheckBox ---------------- */

static int CheckBox_new(lua_State *L) {
    int b = arg_base(L);
    const char *cap = opt_string(L, b, "");
    bool marked = lua_isboolean(L, b + 1) ? lua_toboolean(L, b + 1) : false;
    auto *w = new gcn::CheckBox(cap, marked);
    tolua_pushusertype(L, w, "CheckBox");
    return 1;
}
static int CheckBox_isMarked(lua_State *L) {
    auto *cb = self<gcn::CheckBox>(L, 1);
    lua_pushboolean(L, cb ? cb->isSelected() : false);
    return 1;
}
static int CheckBox_setMarked(lua_State *L) {
    auto *cb = self<gcn::CheckBox>(L, 1);
    if (cb) cb->setSelected(lua_toboolean(L, 2));
    return 0;
}
static int CheckBox_setCaption(lua_State *L) {
    auto *cb = self<gcn::CheckBox>(L, 1);
    if (cb) cb->setCaption(opt_string(L, 2));
    return 0;
}
static int CheckBox_getCaption(lua_State *L) {
    auto *cb = self<gcn::CheckBox>(L, 1);
    lua_pushstring(L, cb ? cb->getCaption().c_str() : "");
    return 1;
}
static int CheckBox_adjustSize(lua_State *L) {
    auto *cb = self<gcn::CheckBox>(L, 1);
    if (cb) cb->adjustSize();
    return 0;
}

/* ---------------- RadioButton ---------------- */

static int RadioButton_new(lua_State *L) {
    int b = arg_base(L);
    const char *cap = opt_string(L, b, "");
    const char *grp = opt_string(L, b + 1, "");
    bool marked = lua_isboolean(L, b + 2) ? lua_toboolean(L, b + 2) : false;
    auto *w = (cap[0] || grp[0]) ? new gcn::RadioButton(cap, grp, marked)
                                  : new gcn::RadioButton();
    tolua_pushusertype(L, w, "RadioButton");
    return 1;
}
static int RadioButton_setCaption(lua_State *L) {
    auto *r = self<gcn::RadioButton>(L, 1);
    if (r) r->setCaption(opt_string(L, 2));
    return 0;
}
static int RadioButton_setGroup(lua_State *L) {
    auto *r = self<gcn::RadioButton>(L, 1);
    if (r) r->setGroup(opt_string(L, 2));
    return 0;
}
static int RadioButton_isMarked(lua_State *L) {
    auto *r = self<gcn::RadioButton>(L, 1);
    lua_pushboolean(L, r ? r->isSelected() : false);
    return 1;
}
static int RadioButton_setMarked(lua_State *L) {
    auto *r = self<gcn::RadioButton>(L, 1);
    if (r) r->setSelected(lua_toboolean(L, 2));
    return 0;
}
static int RadioButton_adjustSize(lua_State *L) {
    auto *r = self<gcn::RadioButton>(L, 1);
    if (r) r->adjustSize();
    return 0;
}

/* ---------------- Slider ---------------- */

static int Slider_new(lua_State *L) {
    int b = arg_base(L);
    int n = lua_gettop(L) - (b - 1);
    gcn::Slider *s;
    if (n >= 2)      s = new gcn::Slider(opt_num(L, b + 0), opt_num(L, b + 1));
    else if (n == 1) s = new gcn::Slider(opt_num(L, b + 0, 1.0));
    else             s = new gcn::Slider(1.0);
    tolua_pushusertype(L, s, "Slider");
    return 1;
}
static int Slider_getValue(lua_State *L) {
    auto *s = self<gcn::Slider>(L, 1);
    lua_pushnumber(L, s ? s->getValue() : 0.0);
    return 1;
}
static int Slider_setValue(lua_State *L) {
    auto *s = self<gcn::Slider>(L, 1);
    if (s) s->setValue(opt_num(L, 2));
    return 0;
}
static int Slider_setScale(lua_State *L) {
    auto *s = self<gcn::Slider>(L, 1);
    if (s) s->setScale(opt_num(L, 2), opt_num(L, 3));
    return 0;
}
static int Slider_setMarkerLength(lua_State *L) {
    auto *s = self<gcn::Slider>(L, 1);
    if (s) s->setMarkerLength(opt_int(L, 2));
    return 0;
}
static int Slider_setOrientation(lua_State *L) {
    auto *s = self<gcn::Slider>(L, 1);
    if (s) s->setOrientation(static_cast<gcn::Slider::Orientation>(opt_int(L, 2)));
    return 0;
}
static int Slider_setStepLength(lua_State *L) {
    auto *s = self<gcn::Slider>(L, 1);
    if (s) s->setStepLength(opt_num(L, 2));
    return 0;
}

/* ---------------- TextField (gcn::TextField — Stratagus CTextField subclass
 * has selection/password/UTF-8 extras whose impls live in unbuilt
 * widgets.cpp; defer.) ---------------- */

static int TextField_new(lua_State *L) {
    int b = arg_base(L);
    const char *txt = opt_string(L, b, "");
    auto *t = new gcn::TextField(txt);
    tolua_pushusertype(L, t, "TextField");
    return 1;
}
static int TextField_setText(lua_State *L) {
    auto *t = self<gcn::TextField>(L, 1);
    if (t) t->setText(opt_string(L, 2));
    return 0;
}
static int TextField_getText(lua_State *L) {
    auto *t = self<gcn::TextField>(L, 1);
    lua_pushstring(L, t ? t->getText().c_str() : "");
    return 1;
}
static int TextField_setPassword(lua_State *L) {
    /* No-op until CTextField body is in build. */
    (void)L;
    return 0;
}

/* ---------------- TextBox (gcn::TextBox — Stratagus subclass deferred) ---------------- */

static int TextBox_new(lua_State *L) {
    int b = arg_base(L);
    const char *txt = opt_string(L, b, "");
    auto *tb = new gcn::TextBox(txt);
    tolua_pushusertype(L, tb, "TextBox");
    return 1;
}
static int TextBox_getText(lua_State *L) {
    auto *t = self<gcn::TextBox>(L, 1);
    lua_pushstring(L, t ? t->getText().c_str() : "");
    return 1;
}
static int TextBox_setEditable(lua_State *L) {
    auto *t = self<gcn::TextBox>(L, 1);
    if (t) t->setEditable(lua_toboolean(L, 2));
    return 0;
}

/* ---------------- ScrollArea ---------------- */

static int ScrollArea_new(lua_State *L) {
    (void)L;
    auto *s = new gcn::ScrollArea();
    tolua_pushusertype(L, s, "ScrollArea");
    return 1;
}
static int ScrollArea_setContent(lua_State *L) {
    auto *s = self<gcn::ScrollArea>(L, 1);
    auto *w = self<gcn::Widget>(L, 2);
    if (s) s->setContent(w);
    return 0;
}
static int ScrollArea_getContent(lua_State *L) {
    auto *s = self<gcn::ScrollArea>(L, 1);
    auto *w = s ? s->getContent() : nullptr;
    if (w) tolua_pushusertype(L, w, "Widget");
    else lua_pushnil(L);
    return 1;
}
static int ScrollArea_setScrollbarWidth(lua_State *L) {
    auto *s = self<gcn::ScrollArea>(L, 1);
    if (s) s->setScrollbarWidth(opt_int(L, 2));
    return 0;
}

/* ---------------- ListBox (gcn::ListBox — Stratagus's ListBoxWidget
 * Lua-list bridge needs LuaListModel from unbuilt widgets.cpp; defer.) ---------------- */

static int ListBoxWidget_new(lua_State *L) {
    int b = arg_base(L);
    int w = opt_int(L, b + 0, 100);
    int h = opt_int(L, b + 1, 50);
    auto *lb = new gcn::ListBox();
    lb->setSize(w, h);
    /* Pre-attach an empty StringListModel so setList can repopulate it
     * in place; saves the "no model yet" branch on the hot path. The
     * model is heap-owned and outlives the listbox (we leak it on
     * teardown — menus are short-lived in scripts and the listbox
     * itself is GC'd only when the menu table goes away). */
    lb->setListModel(new gcn::StringListModel());
    tolua_pushusertype(L, lb, "ListBoxWidget");
    return 1;
}
static int ListBoxWidget_setList(lua_State *L) {
    auto *lb = self<gcn::ListBox>(L, 1);
    if (!lb) return 0;
    auto *model = dynamic_cast<gcn::StringListModel *>(lb->getListModel());
    if (!model) {
        model = new gcn::StringListModel();
        lb->setListModel(model);
    }
    /* Replace the model's vector with the Lua array's contents. The
     * caller passes a 1-indexed Lua sequence of strings (typical
     * `dirlist` from guichan.lua addBrowser). */
    auto &vec = model->getElements();
    vec.clear();
    if (lua_istable(L, 2)) {
        int n = (int)lua_rawlen(L, 2);
        vec.reserve((size_t)n);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, 2, i);
            const char *s = lua_tostring(L, -1);
            vec.emplace_back(s ? s : "");
            lua_pop(L, 1);
        }
    }
    /* Re-set the model pointer to nudge gcn::ListBox to update its
     * internal accounting (e.g. selected-index clamp). Do NOT call
     * adjustSize() — that shrinks the widget to content height (single
     * row = ~12 px) and the explicit (w, h) passed to ListBoxWidget()
     * is gone, so user clicks in the original area never reach the
     * widget bounds. Upstream Stratagus wraps ListBox in a ScrollArea
     * that preserves outer bounds; we use a bare ListBox so we keep
     * its size set at construction time. */
    lb->setListModel(model);
    return 0;
}
static int ListBoxWidget_setSelected(lua_State *L) {
    auto *lb = self<gcn::ListBox>(L, 1);
    if (lb) lb->setSelected(opt_int(L, 2));
    return 0;
}
static int ListBoxWidget_getSelected(lua_State *L) {
    auto *lb = self<gcn::ListBox>(L, 1);
    lua_pushinteger(L, lb ? lb->getSelected() : 0);
    return 1;
}

/* ---------------- DropDown (gcn::DropDown — Stratagus DropDownWidget
 * adds Lua list integration whose body lives in unbuilt widgets.cpp;
 * defer.) ---------------- */

static int DropDownWidget_new(lua_State *L) {
    /* gcn::DropDown wants a ListModel/ScrollArea/ListBox; null-construct
     * is allowed. Items remain empty until LuaListModel binding lands. */
    auto *dd = new gcn::DropDown();
    tolua_pushusertype(L, dd, "DropDownWidget");
    return 1;
}
static int DropDownWidget_setList_noop(lua_State *L) { (void)L; return 0; }
static int DropDownWidget_setSize(lua_State *L) {
    auto *dd = self<gcn::DropDown>(L, 1);
    if (dd) dd->setSize(opt_int(L, 2), opt_int(L, 3));
    return 0;
}
static int DropDownWidget_getSelected(lua_State *L) {
    auto *dd = self<gcn::DropDown>(L, 1);
    lua_pushinteger(L, dd ? dd->getSelected() : 0);
    return 1;
}
static int DropDownWidget_setSelected(lua_State *L) {
    auto *dd = self<gcn::DropDown>(L, 1);
    if (dd) dd->setSelected(opt_int(L, 2));
    return 0;
}

/* ---------------- Window ---------------- */

static int Window_new(lua_State *L) {
    int b = arg_base(L);
    const char *cap = opt_string(L, b, "");
    auto *w = cap[0] ? new gcn::Window(cap) : new gcn::Window();
    tolua_pushusertype(L, w, "Window");
    return 1;
}
static int Window_setCaption(lua_State *L) {
    auto *w = self<gcn::Window>(L, 1);
    if (w) w->setCaption(opt_string(L, 2));
    return 0;
}
static int Window_getCaption(lua_State *L) {
    auto *w = self<gcn::Window>(L, 1);
    lua_pushstring(L, w ? w->getCaption().c_str() : "");
    return 1;
}
static int Window_setMovable(lua_State *L) {
    auto *w = self<gcn::Window>(L, 1);
    if (w) w->setMovable(lua_toboolean(L, 2));
    return 0;
}
static int Window_resizeToContent(lua_State *L) {
    auto *w = self<gcn::Window>(L, 1);
    if (w) w->resizeToContent();
    return 0;
}

/* ---------------- StringListModel ---------------- */

static int StringListModel_new(lua_State *L) {
    int b = arg_base(L);
    std::vector<std::string> v;
    if (lua_istable(L, b)) {
        int n = (int)lua_rawlen(L, b);
        v.reserve(n);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, b, i);
            v.emplace_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : "");
            lua_pop(L, 1);
        }
    }
    auto *m = new ::StringListModel(std::move(v));
    tolua_pushusertype(L, m, "StringListModel");
    return 1;
}

/* ---------------- ButtonStyle (placeholder; real type lives in font/ui) ---------------- */
/* Most uses just pass it around as a tag — leave it as an empty struct
 * userdata that any setter accepts as a no-op. */
struct ButtonStyleStub {};
static int ButtonStyle_new(lua_State *L) {
    auto *s = new ButtonStyleStub();
    tolua_pushusertype_and_takeownership(L, s, "ButtonStyle");
    return 1;
}
static int ButtonStyle_collect(lua_State *L) {
    delete reinterpret_cast<ButtonStyleStub *>(tolua_tousertype(L, 1, nullptr));
    return 0;
}

/* ---------------- LuaActionListener wrapper ---------------- */

static int LuaActionListener_new(lua_State *L) {
    /* Args: class_table, lua_function. Adapt to LuaActionListener's
     * (lua_State*, lua_Object) constructor — lua_Object is just an int
     * in this tolua flavor (typedef int lua_Object), referring to a
     * stack index. We keep a registry ref of the function for safety. */
    int b = arg_base(L);
    if (!lua_isfunction(L, b)) {
        lua_pushnil(L);
        return 1;
    }
    /* lua_Object is the absolute stack index of the function. */
    auto *al = new LuaActionListener(L, b);
    /* A1: push WITHOUT takeownership so Lua GC never deletes the
     * listener. Widgets keep raw pointers to listeners; with widgets
     * themselves never destructed in our embed, freeing the listener
     * dangles those raw pointers and the next Gui->logic() crashes.
     * Bounded leak (one listener per button per session) is much
     * better than the unbounded UAF. */
    tolua_pushusertype(L, al, "LuaActionListener");
    return 1;
}
/* LuaActionListener_collect intentionally unreachable now that
 * LuaActionListener is no longer takeownership'd — kept for parity
 * with the tolua collect-fn signature in case we ever want to wire a
 * deferred-cleanup hook here. */
static int LuaActionListener_collect(lua_State *L) {
    delete reinterpret_cast<LuaActionListener *>(tolua_tousertype(L, 1, nullptr));
    return 0;
}

/* ---------------- registration helpers ---------------- */

/* Install __call on a class table so `Cls(args...)` constructs.
 *
 * Subtle: tolua_cclass's mapinheritance sets the class table's metatable
 * to a SHARED parent (base class metatable, or tolua_commonclass for
 * base=""). Writing __call onto that shared parent would make every
 * sibling-class's later install_ctor overwrite it — so e.g. Color()
 * could end up calling ButtonStyle's ctor.
 *
 * Fix: insert a fresh per-class wrapper metatable BETWEEN the class
 * table and its old inheritance metatable. The wrapper carries __call;
 * its own metatable is the OLD inheritance metatable, so tolua's
 * class_index_event (which walks via lua_getmetatable to find inherited
 * methods) continues to find methods on parent class tables. */
static void install_ctor(lua_State *L, const char *cls_name, lua_CFunction ctor) {
    lua_getglobal(L, cls_name);                  /* class_tbl */
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    /* Capture existing metatable (the shared inheritance-chain parent). */
    lua_getmetatable(L, -1);                     /* class_tbl old_mt | nil */
    /* Build a fresh per-class wrapper metatable. */
    lua_newtable(L);                             /* class_tbl old_mt wrapper */
    lua_pushcfunction(L, ctor);
    lua_setfield(L, -2, "__call");
    if (lua_istable(L, -2)) {
        /* Make wrapper's metatable = old_mt so the metatable-walk chain
         * continues for inherited method resolution. */
        lua_pushvalue(L, -2);
        lua_setmetatable(L, -2);
    }
    /* Slot wrapper in as class_tbl's metatable. */
    lua_setmetatable(L, -3);                     /* class_tbl old_mt */
    lua_pop(L, 2);
}

/* Permissive __index fallback: when a method isn't bound, return a
 * no-op function so the call doesn't error. Lets unbound methods on
 * less-used widget classes silently no-op until we bind them. */
static int permissive_method(lua_State *L) {
    (void)L;
    return 0;
}
/* Install on a class metatable (the type's metatable) such that any
 * method-name lookup that misses both the metatable and the per-class
 * .get table returns the no-op. We chain to class_index_event by
 * checking it first; if it returned nil, we replace with the no-op. */
static int permissive_index(lua_State *L) {
    /* Fallback only — caller has already exhausted normal lookup. */
    (void)L;
    lua_pushcfunction(L, permissive_method);
    return 1;
}

/* Set the __index of the type's metatable to a function that:
 *   1) Calls tolua's class_index_event for normal resolution
 *   2) If result is nil, returns the no-op
 * Implemented as a Lua closure over the original __index. */
static int wrapped_index(lua_State *L) {
    /* upvalue 1 = original __index function (tolua's class_index_event) */
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushvalue(L, 1);  /* obj */
    lua_pushvalue(L, 2);  /* key */
    lua_call(L, 2, 1);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushcfunction(L, permissive_method);
    }
    return 1;
}
static void install_permissive_fallback(lua_State *L, const char *type_name) {
    luaL_getmetatable(L, type_name);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    lua_getfield(L, -1, "__index");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }
    /* Wrap original __index as upvalue */
    lua_pushcclosure(L, wrapped_index, 1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
}

} /* anon namespace */

/* ====================================================================
 * war1_register_widget_stubs — historical name; now registers REAL
 * widget bindings (the "stubs" qualifier kept so existing call sites in
 * war1_widgets.cpp keep working without an Edit.). Called from
 * initGuichan() after gcn::Gui is set up. */
extern "C" void war1_close_active_menu(void);  /* war1_close_active_menu_decl */
static int war1_close_all_menus_lua(lua_State *) {
    war1_close_active_menu();
    return 0;
}
extern "C" void war1_register_widget_stubs(lua_State *L) {
    if (!L) return;

    tolua_open(L);
    tolua_module(L, NULL, 0);
    tolua_beginmodule(L, NULL);

    /* ----- Color ----- */
    tolua_usertype(L, "Color");
    tolua_cclass(L, "Color", "Color", "", Color_collect);
    tolua_beginmodule(L, "Color");
        tolua_function(L, "new", Color_new);
        tolua_variable(L, "r", Color_get_r, Color_set_r);
        tolua_variable(L, "g", Color_get_g, Color_set_g);
        tolua_variable(L, "b", Color_get_b, Color_set_b);
        tolua_variable(L, "a", Color_get_a, Color_set_a);
    tolua_endmodule(L);

    /* ----- Widget (base) ----- */
    tolua_usertype(L, "Widget");
    tolua_cclass(L, "Widget", "Widget", "", NULL);
    tolua_beginmodule(L, "Widget");
        tolua_function(L, "setSize", Widget_setSize);
        tolua_function(L, "setWidth", Widget_setWidth);
        tolua_function(L, "setHeight", Widget_setHeight);
        tolua_function(L, "getWidth", Widget_getWidth);
        tolua_function(L, "getHeight", Widget_getHeight);
        tolua_function(L, "setPosition", Widget_setPosition);
        tolua_function(L, "setX", Widget_setX);
        tolua_function(L, "setY", Widget_setY);
        tolua_function(L, "getX", Widget_getX);
        tolua_function(L, "getY", Widget_getY);
        tolua_function(L, "setBaseColor", Widget_setBaseColor);
        tolua_function(L, "setForegroundColor", Widget_setForegroundColor);
        tolua_function(L, "setBackgroundColor", Widget_setBackgroundColor);
        tolua_function(L, "setBorderSize", Widget_setBorderSize);
        tolua_function(L, "setEnabled", Widget_setEnabled);
        tolua_function(L, "setVisible", Widget_setVisible);
        tolua_function(L, "isVisible", Widget_isVisible);
        tolua_function(L, "setFont", Widget_setFont);
        tolua_function(L, "setGlobalFont", Widget_setGlobalFont);
        tolua_function(L, "setHotKey", Widget_setHotKey);
        tolua_function(L, "requestFocus", Widget_requestFocus);
        tolua_function(L, "addActionListener", Widget_addActionListener);
        tolua_function(L, "addMouseListener", Widget_addMouseListener);
        tolua_function(L, "addKeyListener", Widget_addKeyListener);
        tolua_function(L, "setOpaque", Widget_setOpaque);
        tolua_function(L, "setDirty", Widget_setDirty);
        tolua_function(L, "setDisabledColor", Widget_setDisabledColor);
    tolua_endmodule(L);

    /* ----- BasicContainer (transparent base; no extra methods) ----- */
    tolua_usertype(L, "BasicContainer");
    tolua_cclass(L, "BasicContainer", "BasicContainer", "Widget", NULL);
    tolua_beginmodule(L, "BasicContainer");
    tolua_endmodule(L);

    /* ----- Container ----- */
    tolua_usertype(L, "Container");
    tolua_cclass(L, "Container", "Container", "BasicContainer", NULL);
    tolua_beginmodule(L, "Container");
        tolua_function(L, "new", Container_new);
        tolua_function(L, "add", Container_add);
        tolua_function(L, "remove", Container_remove);
        tolua_function(L, "clear", Container_clear);
    tolua_endmodule(L);

    /* ----- MenuScreen ----- */
    tolua_usertype(L, "MenuScreen");
    tolua_cclass(L, "MenuScreen", "MenuScreen", "Container", NULL);
    tolua_beginmodule(L, "MenuScreen");
        tolua_function(L, "new", MenuScreen_new);
        tolua_function(L, "run", MenuScreen_run);
        tolua_function(L, "stop", MenuScreen_stop);
        tolua_function(L, "stopAll", MenuScreen_stopAll);
        tolua_function(L, "addLogicCallback", MenuScreen_addLogicCallback);
        tolua_function(L, "setDrawMenusUnder", MenuScreen_setDrawMenusUnder);
        tolua_function(L, "getDrawMenusUnder", MenuScreen_getDrawMenusUnder);
    tolua_endmodule(L);

    /* ----- Label ----- */
    tolua_usertype(L, "Label");
    tolua_cclass(L, "Label", "Label", "Widget", NULL);
    tolua_beginmodule(L, "Label");
        tolua_function(L, "new", Label_new);
        tolua_function(L, "setCaption", Label_setCaption);
        tolua_function(L, "getCaption", Label_getCaption);
        tolua_function(L, "adjustSize", Label_adjustSize);
        tolua_function(L, "setAlignment", Label_setAlignment);
    tolua_endmodule(L);

    /* ----- MultiLineLabel ----- */
    tolua_usertype(L, "MultiLineLabel");
    tolua_cclass(L, "MultiLineLabel", "MultiLineLabel", "Widget", NULL);
    tolua_beginmodule(L, "MultiLineLabel");
        tolua_function(L, "new", MultiLineLabel_new);
        tolua_function(L, "setCaption", MultiLineLabel_setCaption);
        tolua_function(L, "getCaption", MultiLineLabel_getCaption);
        tolua_function(L, "setAlignment", MultiLineLabel_setAlignment);
        tolua_function(L, "setVerticalAlignment", MultiLineLabel_setVerticalAlignment);
        tolua_function(L, "setLineWidth", MultiLineLabel_setLineWidth);
        tolua_function(L, "adjustSize", MultiLineLabel_adjustSize);
        tolua_constant(L, "LEFT",   0);
        tolua_constant(L, "CENTER", 1);
        tolua_constant(L, "RIGHT",  2);
        tolua_constant(L, "TOP",    3);
        tolua_constant(L, "BOTTOM", 4);
    tolua_endmodule(L);

    /* ----- ImageWidget ----- */
    tolua_usertype(L, "ImageWidget");
    tolua_cclass(L, "ImageWidget", "ImageWidget", "Widget", NULL);
    tolua_beginmodule(L, "ImageWidget");
        tolua_function(L, "new", ImageWidget_new);
    tolua_endmodule(L);

    /* ----- ScrollingWidget ----- */
    tolua_usertype(L, "ScrollingWidget");
    tolua_cclass(L, "ScrollingWidget", "ScrollingWidget", "Widget", NULL);
    tolua_beginmodule(L, "ScrollingWidget");
        tolua_function(L, "new", ScrollingWidget_new);
        tolua_function(L, "add", ScrollingWidget_add);
        tolua_function(L, "setSpeed", ScrollingWidget_setSpeed);
        tolua_function(L, "restart", ScrollingWidget_restart);
    tolua_endmodule(L);

    /* ----- Button + ButtonWidget + ImageButton ----- */
    tolua_usertype(L, "Button");
    tolua_cclass(L, "Button", "Button", "Widget", NULL);
    tolua_beginmodule(L, "Button");
        tolua_function(L, "setCaption", Button_setCaption);
        tolua_function(L, "getCaption", Button_getCaption);
        tolua_function(L, "adjustSize", Button_adjustSize);
    tolua_endmodule(L);

    tolua_usertype(L, "ButtonWidget");
    tolua_cclass(L, "ButtonWidget", "ButtonWidget", "Button", NULL);
    tolua_beginmodule(L, "ButtonWidget");
        tolua_function(L, "new", ButtonWidget_new);
    tolua_endmodule(L);

    tolua_usertype(L, "ImageButton");
    tolua_cclass(L, "ImageButton", "ImageButton", "Button", NULL);
    tolua_beginmodule(L, "ImageButton");
        tolua_function(L, "new", ImageButton_new);
        tolua_function(L, "setNormalImage", ImageButton_setNormalImage);
        tolua_function(L, "setPressedImage", ImageButton_setImage_noop);
        tolua_function(L, "setDisabledImage", ImageButton_setImage_noop);
    tolua_endmodule(L);

    /* ----- CheckBox ----- */
    tolua_usertype(L, "CheckBox");
    tolua_cclass(L, "CheckBox", "CheckBox", "Widget", NULL);
    tolua_beginmodule(L, "CheckBox");
        tolua_function(L, "new", CheckBox_new);
        tolua_function(L, "isMarked", CheckBox_isMarked);
        tolua_function(L, "setMarked", CheckBox_setMarked);
        tolua_function(L, "setCaption", CheckBox_setCaption);
        tolua_function(L, "getCaption", CheckBox_getCaption);
        tolua_function(L, "adjustSize", CheckBox_adjustSize);
    tolua_endmodule(L);

    /* ----- RadioButton ----- */
    tolua_usertype(L, "RadioButton");
    tolua_cclass(L, "RadioButton", "RadioButton", "Widget", NULL);
    tolua_beginmodule(L, "RadioButton");
        tolua_function(L, "new", RadioButton_new);
        tolua_function(L, "setCaption", RadioButton_setCaption);
        tolua_function(L, "setGroup", RadioButton_setGroup);
        tolua_function(L, "isMarked", RadioButton_isMarked);
        tolua_function(L, "setMarked", RadioButton_setMarked);
        tolua_function(L, "adjustSize", RadioButton_adjustSize);
    tolua_endmodule(L);

    /* ----- Slider ----- */
    tolua_usertype(L, "Slider");
    tolua_cclass(L, "Slider", "Slider", "Widget", NULL);
    tolua_beginmodule(L, "Slider");
        tolua_function(L, "new", Slider_new);
        tolua_function(L, "getValue", Slider_getValue);
        tolua_function(L, "setValue", Slider_setValue);
        tolua_function(L, "setScale", Slider_setScale);
        tolua_function(L, "setMarkerLength", Slider_setMarkerLength);
        tolua_function(L, "setOrientation", Slider_setOrientation);
        tolua_function(L, "setStepLength", Slider_setStepLength);
        tolua_constant(L, "HORIZONTAL", (int)gcn::Slider::Orientation::Horizontal);
        tolua_constant(L, "VERTICAL", (int)gcn::Slider::Orientation::Vertical);
    tolua_endmodule(L);

    /* ----- TextField / TextBox ----- */
    tolua_usertype(L, "TextField");
    tolua_cclass(L, "TextField", "TextField", "Widget", NULL);
    tolua_beginmodule(L, "TextField");
        tolua_function(L, "new", TextField_new);
        tolua_function(L, "setText", TextField_setText);
        tolua_function(L, "getText", TextField_getText);
        tolua_function(L, "setPassword", TextField_setPassword);
    tolua_endmodule(L);

    tolua_usertype(L, "TextBox");
    tolua_cclass(L, "TextBox", "TextBox", "Widget", NULL);
    tolua_beginmodule(L, "TextBox");
        tolua_function(L, "new", TextBox_new);
        tolua_function(L, "getText", TextBox_getText);
        tolua_function(L, "setEditable", TextBox_setEditable);
    tolua_endmodule(L);

    /* ----- ScrollArea ----- */
    tolua_usertype(L, "ScrollArea");
    tolua_cclass(L, "ScrollArea", "ScrollArea", "BasicContainer", NULL);
    tolua_beginmodule(L, "ScrollArea");
        tolua_function(L, "new", ScrollArea_new);
        tolua_function(L, "setContent", ScrollArea_setContent);
        tolua_function(L, "getContent", ScrollArea_getContent);
        tolua_function(L, "setScrollbarWidth", ScrollArea_setScrollbarWidth);
    tolua_endmodule(L);

    /* ----- ListBoxWidget ----- */
    tolua_usertype(L, "ListBoxWidget");
    tolua_cclass(L, "ListBoxWidget", "ListBoxWidget", "ScrollArea", NULL);
    tolua_beginmodule(L, "ListBoxWidget");
        tolua_function(L, "new", ListBoxWidget_new);
        tolua_function(L, "setList", ListBoxWidget_setList);
        tolua_function(L, "setSelected", ListBoxWidget_setSelected);
        tolua_function(L, "getSelected", ListBoxWidget_getSelected);
    tolua_endmodule(L);

    /* ----- DropDownWidget ----- */
    tolua_usertype(L, "DropDownWidget");
    tolua_cclass(L, "DropDownWidget", "DropDownWidget", "BasicContainer", NULL);
    tolua_beginmodule(L, "DropDownWidget");
        tolua_function(L, "new", DropDownWidget_new);
        tolua_function(L, "setList", DropDownWidget_setList_noop);
        tolua_function(L, "setSize", DropDownWidget_setSize);
        tolua_function(L, "getSelected", DropDownWidget_getSelected);
        tolua_function(L, "setSelected", DropDownWidget_setSelected);
    tolua_endmodule(L);
    /* DropDown alias for upstream's bare class name */
    tolua_usertype(L, "DropDown");
    tolua_cclass(L, "DropDown", "DropDown", "BasicContainer", NULL);
    tolua_beginmodule(L, "DropDown");
        tolua_function(L, "new", DropDownWidget_new);
        tolua_function(L, "getSelected", DropDownWidget_getSelected);
        tolua_function(L, "setSelected", DropDownWidget_setSelected);
    tolua_endmodule(L);

    /* ----- Window ----- */
    tolua_usertype(L, "Window");
    tolua_cclass(L, "Window", "Window", "BasicContainer", NULL);
    tolua_beginmodule(L, "Window");
        tolua_function(L, "new", Window_new);
        tolua_function(L, "setCaption", Window_setCaption);
        tolua_function(L, "getCaption", Window_getCaption);
        tolua_function(L, "setMovable", Window_setMovable);
        tolua_function(L, "resizeToContent", Window_resizeToContent);
    tolua_endmodule(L);

    /* ----- StringListModel ----- */
    tolua_usertype(L, "StringListModel");
    tolua_cclass(L, "StringListModel", "StringListModel", "", NULL);
    tolua_beginmodule(L, "StringListModel");
        tolua_function(L, "new", StringListModel_new);
    tolua_endmodule(L);

    /* ----- ButtonStyle (placeholder) ----- */
    tolua_usertype(L, "ButtonStyle");
    tolua_cclass(L, "ButtonStyle", "ButtonStyle", "", ButtonStyle_collect);
    tolua_beginmodule(L, "ButtonStyle");
        tolua_function(L, "new", ButtonStyle_new);
    tolua_endmodule(L);

    /* ----- LuaActionListener constructor ----- */
    /* The TYPE itself was registered by tolua_stratagus_open. We just
     * need a constructor entry on the Lua side. */
    {
        /* If the class table doesn't exist yet, create it. */
        lua_getglobal(L, "LuaActionListener");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            tolua_usertype(L, "LuaActionListener");
            tolua_cclass(L, "LuaActionListener", "LuaActionListener",
                         "", LuaActionListener_collect);
            tolua_beginmodule(L, "LuaActionListener");
                tolua_function(L, "new", LuaActionListener_new);
            tolua_endmodule(L);
        } else {
            /* Class already exists from upstream tolua bindings — just
             * add the `new` method on it. */
            tolua_beginmodule(L, "LuaActionListener");
                tolua_function(L, "new", LuaActionListener_new);
            tolua_endmodule(L);
            lua_pop(L, 1);
        }
    }

    tolua_endmodule(L);  /* global */

    /* Install __call constructors so `Cls(args)` works as syntactic
     * sugar for `Cls:new(args)`. Mirrors guichan.lua's idiom. */
    install_ctor(L, "Color",            Color_new);
    install_ctor(L, "Container",        Container_new);
    install_ctor(L, "MenuScreen",       MenuScreen_new);
    install_ctor(L, "Label",            Label_new);
    install_ctor(L, "MultiLineLabel",   MultiLineLabel_new);
    install_ctor(L, "ImageWidget",      ImageWidget_new);
    install_ctor(L, "ScrollingWidget",  ScrollingWidget_new);
    install_ctor(L, "ButtonWidget",     ButtonWidget_new);
    install_ctor(L, "ImageButton",      ImageButton_new);
    install_ctor(L, "CheckBox",         CheckBox_new);
    install_ctor(L, "RadioButton",      RadioButton_new);
    install_ctor(L, "Slider",           Slider_new);
    install_ctor(L, "TextField",        TextField_new);
    install_ctor(L, "TextBox",          TextBox_new);
    install_ctor(L, "ScrollArea",       ScrollArea_new);
    install_ctor(L, "ListBoxWidget",    ListBoxWidget_new);
    install_ctor(L, "DropDownWidget",   DropDownWidget_new);
    install_ctor(L, "DropDown",         DropDownWidget_new);
    install_ctor(L, "Window",           Window_new);
    install_ctor(L, "StringListModel",  StringListModel_new);
    install_ctor(L, "ButtonStyle",      ButtonStyle_new);
    install_ctor(L, "LuaActionListener", LuaActionListener_new);

    /* Lua-callable wrapper around war1_close_active_menu. Used by the
     * campaign-step launcher so a freshly-started game starts with no
     * menu widgets layered over the map render — without this, the
     * Single Player → Campaign → mission chain leaves the campaign
     * sub-menu's ImageWidget bg as Gui top, and GameMainLoop's
     * UpdateDisplay → DrawGuichanWidgets paints it over the live
     * game every frame. (forward decl of war1_close_active_menu is
     * at namespace scope above; see war1_close_active_menu_decl.) */
    lua_pushcfunction(L, &war1_close_all_menus_lua);
    lua_setglobal(L, "WAR1_CloseAllMenus");

    /* No permissive fallback: a no-op-returning __index masks legitimate
     * instance-field reads (e.g. guichan.lua's `if not w._actioncb`
     * always sees a function and goes wrong). Unbound methods will
     * raise a clear "attempt to call a nil value" — bind them as needed
     * rather than silently swallowing them. */

    /* Define Lua-side helpers from upstream ui.pkg's $[ ... $] block.
     * setActionCallback wraps the Lua function in a LuaActionListener
     * and registers it via addActionListener. The instance keeps a
     * strong ref in `_actioncb` so the listener doesn't get GC'd
     * while it's still wired. */
    /* Use rawset() to bypass tolua's class_newindex_event — assigning
     * directly to a tolua class table (Widget.foo = fn) recurses
     * through module_newindex_event's metameta-walk and blows the C
     * stack. rawset() does a metatable-free table set. */
    static const char *helpers =
        "rawset(Widget, 'setActionCallback', function(w, f)\n"
        "  local lal = LuaActionListener(f)\n"
        "  if not w._actioncb then w._actioncb = {} end\n"
        "  table.insert(w._actioncb, lal)\n"
        "  w:addActionListener(lal)\n"
        "end)\n"
        "rawset(Widget, 'setMouseCallback', function(w, f)\n"
        "  local lal = LuaActionListener(f)\n"
        "  if not w._mousecb then w._mousecb = {} end\n"
        "  table.insert(w._mousecb, lal)\n"
        "  w:addMouseListener(lal)\n"
        "end)\n"
        "rawset(Widget, 'setKeyCallback', function(w, f)\n"
        "  local lal = LuaActionListener(f)\n"
        "  if not w._keycb then w._keycb = {} end\n"
        "  table.insert(w._keycb, lal)\n"
        "  w:addKeyListener(lal)\n"
        "end)\n"
        /* MenuScreen:add strong-ref book-keeping is handled in C
         * (Container_add stashes the child in a registry-anchored
         * weak-keyed table), so no Lua wrapper needed. */
        ;
    if (luaL_dostring(L, helpers) != LUA_OK) {
        uart_puts("[WIDGETS] helper script error: ");
        uart_puts(lua_tostring(L, -1));
        uart_puts("\n");
        lua_pop(L, 1);
    }

    uart_puts("[WIDGETS] real widget bindings registered\n");
}
