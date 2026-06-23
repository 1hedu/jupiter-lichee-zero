/* Jupiter widget integration — owns gcn::Gui + binds the Jupiter
 * Graphics/Input/ImageLoader backends, provides the LuaActionListener
 * that wraps a Lua function so CUIButton::SetCallback can produce a
 * real callback target instead of the no-op shim that ui_min.pkg used.
 *
 * This is the bare-metal counterpart of upstream third_party/stratagus/
 * src/ui/widgets.cpp (which we exclude from the build because it pulls
 * in SDLGraphics / SDLInput). Method bodies kept structurally identical
 * to upstream so behavior is unchanged where it matters.
 *
 * Wiring:
 *   stratagus.cpp:305       initGuichan()      — once at game init
 *   UpdateDisplay loop      DrawGuichanWidgets() — every frame
 *   WaitEventsOneFrame      pushMouseMove/Button → JupiterInput queue
 *
 * Lua integration:
 *   ui.lua line 484:  UI.MenuButton:SetCallback(function() RunGameMenu() end)
 *   tolua handler:    SetCallback registers the function in the Lua
 *                     registry, constructs a LuaActionListener wrapping
 *                     the registry index, and stores it in
 *                     CUIButton::Callback. When CUIButton's underlying
 *                     widget fires (or our BTN_START fast-path), the
 *                     listener->action() invokes the Lua function.
 */

#include "stratagus.h"
#include "video.h"
#include "ui.h"
#include "interface.h"
#include "widgets.h"
#include "luacallback.h"
#include "script.h"
#include "network.h"

#include <guisan/gui.hpp>
#include <guisan/exception.hpp>
#include <guisan/widgets/scrollarea.hpp>
#include <guisan/widgets/container.hpp>
#include <guisan/widgets/button.hpp>

#include "jupiter_graphics.hpp"
#include "jupiter_input.hpp"
#include "jupiter_imageloader.hpp"
#include "jupiter_image.hpp"

#include <memory>
#include <stack>

extern "C" void war1_register_widget_stubs(lua_State *L);
extern "C" void uart_puts(const char *);
extern "C" void uart_puthex(unsigned);
extern "C" void uart_putdec(unsigned);

/* MultiLineLabel — ported from upstream third_party/stratagus/src/ui/
 * widgets.cpp (we don't compile widgets.cpp because it depends on
 * SDLGraphics/SDLInput). wordWrap splits the caption into mTextRows
 * based on getLineWidth(), so the briefing scroller actually wraps
 * instead of rendering one giant line off-screen. */
class War1MultiLineLabel : public gcn::Widget {
public:
    enum { LEFT = 0, CENTER = 1, RIGHT = 2, TOP = 3, BOTTOM = 4 };

