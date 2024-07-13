
#include "stdafx.h"
#include "MainGame.h"

#include <Vorb/logging/Logger.h>

#include <thread>

// Opengl debugging
#ifdef DEBUG
#define IS_DEBUG_OPENGL_CONTEXT 1
#else
// TODO: Disable for live
#define IS_DEBUG_OPENGL_CONTEXT 1
#endif


//#if defined(VORB_OS_WINDOWS)
//#include <SDL/SDL.h>
//#else
#include <SDL2/SDL.h>
//#endif
#define MS_TIME (SDL_GetTicks())

#include "Vorb/graphics/GLStates.h"
#include "ui/IGameScreen.h"
#include "input/InputDispatcher.h"
#include "Vorb/graphics/GraphicsDevice.h"
#include "ui/ScreenList.h"
#include "Vorb/Timing.h"
#include "input/InputDispatcherEventCatcher.h"

vui::MainGame::MainGame() :
    m_screenList(this) {
    // Empty
}
vui::MainGame::~MainGame() {
    // Empty
}

bool vui::MainGame::init() {
    m_lastTime = {};
    m_curTime = {};

    // This Is Vital
    if (!initSystems()) return false;
    m_window.setTitle(nullptr);

    // Initialize Logic And Screens
    onInit();
    addScreens();

    // Try To Get A Screen
    m_screen = m_screenList.getCurrent();
    if (m_screen) {
        // Run The First Game Screen
        m_screen->setRunning();
        m_screen->onEntry(m_lastTime);
    }

    // Set last known time
    m_lastMS = MS_TIME;
    return true;
}
bool vui::MainGame::initSystems() {

    // Create The Window
    if (!m_window.init(true, IS_DEBUG_OPENGL_CONTEXT)) return false;
    sMainGameWindowHandle = &m_window;

    // TODO: Replace With BlendState
    glEnable(GL_BLEND);
    vg::sBlendStates.ALPHA.set();

    // Set A Default OpenGL State
    vg::DepthState::FULL.set();
    vg::RasterizerState::CULL_CLOCKWISE.set();

    return true;
}
void vui::MainGame::exitGame() {
    if (m_screen) {
        m_screen->onExit(m_lastTime);
        m_screen = nullptr;
    }
    m_screenList.destroy(m_lastTime);
    onExit();
    m_window.dispose();
    m_isRunning = false;
}

bool vui::MainGame::shouldTerminate() const {
    return !m_isRunning || m_window.shouldQuit() || m_screen == nullptr;
}
bool vui::MainGame::checkScreenChange() {
    // If no screen, then the frame should not do anything
    if (!m_screen) return true;

    switch (m_screen->getState()) {
    case ScreenState::CHANGE_NEXT:
        m_screen->onExit(m_curTime);
        m_screen = m_screenList.moveNext();
        if (m_screen != nullptr) {
            m_screen->setRunning();
            m_screen->onEntry(m_curTime);
        }
        return true;
    case ScreenState::CHANGE_PREVIOUS:
        m_screen->onExit(m_curTime);
        m_screen = m_screenList.movePrevious();
        if (m_screen != nullptr) {
            m_screen->setRunning();
            m_screen->onEntry(m_curTime);
        }
        return true;
    case ScreenState::EXIT_APPLICATION:
        m_isRunning = false;
        return true;
    default:
        // No change occurs otherwise
        return false;
    }
}

void vui::MainGame::run() {
    // Initialize everything except SDL audio and SDL haptic feedback.
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK);

    // Make sure we are using hardware acceleration
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    // For counting the fps
    FpsCounter fpsCounter;

    // Game Loop
    if (init()) {
        m_isRunning = true;
        while (!shouldTerminate()) {
            // Start the FPS counter
            fpsCounter.beginFrame();

            // Refresh time information for this frame
            refreshElapsedTime();

            // Scree logic
            if (!checkScreenChange()) {
                // Update
                onUpdateFrame();
                if (!checkScreenChange()) {
                    // Render
                    onRenderFrame();
                }
            }

            // Swap buffers and synchronize time-step and window input
            ui32 curMS = MS_TIME;
            m_window.sync(curMS - m_lastMS);

            // Get the FPS
            m_fps = fpsCounter.endFrame();
        }

        // Exit application logic
        exitGame();
    }

    SDL_Quit();
}

void vui::MainGame::refreshElapsedTime() {
    ui32 curTimeMs = MS_TIME;
    f64 elapsedMs = curTimeMs - m_lastMS;
    f64 elapsedSec = (elapsedMs) / 1000.0;
    m_lastMS = curTimeMs;

    m_lastTime = m_curTime;
    m_curTime.elapsedSec = elapsedSec;
    m_curTime.totalSec += elapsedSec;
    m_curTime.deltaTime = (float)(elapsedMs / 16.666); // Shoot for 16ms per frame (~60hz)
}
void vui::MainGame::onUpdateFrame() {
    // Perform the screen's update logic
    m_screen->update(m_curTime);
}
void vui::MainGame::onRenderFrame() {
    // TODO: Investigate Removing This
    glViewport(0, 0, m_window.getWidth(), m_window.getHeight());

    // Draw the screen
    m_screen->draw(m_curTime);
}

