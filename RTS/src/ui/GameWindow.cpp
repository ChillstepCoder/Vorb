#include "stdafx.h"
#include "GameWindow.h"

#ifndef VORB_USING_PCH
#include <iostream>
#include <fstream>

#include <GL/glew.h>
#endif // !VORB_USING_PCH

#include <SDL2/SDL.h>
#define VUI_WINDOW_HANDLE(WINDOW_VAR) ( (SDL_Window*) WINDOW_VAR )

#include "Vorb/io/IOManager.h"
#include "input/InputDispatcher.h"

#include <Vorb/logging/Logger.h>

#include <thread>

#define DEFAULT_WINDOW_FLAGS (SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN)

#if defined(VORB_IMPL_IMGUI)
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#endif

vui::GameWindow* sMainGameWindowHandle = nullptr;

SERIALIZABLE_ENUM(vui::GameSwapInterval, GameSaveInterval,
    pair{ vui::GameSwapInterval::UNLIMITED_FPS, "Unlimited"sv },
    pair{ vui::GameSwapInterval::V_SYNC, "VSync"sv },
    pair{ vui::GameSwapInterval::LOW_SYNC, "LowSync"sv },
    pair{ vui::GameSwapInterval::POWER_SAVER, "PowerSaver"sv },
    pair{ vui::GameSwapInterval::USE_VALUE_CAP, "ValueCap"sv }
);

namespace vorb {
    namespace ui {
        SERIALIZABLE_SIMPLE(vui::GameDisplayMode,
            make_field(o.screenWidth, "ScreenWidth"),
            make_field(o.screenHeight, "ScreenHeight"),
            make_field(o.isFullscreen, "IsFullscreen"),
            make_field(o.isBorderless, "IsBorderless"),
            make_field(o.swapInterval, "SwapInterval"),
            make_field(o.maxFPS, "MaxFPS"),
            make_field(o.major, "GraphicsMajor"),
            make_field(o.minor, "GraphicsMinor"),
            make_field(o.core, "GraphicsCore")
        );
    }
}

vui::GameWindow::GameWindow() {
    setDefaultSettings(&m_displayMode);
}
//VORB_MOVABLE_DEF(vui::GameWindow, o) {
//    std::swap(m_glc, o.m_glc);
//    std::swap(m_window, o.m_window);
//    std::swap(m_displayMode, o.m_displayMode);
//    std::swap(m_quitSignal, o.m_quitSignal);
//
//    // Swap events, but keep correct senders
//    std::swap(onQuit, o.onQuit);
//    this->onQuit.setSender(this);
//    o.onQuit.setSender(&o);
//    return *this;
//}

bool vui::GameWindow::init(bool isResizable /*= true*/, bool isDebug /*= false*/) {
    if (isInitialized()) return false;
    m_displayMode.isResizable = isResizable;

    // Attempt to read custom settings
    readSettings();

    SDL_WindowFlags flags = (SDL_WindowFlags)DEFAULT_WINDOW_FLAGS;
    if (m_displayMode.isResizable) flags = (SDL_WindowFlags)(flags | SDL_WINDOW_RESIZABLE);
    if (m_displayMode.isBorderless) flags = (SDL_WindowFlags)(flags | SDL_WINDOW_BORDERLESS);
    if (m_displayMode.isFullscreen) flags = (SDL_WindowFlags)(flags | SDL_WINDOW_FULLSCREEN);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    m_window = SDL_CreateWindow(DEFAULT_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_displayMode.screenWidth, m_displayMode.screenHeight, flags);

    // Create The Window
    if (m_window == nullptr) {
        VORB_LOG_CRITICAL("Window Creation Failed");
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, (int)m_displayMode.major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (int)m_displayMode.minor);
    if (isDebug) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }
    if (m_displayMode.core) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    } else {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    }
    m_glc = SDL_GL_CreateContext(VUI_WINDOW_HANDLE(m_window));
    SDL_GL_MakeCurrent(VUI_WINDOW_HANDLE(m_window), (SDL_GLContext)m_glc);

    // Check for a valid context
    if (m_glc == nullptr) {
        VORB_LOG_CRITICAL("{}", SDL_GetError());
        std::cout << "Enter any key to exit...\n";
        int c;
        std::cin >> c;
        return false;
    }

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        VORB_LOG_CRITICAL("Glew failed to initialize. Your graphics card is probably WAY too old. Try updating drivers?");
        return false;
    }

    // Create default clear values
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);

    // Initialize Frame Buffer
    glViewport(0, 0, getWidth(), getHeight());

    { // Get supported window resolutions
        SDL_DisplayMode mode;
        // TODO: Handle other displays indices.
        int displayIndex = 0;
        int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
        for (int i = 0; i < numDisplayModes; i++) {
            SDL_GetDisplayMode(displayIndex, i, &mode);
            ui32v2 res(mode.w, mode.h);
            if (i == 0 || m_supportedResolutions.back() != res) {
                m_supportedResolutions.push_back(res);
            }
        }
    }

    // Set More Display Settings
    setSwapInterval(m_displayMode.swapInterval, true);

    // Push input from this window and receive quit signals
    vui::InputDispatcher::init(this);
    vui::InputDispatcher::window.addCloseListener([this](const WindowEvent&) { onQuitSignal(); });
    vui::InputDispatcher::window.addResizeListener([this](const WindowResizeEvent& e) { onResize(e); });
    vui::InputDispatcher::window.mCurrentDims = ui32v2(getWidth(), getHeight());
    vui::InputDispatcher::onQuit.append([this]() { onQuitSignal(); });
    m_quitSignal = false;