    War1MultiLineLabel() = default;
    explicit War1MultiLineLabel(const std::string &caption) : mCaption(caption) { wordWrap(); }
    void setCaption(const std::string &c) { mCaption = c; wordWrap(); }
    const std::string &getCaption() const { return mCaption; }
    void setAlignment(unsigned a) { mAlignment = a; }
    unsigned getAlignment() const { return mAlignment; }
    void setVerticalAlignment(unsigned a) { mVAlign = a; }
    unsigned getVerticalAlignment() const { return mVAlign; }
    void setLineWidth(int w) { mLineWidth = w; wordWrap(); }
    int  getLineWidth() const { return mLineWidth; }
    void adjustSize() {
        gcn::Font *f = getFont();
        int width = 0;
        for (const auto &s : mTextRows) {
            int w = f ? f->getWidth(s) : 0;
            if (width < w) width = std::min(w, mLineWidth);
        }
        setWidth(width);
        setHeight((f ? f->getHeight() : 8) * (int)mTextRows.size());
    }
    void draw(gcn::Graphics *graphics) override {
        if (!graphics) return;
        gcn::Font *f = getFont();
        if (!f) return;
        graphics->setFont(f);
        graphics->setColor(getForegroundColor());

        int textX = 0;
        switch (mAlignment) {
            case CENTER: textX = getWidth() / 2; break;
            case RIGHT:  textX = getWidth();     break;
            default:     textX = 0;              break;
        }
        const int rowH = f->getHeight();
        int textY = 0;
        switch (mVAlign) {
            case CENTER: textY = (getHeight() - (int)mTextRows.size() * rowH) / 2; break;
            case BOTTOM: textY =  getHeight() - (int)mTextRows.size() * rowH;      break;
            default:     textY = 0;                                                 break;
        }
        const auto align = mAlignment == CENTER ? gcn::Graphics::Center
                          : mAlignment == RIGHT ? gcn::Graphics::Right
                                                : gcn::Graphics::Left;
        for (size_t i = 0; i < mTextRows.size(); i++) {
            if (!mTextRows[i].empty()) {
                graphics->drawText(mTextRows[i], textX, textY + (int)i * rowH, align, isEnabled());
            }
        }
    }
private:
    /* Verbatim port of upstream MultiLineLabel::wordWrap. Splits mCaption
     * into mTextRows on whitespace, never letting any row exceed
     * mLineWidth in pixels. */
    void wordWrap() {
        mTextRows.clear();
        gcn::Font *font = getFont();
        if (!font || mLineWidth <= 0) {
            mTextRows.push_back(mCaption);
            return;
        }
        int lineWidth = mLineWidth;
        std::string str = mCaption;
        size_t pos, lastPos;
        std::string substr;
        bool done = false;
        bool first = true;
        while (!done) {
            if (str.find('\n') != std::string::npos || font->getWidth(str) > lineWidth) {
                first = true;
                lastPos = 0;
                while (1) {
                    pos = str.find_first_of(" \t\n", first ? 0 : lastPos + 1);
                    if (pos != std::string::npos) {
                        substr = str.substr(0, pos);
                        if (font->getWidth(substr) > lineWidth) {
                            if (first) {
                                substr = str.substr(0, pos);
                                mTextRows.push_back(substr);
                                str = str.substr(pos + 1);
                                break;
                            } else {
                                substr = str.substr(0, lastPos);
                                mTextRows.push_back(substr);
                                if (str[lastPos] != '\n') {
                                    while (str[lastPos + 1] == ' ' || str[lastPos + 1] == '\t' || str[lastPos + 1] == '\n') {
                                        ++lastPos;
                                        if (str[lastPos] == '\n') break;
                                    }
                                }
                                str = str.substr(lastPos + 1);
                                break;
                            }
                        } else {
                            if (str[pos] == '\n') {
                                substr = str.substr(0, pos);
                                mTextRows.push_back(substr);
                                str = str.substr(pos + 1);
                                break;
                            }
                        }
                    } else {
                        if (first) {
                            mTextRows.push_back(str);
                            done = true;
                            break;
                        } else {
                            substr = str.substr(0, lastPos);
                            mTextRows.push_back(substr);
                            str = str.substr(lastPos + 1);
                            break;
                        }
                    }
                    lastPos = pos;
                    first = false;
                }
            } else {
                mTextRows.push_back(str);
                done = true;
            }
        }
    }

    std::string mCaption;
    std::vector<std::string> mTextRows;
    unsigned mAlignment = LEFT;
    unsigned mVAlign    = TOP;
    int      mLineWidth = 0;
};

/* CImageButton + ScrollingWidget — VERBATIM PORT from upstream
 * third_party/stratagus/src/ui/widgets.cpp. We don't compile widgets.cpp
 * itself because it depends on SDLGraphics/SDLInput; the class bodies
 * below are the upstream ones with no Jupiter modifications. */

CImageButton::CImageButton() : Button()
{
    setForegroundColor(0xffffff);
}

CImageButton::CImageButton(const std::string &caption) : Button(caption)
{
    setForegroundColor(0xffffff);
}

