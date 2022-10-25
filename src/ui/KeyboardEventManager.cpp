#include "Vorb/stdafx.h"
#include "Vorb/ui/KeyboardEventManager.h"

vui::KeyboardEventManager::KeyboardEventManager() {
    for (auto& key : m_presses) std::atomic_init(&key, 0);
    memset(m_state, 0, NUM_KEY_CODES * sizeof(bool));

    addKeyDownListener([this](const vui::KeyEvent& event) {
        if (event.keyCode > 0 && event.keyCode < VKEY_HIGHEST_VALUE) {
            m_state[event.keyCode] = true;
        }
    });
    addKeyUpListener([this](const vui::KeyEvent& event) {
        if (event.keyCode > 0 && event.keyCode < VKEY_HIGHEST_VALUE) {
            m_state[event.keyCode] = false;
        }
    });

}

i32 vui::KeyboardEventManager::getNumPresses(VirtualKey k) const {
    return std::atomic_load(&m_presses[k]);
}
bool vui::KeyboardEventManager::hasFocus() const {
    return std::atomic_load(&m_focus) != 0;
}

bool vui::KeyboardEventManager::isKeyPressed(VirtualKey k) const {
    return m_state[k];
}

void vui::KeyboardEventManager::addPress(VirtualKey k) {
    m_presses[k]++; // ++ is overloaded as an atomic operation so this is (read, should be) fine.
}
void vui::KeyboardEventManager::release(VirtualKey k) {
    std::atomic_store(&m_presses[k], 0);
}
