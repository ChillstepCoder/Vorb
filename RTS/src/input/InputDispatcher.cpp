#include "stdafx.h"
#include "InputDispatcher.h"

#include <stdexcept>

#include "input/InputDispatcherEventCatcher.h"
#include "ui/GameWindow.h"
#include "Vorb/ui/KeyMappings.inl"

#include <SDL2/SDL.h>

#include "ui/noesis/NoesisGuiContext.h"

#if defined(VORB_IMPL_IMGUI)
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#endif

vui::MouseEventManager vui::InputDispatcher::mouse;
vui::KeyboardEventManager vui::InputDispatcher::key;
vui::WindowEventManager vui::InputDispatcher::window;
eventpp::CallbackList<void()> vui::InputDispatcher::onQuit;
volatile bool vui::InputDispatcher::m_isInit = false;
vui::GameWindow* vui::InputDispatcher::m_window = nullptr;

void vui::InputDispatcher::init(GameWindow* w) {
    if (m_isInit) throw std::runtime_error("Input dispatcher is already initialized");
    m_isInit = true;
    m_window = w;

    SDL_SetEventFilter(impl::InputDispatcherEventCatcher::onSDLEvent, nullptr);
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    SDL_StartTextInput();

    // Clear values
    memset(key.m_state, 0, sizeof(key.m_state));
    mouse.m_fullScroll = i32v2(0, 0);
    mouse.m_lastPos = i32v2(0, 0);
}
void vui::InputDispatcher::dispose() {
    if (!m_isInit) return;
    m_isInit = false;
    SDL_StopTextInput();
    SDL_SetEventFilter(nullptr, nullptr);
    m_window = nullptr;
}

/// Memory-efficient way to split through multiple event types
struct InputEvent {
    union {
        vui::MouseEvent mouse;
        vui::MouseButtonEvent mouseButton;
        vui::MouseMotionEvent mouseMotion;
        vui::MouseWheelEvent mouseWheel;
        vui::KeyEvent key;
        vui::TextEvent text;
        vui::WindowEvent windowEvent;
        vui::WindowResizeEvent windowResize;
        vui::WindowFileEvent windowFile;
    };
};

void vui::InputDispatcher::injectMouseButtonEvent(i32 x, i32 y, MouseButton button, ui8 clicks, bool pressed) {
    InputEvent ie{};
    ie.mouseButton.x = x;
    ie.mouseButton.y = y;
    ie.mouseButton.button = button;
    ie.mouseButton.clicks = clicks;
    if (pressed) {
        vui::InputDispatcher::mouse.dispatchButtonDown(ie.mouseButton);
    }
    else {
        vui::InputDispatcher::mouse.dispatchButtonUp(ie.mouseButton);
    }
    vui::InputDispatcher::mouse.m_state[static_cast<int>(ie.mouseButton.button)] = pressed;
}

void convert(vui::KeyModifiers& km, const ui16& sm) {
#define MASK_BOOL(F) ((F) == 0) ? false : true;
    km.lAlt = MASK_BOOL(sm & KMOD_LALT);
    km.rAlt = MASK_BOOL(sm & KMOD_RALT);
    km.lCtrl = MASK_BOOL(sm & KMOD_LCTRL);
    km.rCtrl = MASK_BOOL(sm & KMOD_RCTRL);
    km.lGUI = MASK_BOOL(sm & KMOD_LGUI);
    km.rGUI = MASK_BOOL(sm & KMOD_RGUI);
    km.lShift = MASK_BOOL(sm & KMOD_LSHIFT);
    km.rShift = MASK_BOOL(sm & KMOD_RSHIFT);
    km.caps = MASK_BOOL(sm & KMOD_CAPS);
    km.num = MASK_BOOL(sm & KMOD_NUM);
#undef MASK_BOOL
}
void convert(vui::MouseButton& mb, const ui8& sb) {
    switch (sb) {
    case SDL_BUTTON_LEFT:
        mb = vui::MouseButton::LEFT;
        break;
    case SDL_BUTTON_MIDDLE:
        mb = vui::MouseButton::MIDDLE;
        break;
    case SDL_BUTTON_RIGHT:
        mb = vui::MouseButton::RIGHT;
        break;
    case SDL_BUTTON_X1:
        mb = vui::MouseButton::X1;
        break;
    case SDL_BUTTON_X2:
        mb = vui::MouseButton::X2;
        break;
    default:
        mb = vui::MouseButton::UNKNOWN;
        break;
    }
}