void CImageButton::draw(gcn::Graphics *graphics) /* override */
{
    if (!normalImage) {
        if (this->getBackgroundColor().a == 0) {
            return;
        }
        Button::draw(graphics);
        return;
    }

    gcn::Image *img;

    if (!isEnabled()) {
        img = disabledImage ? disabledImage.get() : normalImage.get();
    } else if (isPressed()) {
        img = pressedImage ? pressedImage.get() : normalImage.get();
    } else {
        img = normalImage.get();
    }
    graphics->drawImage(img, 0, 0, 0, 0,
                        img->getWidth(), img->getHeight());

    graphics->setColor(getForegroundColor());

    int textX;
    int textY = getHeight() / 2 - getFont()->getHeight() / 2;

    switch (getAlignment()) {
        case gcn::Graphics::Left:
            textX = 4;
            break;
        case gcn::Graphics::Center:
            textX = getWidth() / 2;
            break;
        case gcn::Graphics::Right:
            textX = getWidth() - 4;
            break;
        default:
            textX = 0;
    }

    graphics->setFont(getFont());

    if (isPressed()) {
        graphics->drawText(getCaption(), textX + 4, textY + 4, getAlignment());
    } else {
        graphics->drawText(getCaption(), textX + 2, textY + 2, getAlignment());
    }
    if (isFocused()) {
        graphics->drawRectangle(gcn::Rectangle(0, 0, getWidth(), getHeight()));
    }
}

void CImageButton::adjustSize()
{
    if (normalImage) {
        setWidth(normalImage->getWidth());
        setHeight(normalImage->getHeight());
    } else {
        Button::adjustSize();
    }
}

ScrollingWidget::ScrollingWidget(int width, int height) :
    gcn::ScrollArea(nullptr, gcn::ScrollArea::ScrollPolicy::ShowNever, gcn::ScrollArea::ScrollPolicy::ShowNever),
    speedY(1.f)
{
    container.setDimension(gcn::Rectangle(0, 0, width, height));
    container.setOpaque(false);
    setOpaque(false);
    setContent(&container);
    setDimension(gcn::Rectangle(0, 0, width, height));
}

void ScrollingWidget::add(gcn::Widget *widget, int x, int y)
{
    container.add(widget, x, y);
    if (x + widget->getWidth() > container.getWidth()) {
        container.setWidth(x + widget->getWidth());
    }
    if (y + widget->getHeight() > container.getHeight()) {
        container.setHeight(y + widget->getHeight());
    }
}

void ScrollingWidget::logic() /* override */
{
    if (container.getHeight() + containerY - speedY > 0) {
        containerY -= speedY;
        container.setY((int)containerY);
    } else if (!finished) {
        finished = true;
        distributeActionEvent();
    }
}

void ScrollingWidget::restart()
{
    container.setY(0);
    containerY = 0.f;
    finished = (container.getHeight() == getHeight());
}

/* The single Gui instance — Stratagus expects this as a global
 * std::unique_ptr<gcn::Gui>. Lifetime: created in initGuichan, destroyed
 * in freeGuichan. */
std::unique_ptr<gcn::Gui> Gui;

/* Backend instances — owned by us, referenced by Gui. */
static gcn::JupiterInput     *s_input = nullptr;
static gcn::JupiterGraphics  *s_graphics = nullptr;
static gcn::JupiterImageLoader *s_imageloader = nullptr;

bool GuichanActive = false;

void initGuichan()
{
    /* Backend ImageLoader is the global guisan can use to load images
     * by filename — no widget code needs to know about pc8. */
    s_imageloader = new gcn::JupiterImageLoader();
    gcn::Image::setImageLoader(s_imageloader);

    s_graphics = new gcn::JupiterGraphics();
    s_input    = gcn::getJupiterInput();

    Gui = std::make_unique<gcn::Gui>();
    Gui->setGraphics(s_graphics);
    Gui->setInput(s_input);
    Gui->setTop(nullptr);

    /* Register the permissive widget-class stubs so guichan.lua can
     * load (it instantiates Label/ImageWidget/etc. at top level). */
    war1_register_widget_stubs(Lua);

    GuichanActive = true;
}

