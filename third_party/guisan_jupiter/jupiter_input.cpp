/* Jupiter Input backend — see header.
 *
 * IMPORTANT: gcn::Gui::logic() is only invoked when a guisan menu is the
 * active top widget (see DrawGuichanWidgets). When no menu is open
 * nothing drains mMouseQueue / mKeyQueue, so pushing every frame's
 * cursor coords would let the queue grow unbounded — by the time a menu
 * eventually opens, the first logic() spins for ages processing the
 * backlog. We gate pushes on "is there an active consumer?" and drop
 * any events that would otherwise pile up. */
#include "jupiter_input.hpp"
#include <guisan/gui.hpp>
#include <memory>

/* The Gui global is at file scope (declared in war1_widgets.cpp). */
extern std::unique_ptr<gcn::Gui> Gui;

namespace gcn {

static JupiterInput *s_jupiter_input = nullptr;
JupiterInput *getJupiterInput() {
    if (!s_jupiter_input) s_jupiter_input = new JupiterInput();
    return s_jupiter_input;
}

/* Cap the queue so a runaway producer never starves logic. Even when
 * a menu is open, more than ~256 unprocessed events between frames
 * means the producer is firing too fast — drop the oldest. */
static constexpr size_t kMaxQueued = 256;

static bool consumer_ready() {
    /* `::Gui` to disambiguate from gcn::Gui (the class) inside this ns. */
    return ::Gui && ::Gui->getTop() != nullptr;
}

bool JupiterInput::isKeyQueueEmpty()    { return mKeyQueue.empty(); }
bool JupiterInput::isMouseQueueEmpty()  { return mMouseQueue.empty(); }

KeyInput JupiterInput::dequeueKeyInput() {
    if (mKeyQueue.empty()) return KeyInput();
    KeyInput k = mKeyQueue.front();
    mKeyQueue.pop();
    return k;
}

MouseInput JupiterInput::dequeueMouseInput() {
    if (mMouseQueue.empty()) return MouseInput();
    MouseInput m = mMouseQueue.front();
    mMouseQueue.pop();
    return m;
}

void JupiterInput::_pollInput() {}

void JupiterInput::pushMouseMove(int x, int y) {
    if (!consumer_ready()) return;
    while (mMouseQueue.size() >= kMaxQueued) mMouseQueue.pop();
    MouseInput m;
    m.setX(x);
    m.setY(y);
    m.setButton(MouseInput::Empty);
    m.setType(MouseInput::Moved);
    m.setTimeStamp(0);
    mMouseQueue.push(m);
}

void JupiterInput::pushMouseButton(int x, int y, int button, bool pressed) {
    if (!consumer_ready()) return;
    while (mMouseQueue.size() >= kMaxQueued) mMouseQueue.pop();
    MouseInput m;
    m.setX(x);
    m.setY(y);
    int gcnBtn = MouseInput::Empty;
    if      (button == 1) gcnBtn = MouseInput::Left;
    else if (button == 2) gcnBtn = MouseInput::Middle;
    else if (button == 3) gcnBtn = MouseInput::Right;
    m.setButton(gcnBtn);
    m.setType(pressed ? MouseInput::Pressed : MouseInput::Released);
    m.setTimeStamp(0);
    mMouseQueue.push(m);
}

} /* namespace gcn */
