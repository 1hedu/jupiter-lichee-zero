/* Jupiter backend for guisan: Input.
 *
 * Drains keyboard + mouse events from our N64 input poll into guisan's
 * KeyInput / MouseInput queues. _pollInput() is called once per frame
 * by gcn::Gui::logic(); it should push any new key/mouse events into
 * the queues. dequeue* / isEmpty* are then called by gcn::Gui to
 * deliver the events to focused widgets.
 *
 * Mouse: cursor position comes from CursorScreenPos (Stratagus's global
 *        already updated by our InputMouseMove). Buttons come from
 *        BTN_B (LMB) and BTN_A (RMB) edges.
 * Keys:  not yet wired — guisan menus mostly need mouse; key events
 *        added later if the menu has shortcut support that matters.
 */
#ifndef JUPITER_INPUT_HPP
#define JUPITER_INPUT_HPP

#include <guisan/input.hpp>
#include <guisan/keyinput.hpp>
#include <guisan/mouseinput.hpp>
#include <queue>

namespace gcn {

class JupiterInput : public Input {
public:
    bool isKeyQueueEmpty() override;
    KeyInput dequeueKeyInput() override;
    bool isMouseQueueEmpty() override;
    MouseInput dequeueMouseInput() override;
    void _pollInput() override;

    /* Push events from the WaitEventsOneFrame layer. Lets us share the
     * one input poll our existing Stratagus event loop already does
     * instead of polling twice. */
    void pushMouseMove(int x, int y);
    void pushMouseButton(int x, int y, int button, bool pressed);

private:
    std::queue<MouseInput> mMouseQueue;
    std::queue<KeyInput>   mKeyQueue;
};

/* Singleton accessor — the Stratagus widget integration creates one
 * Input instance and binds it to gcn::Gui. We expose the pointer so
 * the input bridge can call pushMouseMove etc. from outside the class. */
JupiterInput *getJupiterInput();

} /* namespace gcn */

#endif /* JUPITER_INPUT_HPP */