void freeGuichan()
{
    if (Gui) {
        delete Gui->getGraphics();   /* JupiterGraphics — we new'd it */
        Gui = nullptr;
        s_graphics = nullptr;
    }
    if (s_imageloader) {
        gcn::Image::setImageLoader(nullptr);
        delete s_imageloader;
        s_imageloader = nullptr;
    }
    /* s_input is owned by getJupiterInput(); leave it. */
    GuichanActive = false;
}

/* Called once per frame from UpdateDisplay (after the Stratagus HUD
 * paint, before Invalidate). Runs widget logic + paints the active
 * widget tree to the back fb. If no widget tree is set (idle state),
 * nothing visible draws. */
void DrawGuichanWidgets()
{
    /* gcn::Gui::draw / logic both throw GCN_EXCEPTION("No top widget
     * set") when Top is nullptr. We bind a real Top only when a menu
     * is active; in idle gameplay there's nothing to draw. Guard so
     * we don't crash on every frame of normal play.
     *
     * We deliberately SKIP Gui->logic(): the title menu's first frame
     * worked, but the moment a sub-menu replaces the top via setTopRaw,
     * Gui::handleModalFocus / handleModalMouseInputFocus see
     * mFocusHandler->mLastWidgetWith*Focus pointing into the previous
     * (now-popped) widget tree while getModalMouseInputFocused() is
     * null — that mismatch fires handleModalFocusReleased →
     * distributeMouseEvent on a stale widget pointer, which is the
     * "second Gui->logic hangs" symptom from earlier bisection.
     *
     * mTop->_logic walks every widget calling its logic() — none of
     * the widget classes we use (Container, Label, ImageWidget,
     * Button family) actually need per-frame logic ticks; they're
     * pure-render with state changes pushed in via setters. Input is
     * routed straight into war1_menu_click on the BTN_B edge in
     * WaitEventsOneFrame, so we don't need Gui's input-dispatch
     * pipeline either. Gui->draw() is the only call we need for
     * widgets to appear. */
    if (Gui && Gui->getTop()) {
        try {
            Gui->logic();
            Gui->draw();
        } catch (const gcn::Exception &e) {
            uart_puts("[GUI] exception: ");
            uart_puts(e.getMessage().c_str());
            uart_puts("\n");
        } catch (...) {
            uart_puts("[GUI] unknown exception\n");
        }
    }
}

/* Upstream's handleInput takes an SDL_Event; we don't have those.
 * Instead the WaitEventsOneFrame layer pushes events directly into the
 * JupiterInput queue via pushMouseMove / pushMouseButton, so this is
 * a no-op kept for source-compat with anything that might call it. */
void handleInput(const SDL_Event * /*event*/) {}

/* ---------------- LuaActionListener ----------------
 * Mirrors third_party/stratagus/src/ui/widgets.cpp:259-340. Same
 * callback semantics so Lua scripts written for upstream Stratagus
 * fire the same arguments. */

LuaActionListener::LuaActionListener(lua_State *l, lua_Object f)
    : callback(l, f)
{
}

LuaActionListener::~LuaActionListener() = default;

void LuaActionListener::action(const gcn::ActionEvent &event)
{
    callback.call(event.getId());
}