#ifdef VORB_IMPL_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 440";
    // Make sure we are using the right gl version (4.5)
    const ui32 minor = getGLMinorVersion();
    const ui32 major = getGLMajorVersion();
    VORB_LOG_INFO("Initializing opengl for imgui with minor {} and major {}", minor, major);
    //assert(minor == 6 && major == 4 && "App.config needs opengl set to 4.6\n");
    ImGui_ImplSDL2_InitForOpenGL(static_cast<SDL_Window*>(m_window), m_glc);
    ImGui_ImplOpenGL3_Init(glsl_version);
#endif

    return true;
}
void vui::GameWindow::dispose() {
    if (!isInitialized()) return;
    vui::InputDispatcher::dispose();
    saveSettings();

    if (m_glc) {
        SDL_GL_DeleteContext((SDL_GLContext)m_glc);
        delete m_glc;
    }
    if (m_window) {
        SDL_DestroyWindow(VUI_WINDOW_HANDLE(m_window));
    }

    // Get rid of dangling pointers
    m_window = nullptr;
    m_glc = nullptr;
}

void vui::GameWindow::setDefaultSettings(GameDisplayMode* mode) {
    mode->screenWidth = DEFAULT_WINDOW_WIDTH;
    mode->screenHeight = DEFAULT_WINDOW_HEIGHT;
    mode->isBorderless = false;
    mode->isFullscreen = false;
    mode->isResizable = true;
    mode->maxFPS = DEFAULT_MAX_FPS;
    mode->swapInterval = DEFAULT_SWAP_INTERVAL;
    mode->major = 4;
    mode->minor = 6;
    mode->core = true;
}
void vui::GameWindow::readSettings() {
    vio::IOManager iom;
    nString data;
    iom.readFileToString(DEFAULT_APP_CONFIG_FILE, data);

    if (data.size()) {
        YmlSerializer::readFileData(data, m_displayMode);
    } else {
        // If there is no app.config, save a default one.
        saveSettings();
    }
}
void vui::GameWindow::saveSettings() const {
    VORB_LOG_CRITICAL("FIX saveSettings()");

    vio::Path filePath = DEFAULT_APP_CONFIG_FILE;

    ryml::Tree tree;
    ryml::NodeRef root = tree.rootref();
    root |= ryml::MAP;
    root << m_displayMode;
    std::stringstream ss;
    ss << tree;

    std::ofstream file(DEFAULT_APP_CONFIG_FILE);
    if (file.fail()) {
        panic("Failed to open {} for write", DEFAULT_APP_CONFIG_FILE);
    }
    file << ss.str() << std::endl;
    file.close();
}

