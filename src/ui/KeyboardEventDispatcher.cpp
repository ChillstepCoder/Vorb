#include "Vorb/stdafx.h"
#include "Vorb/ui/KeyboardEventDispatcher.h"

vui::KeyboardEventDispatcher::KeyboardEventDispatcher() {
    for (auto& key : m_presses) std::atomic_init(&key, 0);
    memset(m_state, 0, NUM_KEY_CODES * sizeof(bool));

    onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {
        if (event.keyCode > 0 && event.keyCode < VKEY_HIGHEST_VALUE) {
            m_state[event.keyCode] = true;
        }
    });

    onKeyUp.addFunctor([this](Sender sender, const vui::KeyEvent& event) {
        if (event.keyCode > 0 && event.keyCode < VKEY_HIGHEST_VALUE) {
            m_state[event.keyCode] = false;
        }
    });
}

i32 vui::KeyboardEventDispatcher::getNumPresses(VirtualKey k) const {
    return std::atomic_load(&m_presses[k]);
}
bool vui::KeyboardEventDispatcher::hasFocus() const {
    return std::atomic_load(&m_focus) != 0;
}

bool vui::KeyboardEventDispatcher::isKeyPressed(VirtualKey k) const {
    return m_state[k];
}

void vui::KeyboardEventDispatcher::addPress(VirtualKey k) {
    m_presses[k]++; // ++ is overloaded as an atomic operation so this is (read, should be) fine.
}
void vui::KeyboardEventDispatcher::release(VirtualKey k) {
    std::atomic_store(&m_presses[k], 0);
}