void LuaActionListener::keyPressed(gcn::KeyEvent &event) {
    callback.call("keyPress", to_utf8(event.getKey().getValue()));
    event.consume();
}
void LuaActionListener::keyReleased(gcn::KeyEvent &event) {
    callback.call("keyRelease", to_utf8(event.getKey().getValue()));
    event.consume();
}
void LuaActionListener::hotKeyPressed(const gcn::Key &key) {
    callback.call("hotKeyPress", to_utf8(key.getValue()));
}
void LuaActionListener::hotKeyReleased(const gcn::Key &key) {
    callback.call("hotKeyRelease", to_utf8(key.getValue()));
}
void LuaActionListener::mouseEntered(gcn::MouseEvent &event) {
    callback.call("mouseIn"); event.consume();
}
void LuaActionListener::mouseExited(gcn::MouseEvent &event) {
    callback.call("mouseOut"); event.consume();
}
void LuaActionListener::mousePressed(gcn::MouseEvent &event) {
    callback.call("mousePress", event.getButton()); event.consume();
}
void LuaActionListener::mouseReleased(gcn::MouseEvent &event) {
    callback.call("mouseRelease", event.getButton()); event.consume();
}
void LuaActionListener::mouseClicked(gcn::MouseEvent &event) {
    callback.call("mouseClick", event.getButton(), event.getClickCount());
    event.consume();
}
void LuaActionListener::mouseWheelMovedUp(gcn::MouseEvent &event) {
    callback.call("mouseWheelUp"); event.consume();
}
void LuaActionListener::mouseWheelMovedDown(gcn::MouseEvent &event) {
    callback.call("mouseWheelDown"); event.consume();
}
void LuaActionListener::mouseMoved(gcn::MouseEvent &) {}

/* ---------------- MenuScreen ----------------
 * Mirrors third_party/stratagus/src/ui/widgets.cpp:2608-2715. We can't
 * compile that file directly (SDL deps + font.cpp/color.cpp dep cascade
 * we excluded), so the bodies live here. Behavior matches upstream
 * EXCEPT run() is non-blocking even when loop=true: the bare-metal
 * main loop drives Gui->draw() each frame anyway via DrawGuichanWidgets,
 * so a blocking sub-loop here would just deadlock. Lua callbacks fire
 * naturally from the input pump's button events. */

static std::stack< ::MenuScreen * > MenuStack;

/* When the menu was constructed (frame counter) — used to refuse a stop()
 * that fires within the first few frames, which is almost always a sign
 * something in the widget-add chain is misfiring an action listener (e.g.
 * a phantom click). A real user-initiated close happens many frames later. */
extern "C" volatile uint32_t g_war1_frame_counter;
static uint32_t s_menu_open_frame = 0;

::MenuScreen::MenuScreen() : gcn::Container(), runLoop(true)
{
    setDimension(gcn::Rectangle(0, 0, Video.Width, Video.Height));
    setOpaque(false);
    s_menu_open_frame = g_war1_frame_counter;
    if (Gui) {
        oldtop = Gui->getTop();
        Gui->setTop(this);
    } else {
        oldtop = nullptr;
    }
}

int ::MenuScreen::run(bool loop)
{
    this->loopResult = 0;
    this->runLoop = loop;
    MenuStack.push(this);

    /* Non-blocking path: in-game RunGameMenu fires this from inside the
     * frame loop's BTN_START handler; we just register on the stack and
     * let the already-running GameMainLoop pump UpdateDisplay each frame.
     * Blocking case (loop=true): the canonical title menu reaches us via
     * stratagusMain → MenuLoop → guichan.lua → RunProgramStartMenu, with
     * NO outer pump running. We have to drive the frame loop ourselves
     * until stop() flips runLoop, otherwise menu:run() returns into a
     * dead RunProgramStartMenu loop and stratagusMain falls into Exit(0)
     * while the LCD never receives the first menu paint. */
    if (loop) {
        while (this->runLoop) {
            UpdateDisplay();
            RealizeVideoMemory();
            WaitEventsOneFrame();
        }
    }
    return this->loopResult;
}

