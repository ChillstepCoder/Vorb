#include "Vorb/stdafx.h"
#include "Vorb/ui/MouseEventManager.h"

void vorb::ui::MouseEventManager::getPosition(i32* x, i32* y) const {
    *x = std::atomic_load(&m_x);
    *y = std::atomic_load(&m_y);
}
bool vorb::ui::MouseEventManager::hasFocus() const {
    return std::atomic_load(&m_focus) != 0;
}
bool vorb::ui::MouseEventManager::isRelative() const {
    return std::atomic_load(&m_relative) != 0;
}
bool vorb::ui::MouseEventManager::isHidden() const {
    return std::atomic_load(&m_hidden) != 0;
}

bool vorb::ui::MouseEventManager::isButtonPressed(MouseButton button) const {
    return m_state[static_cast<int>(button)];
}

void vui::MouseEventManager::setPos(i32 x, i32 y) {
    std::atomic_store(&m_x, x);
    std::atomic_store(&m_y, y);
}
void vui::MouseEventManager::setFocus(bool v) {
    std::atomic_store(&m_focus, v ? 1 : 0);
}
void vui::MouseEventManager::setRelative(bool v) {
    std::atomic_store(&m_relative, v ? 1 : 0);
}
void vui::MouseEventManager::setHidden(bool v) {
    std::atomic_store(&m_hidden, v ? 1 : 0);
}