void vui::GameWindow::setScreenSize(i32 w, i32 h, bool overrideCheck /*= false*/) {
    // Apply A Minimal State Change
    if ((overrideCheck || m_displayMode.screenWidth != w || m_displayMode.screenHeight != h) && !m_displayMode.isFullscreen){
        m_displayMode.screenWidth = w;
        m_displayMode.screenHeight = h;
        SDL_SetWindowSize(VUI_WINDOW_HANDLE(m_window), m_displayMode.screenWidth, m_displayMode.screenHeight);
    }
}
void vui::GameWindow::setFullscreen(bool useFullscreen, bool overrideCheck /*= false*/) {
    if (overrideCheck || m_displayMode.isFullscreen != useFullscreen) {
        m_displayMode.isFullscreen = useFullscreen;
        SDL_SetWindowFullscreen(VUI_WINDOW_HANDLE(m_window), m_displayMode.isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }
}
void vui::GameWindow::setBorderless(bool useBorderless, bool overrideCheck /*= false*/) {
    if ((overrideCheck || m_displayMode.isBorderless != useBorderless) && !m_displayMode.isFullscreen) {
        m_displayMode.isBorderless = useBorderless;
        SDL_SetWindowBordered((SDL_Window*)m_window, m_displayMode.isBorderless ? SDL_FALSE : SDL_TRUE);
    }
}

void vui::GameWindow::setSwapInterval(GameSwapInterval mode, bool overrideCheck /*= false*/) {
    if (overrideCheck || m_displayMode.swapInterval != mode) {
        m_displayMode.swapInterval = mode;
        switch (m_displayMode.swapInterval) {
        case GameSwapInterval::UNLIMITED_FPS:
        case GameSwapInterval::USE_VALUE_CAP:
            SDL_GL_SetSwapInterval(0);
            break;
        default:
            SDL_GL_SetSwapInterval(static_cast<i32>(DEFAULT_SWAP_INTERVAL));
            break;
        }
    }
}

void vui::GameWindow::setTemporaryUnlimitedFPS(bool unlimitedFPS) {
    if (m_displayMode.temporaryUnlimitedFPS != unlimitedFPS) {
        m_displayMode.temporaryUnlimitedFPS = unlimitedFPS;
        if (unlimitedFPS) {
            SDL_GL_SetSwapInterval(0);
        }
        else {
            setSwapInterval(m_displayMode.swapInterval, true);
        }
    }
}

void vui::GameWindow::setHideMouse(bool mouseHide) {
    SDL_ShowCursor(!mouseHide);
}

void vui::GameWindow::setRelativeMouseMode(bool relativeMouse) {
    SDL_SetRelativeMouseMode(relativeMouse ? SDL_TRUE : SDL_FALSE);
}

void vui::GameWindow::warpMouse(int x, int y) {
    SDL_WarpMouseInWindow((SDL_Window*)m_window, x, y);
}

void vui::GameWindow::setMaxFPS(f32 fpsLimit) {
    m_displayMode.maxFPS = fpsLimit;
}


void vui::GameWindow::setTitle(const cString title) const {
    if (!title) title = DEFAULT_TITLE;
    SDL_SetWindowTitle((SDL_Window*)m_window, title);
}

void vui::GameWindow::setPosition(int x, int y) {
    SDL_SetWindowPosition((SDL_Window*)m_window, x, y);
}

void vui::GameWindow::sync(ui32 frameTime) {
    pollInput();

    SDL_GL_SwapWindow(VUI_WINDOW_HANDLE(m_window));

    // Limit FPS
    if (m_displayMode.swapInterval == GameSwapInterval::USE_VALUE_CAP) {
        f32 desiredFPS = 1000.0f / (f32)m_displayMode.maxFPS;
        ui32 sleepTime = (ui32)(desiredFPS - frameTime);
        if (desiredFPS > frameTime && sleepTime > 0) std::this_thread::sleep_for (std::chrono::milliseconds(sleepTime));
    }
}

vui::GraphicsContext vui::GameWindow::getContext() const {
    return m_glc;
}

i32 vui::GameWindow::getX() const {
    i32 v;
    SDL_GetWindowPosition(VUI_WINDOW_HANDLE(m_window), &v, nullptr);
    return v;
}
i32 vui::GameWindow::getY() const {
    i32 v;
    SDL_GetWindowPosition(VUI_WINDOW_HANDLE(m_window), nullptr, &v);
    return v;
}
i32v2 vui::GameWindow::getPosition() const {
    i32v2 v;
    SDL_GetWindowPosition(VUI_WINDOW_HANDLE(m_window), &v.x, &v.y);
    return v;
}

void vui::GameWindow::pollInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) continue;
}

f32v2 vorb::ui::GameWindow::clampBoxPosToWindow(const f32v2& boxPosTopLeft, const f32v2& boxDims) const {
    return f32v2(glm::min(boxPosTopLeft.x, (f32)(m_displayMode.screenWidth - boxDims.x)), glm::min(boxPosTopLeft.y, (f32)(m_displayMode.screenHeight - boxDims.y)));
}

void vorb::ui::GameWindow::onResize(const WindowResizeEvent& e) {
    m_displayMode.screenWidth = e.w;
    m_displayMode.screenHeight = e.h;
}

void vorb::ui::GameWindow::onQuitSignal() {
    m_quitSignal = true;
}