void ::MenuScreen::stop(int result, bool stopAll)
{
    uint32_t now = g_war1_frame_counter;
    if (now - s_menu_open_frame < 5 && !stopAll) {
        return;
    }
    if (Gui) {
        Gui->setTop(this->oldtop);
    }
    if (!MenuStack.empty() && MenuStack.top() == this) {
        MenuStack.pop();
    }
    if (stopAll) {
        /* Flip runLoop=false on every nested menu before popping. The
         * actually-blocking C++ stack frame is whichever menu was opened
         * with menu:run(true). Without this, the parent's while(runLoop)
         * loop keeps spinning after stopAll, never returning into the
         * script that triggered it. */
        while (!MenuStack.empty()) {
            ::MenuScreen *m = MenuStack.top();
            if (m) m->runLoop = false;
            MenuStack.pop();
        }
    }
    if (MenuStack.empty()) {
        GamePaused = false;
    }
    this->runLoop = false;
    this->loopResult = result;
}

void ::MenuScreen::addLogicCallback(LuaActionListener *listener)
{
    logiclistener = listener;
}

/* Close whatever menu is currently on top — used by the START-toggles-pause
 * trigger so pressing START a second time tears down the menu instead of
 * leaving it sitting over the game. Walks the MenuStack to find the
 * MenuScreen Gui currently shows and calls stop() on it. */
/* Used by WaitEventsOneFrame to gate "menu is active" input handling.
 * Returning true blocks game-world clicks (so widgets get the click
 * instead). StartMap's leak-container goes via Gui->setTop() but is NOT
 * pushed onto MenuStack — only real MenuScreen::run calls push — so this
 * cleanly distinguishes "menu open" from "Gui has a sentinel top". */
extern "C" int war1_menu_stack_active(void)
{
    return !MenuStack.empty();
}

extern "C" void war1_close_active_menu(void)
{
    if (!Gui) return;
    if (!MenuStack.empty()) {
        ::MenuScreen *m = MenuStack.top();
        if (m) m->stop(0, true /*stopAll*/);
    }
    Gui->setTop(nullptr);
}

/* Click dispatch — bypasses gcn::Gui::distributeMouseEvent (which hangs
 * in our embedded build via accumulated FocusHandler::mWidgets state).
 * Walks the menu's children, finds whichever widget contains the
 * cursor's absolute screen position, and fires its action listeners
 * directly. That covers ButtonWidget (each one has a LuaActionListener
 * registered via setActionCallback that runs the callback function the
 * Lua menu code set on it). Returns 1 if a button was clicked. */
/* Construction helpers exposed to widget bindings. */
extern "C" gcn::Widget *war1_new_multilinelabel(const char *caption) {
    return new War1MultiLineLabel(caption ? caption : "");
}
extern "C" void war1_mll_set_caption(gcn::Widget *w, const char *caption) {
    if (auto *m = dynamic_cast<War1MultiLineLabel *>(w))
        m->setCaption(caption ? caption : "");
}
extern "C" const char *war1_mll_get_caption(gcn::Widget *w) {
    if (auto *m = dynamic_cast<War1MultiLineLabel *>(w))
        return m->getCaption().c_str();
    return "";
}
extern "C" void war1_mll_set_alignment(gcn::Widget *w, int a) {
    if (auto *m = dynamic_cast<War1MultiLineLabel *>(w)) m->setAlignment(a);
}
extern "C" void war1_mll_set_vertical_alignment(gcn::Widget *w, int a) {
    if (auto *m = dynamic_cast<War1MultiLineLabel *>(w)) m->setVerticalAlignment(a);
}
extern "C" void war1_mll_set_line_width(gcn::Widget *w, int width) {
    if (auto *m = dynamic_cast<War1MultiLineLabel *>(w)) m->setLineWidth(width);
}
extern "C" void war1_mll_adjust_size(gcn::Widget *w) {
    if (auto *m = dynamic_cast<War1MultiLineLabel *>(w)) m->adjustSize();
}

/* ScrollingWidget bridge — wires Lua to upstream's real ScrollingWidget. */
extern "C" gcn::Widget *war1_new_scrolling_widget(int w, int h) {
    return new ::ScrollingWidget(w, h);
}
extern "C" void war1_sw_add(gcn::Widget *sw, gcn::Widget *child, int x, int y) {
    if (auto *s = dynamic_cast< ::ScrollingWidget *>(sw)) s->add(child, x, y);
}
extern "C" void war1_sw_set_speed(gcn::Widget *sw, float speed) {
    if (auto *s = dynamic_cast< ::ScrollingWidget *>(sw)) s->setSpeed(speed);
}
extern "C" void war1_sw_restart(gcn::Widget *sw) {
    if (auto *s = dynamic_cast< ::ScrollingWidget *>(sw)) s->restart();
}