i32 vui::impl::InputDispatcherEventCatcher::onSDLEvent(void*, SDL_Event* e) {
    InputEvent ie{};
    bool suppressKeyboard = false;
    bool suppressMouse = false;

    // Imgui
#ifdef VORB_IMPL_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplSDL2_ProcessEvent(e);
    if (io.WantCaptureKeyboard) {
        suppressKeyboard = true;
    }
    if (io.WantCaptureMouse) {
        suppressMouse = true;
    }
#endif

    // Noessis GUI
    if (sNoesisGuiContext) {
        sNoesisGuiContext->processInput(e);
    }

    // Main application
    switch (e->type) {
    case SDL_KEYDOWN:
        if (suppressKeyboard) return 0;
        convert(ie.key.mod, e->key.keysym.mod);
        ie.key.keyCode = vui::impl::mapping[SDL_GetScancodeFromKey(e->key.keysym.sym) + 1];
        ie.key.scanCode = e->key.keysym.scancode;
        ie.key.repeatCount = e->key.repeat;
        vui::InputDispatcher::key.m_state[ie.key.keyCode] = true;
        vui::InputDispatcher::key.dispatchKeyDown(ie.key);
        break;
    case SDL_KEYUP:
        if (suppressKeyboard) return 0;
        convert(ie.key.mod, e->key.keysym.mod);
        ie.key.keyCode = vui::impl::mapping[SDL_GetScancodeFromKey(e->key.keysym.sym) + 1];
        ie.key.scanCode = e->key.keysym.scancode;
        ie.key.repeatCount = e->key.repeat;
        vui::InputDispatcher::key.m_state[ie.key.keyCode] = false;
        vui::InputDispatcher::key.dispatchKeyUp(ie.key);
        break;
    case SDL_MOUSEMOTION:
        if (suppressMouse) return 0;
        ie.mouseMotion.x = e->motion.x;
        ie.mouseMotion.y = e->motion.y;
        ie.mouseMotion.dx = e->motion.xrel;
        ie.mouseMotion.dy = e->motion.yrel;
        vui::InputDispatcher::mouse.m_lastPos.x = ie.mouseMotion.x;
        vui::InputDispatcher::mouse.m_lastPos.y = ie.mouseMotion.y;
        vui::InputDispatcher::mouse.dispatchMotion(ie.mouseMotion);
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (suppressMouse) return 0;
        convert(ie.mouseButton.button, e->button.button);
        ie.mouseButton.x = e->button.x;
        ie.mouseButton.y = e->button.y;
        ie.mouseButton.clicks = e->button.clicks;
        vui::InputDispatcher::mouse.dispatchButtonDown(ie.mouseButton);
        vui::InputDispatcher::mouse.m_state[static_cast<int>(ie.mouseButton.button)] = true;
        break;
    case SDL_MOUSEBUTTONUP:
        if (suppressMouse) return 0;
        convert(ie.mouseButton.button, e->button.button);
        ie.mouseButton.x = e->button.x;
        ie.mouseButton.y = e->button.y;
        ie.mouseButton.clicks = e->button.clicks;
        vui::InputDispatcher::mouse.dispatchButtonUp(ie.mouseButton);
        vui::InputDispatcher::mouse.m_state[static_cast<int>(ie.mouseButton.button)] = false;
        break;
    case SDL_MOUSEWHEEL:
        if (suppressMouse) return 0;
        ie.mouseWheel.x = vui::InputDispatcher::mouse.m_lastPos.x;
        ie.mouseWheel.y = vui::InputDispatcher::mouse.m_lastPos.y;
        ie.mouseWheel.dx = e->wheel.x;
        ie.mouseWheel.dy = e->wheel.y;
        vui::InputDispatcher::mouse.m_fullScroll.x += ie.mouseWheel.dx;
        vui::InputDispatcher::mouse.m_fullScroll.y += ie.mouseWheel.dy;
        ie.mouseWheel.sx = vui::InputDispatcher::mouse.m_fullScroll.x;
        ie.mouseWheel.sy = vui::InputDispatcher::mouse.m_fullScroll.y;
        vui::InputDispatcher::mouse.dispatchWheel(ie.mouseWheel);
        break;
    case SDL_QUIT:
        vui::InputDispatcher::onQuit();
        break;
    case SDL_WINDOWEVENT:
        switch (e->window.event) {
            case SDL_WINDOWEVENT_CLOSE:
                vui::InputDispatcher::window.dispatchClose(ie.windowEvent);
                break;
            case SDL_WINDOWEVENT_RESIZED:
                ie.windowResize.w = e->window.data1;
                ie.windowResize.h = e->window.data2;
                vui::InputDispatcher::window.mCurrentDims = ui32v2(ie.windowResize.w, ie.windowResize.h);
                vui::InputDispatcher::window.dispatchResize(ie.windowResize);
                break;
            case SDL_WINDOWEVENT_ENTER:
                // We must poll this one instance
                {
                    POINT mp;
                    GetCursorPos(&mp);
                    i32v2 wp = vui::InputDispatcher::m_window->getPosition();
                    vui::InputDispatcher::mouse.m_lastPos.x = mp.x - wp.x;
                    vui::InputDispatcher::mouse.m_lastPos.y = mp.y - wp.y;
                }
  
                ie.mouse.x = vui::InputDispatcher::mouse.m_lastPos.x;
                ie.mouse.y = vui::InputDispatcher::mouse.m_lastPos.y;
                //vui::InputDispatcher::mouse.dispatchFocusGained(ie.mouse);
                break;
            case SDL_WINDOWEVENT_LEAVE:
                ie.mouse.x = vui::InputDispatcher::mouse.m_lastPos.x;
                ie.mouse.y = vui::InputDispatcher::mouse.m_lastPos.y;
                //vui::InputDispatcher::mouse.dispatchFocusLost(ie.mouse);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                vui::InputDispatcher::key.dispatchFocusGained();
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                vui::InputDispatcher::key.dispatchFocusLost();
                break;
            default:
                // Unrecognized window event
                return 1;
        }
        break;
    case SDL_TEXTINPUT:
        if (suppressKeyboard) return 0;
        memcpy(ie.text.text, e->text.text, MAX_TEXT_EVENT_SIZE);
        size_t convertedCount;
        mbstowcs_s(&convertedCount, ie.text.wtext, ie.text.text, MAX_TEXT_EVENT_SIZE >> 1);
        vui::InputDispatcher::key.dispatchText(ie.text);
        break;
    case SDL_DROPFILE:
        ie.windowFile.file = e->drop.file;
        vui::InputDispatcher::window.dispatchFile(ie.windowFile);
        SDL_free(e->drop.file);
        break;
    default:
        // Unrecognized event
        return 1;
    }
    return 0;
}