/* ImageButton bridge — wires Lua to upstream's real CImageButton. The
 * Lua-side `bg:setNormalImage(cgraphic)` passes a CGraphic which we wrap
 * in a JupiterImage shared_ptr (same arg_to_image path used everywhere
 * else for CGraphic→Image conversion) and hand to upstream's
 * CImageButton::setNormalImage. */
extern "C" gcn::Widget *war1_new_image_button(const char *caption) {
    return new ::CImageButton(caption ? caption : "");
}
/* Real bridge lives in war1_widget_bindings.cpp where arg_to_image is.
 * Keeping the symbol declared so the binding can call it. */
extern "C" void war1_imgbtn_set_normal_image_shared(gcn::Widget *w, std::shared_ptr<gcn::Image> img) {
    if (auto *b = dynamic_cast< ::CImageButton *>(w)) b->setNormalImage(img);
}

extern "C" int war1_menu_click(int absX, int absY)
{
    if (!Gui) return 0;
    gcn::Widget *top = Gui->getTop();
    if (!top) return 0;
    int topAbsX, topAbsY;
    top->getAbsolutePosition(topAbsX, topAbsY);
    /* Walk down the tree to the deepest widget covering (absX, absY). */
    gcn::Widget *hit = nullptr;
    int hitAbsX = topAbsX, hitAbsY = topAbsY;
    {
        gcn::Widget *parent = top;
        while (parent) {
            int pAx, pAy;
            parent->getAbsolutePosition(pAx, pAy);
            gcn::Widget *child = parent->getWidgetAt(absX - pAx, absY - pAy);
            if (!child) break;
            hit = child;
            child->getAbsolutePosition(hitAbsX, hitAbsY);
            parent = child;
        }
    }
    if (!hit) return 0;
    int handled = 0;
    /* Mouse listeners first — gcn::ListBox / gcn::CheckBox / etc.
     * register themselves as mouse listeners and react to clicks via
     * their own mousePressed override (e.g. ListBox::mousePressed sets
     * selected row from event.getY()). Construct a MouseEvent in the
     * widget's local coords and dispatch. */
    {
        const auto &mlisteners = hit->_getMouseListeners();
        if (!mlisteners.empty()) {
            gcn::MouseEvent me(
                hit, hit, false, false, false, false,
                gcn::MouseEvent::Pressed, gcn::MouseEvent::Left,
                absX - hitAbsX, absY - hitAbsY, 1);
            for (gcn::MouseListener *l : mlisteners) {
                l->mousePressed(me);
            }
            handled = 1;
        }
    }
    /* Action listeners — buttons fire on press via addActionListener
     * (set up by setActionCallback on the Lua side). ImageWidget for
     * the panel bg has none, so clicks on bg do nothing. */
    {
        const auto &alisteners = hit->_getActionListeners();
        if (!alisteners.empty()) {
            gcn::ActionEvent event(hit, "");
            for (gcn::ActionListener *l : alisteners) {
                l->action(event);
            }
            handled = 1;
        }
    }
    return handled;
}

void ::MenuScreen::draw(gcn::Graphics *graphics) /* override */
{
    if (this->drawUnder && this->oldtop) {
        gcn::Rectangle r = graphics->getCurrentClipArea();
        graphics->popClipArea();
        oldtop->_draw(graphics);
        graphics->pushClipArea(r);
    }
    gcn::Container::draw(graphics);
}

void ::MenuScreen::logic() /* override */
{
    if (logiclistener) {
        logiclistener->action(gcn::ActionEvent{this, ""});
    }
    gcn::Container::logic();
}
