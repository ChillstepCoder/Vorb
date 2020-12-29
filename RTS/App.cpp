#include "stdafx.h"

#include "App.h"
#include "MainMenuScreen.h"
#include "screens/WorldEditorScreen.h"

#include "services/Services.h"
#include "Random.h"

#include <Vorb/Delegate.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/ui/ScreenList.h>
#include <Vorb/sound/SoundEngine.h>

#define USE_EDITOR
#ifdef USE_EDITOR
#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>
#endif


// size of global cached random table
const unsigned CACHED_RANDOM_SIZE = 65536;

App::App() {
}

App::~App() {
}

void App::addScreens() {

#ifdef USE_EDITOR
	mWorldEditorScreen = std::make_unique<WorldEditorScreen>(this);
    m_screenList.addScreen(mWorldEditorScreen.get());
    m_screenList.setScreen(mWorldEditorScreen->getIndex());
#else
    mMainMenuScreen = std::make_unique<MainMenuScreen>(this);
	m_screenList.addScreen(mMainMenuScreen.get());
	m_screenList.setScreen(mMainMenuScreen->getIndex());
#endif
}

void App::onInit() {
	Services::init();

    Random::initCachedRandom(CACHED_RANDOM_SIZE);

    // Init IMGUI
#ifdef USE_EDITOR
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 450";
    // Make sure we are using the right gl version (4.5)
    const ui32 minor = m_window.getGLMinorVersion();
    const ui32 major = m_window.getGLMajorVersion();
    assert(minor == 5 && major == 4);
    ImGui_ImplSDL2_InitForOpenGL(static_cast<SDL_Window*>(m_window.getHandle()), m_window.getContext());
    ImGui_ImplOpenGL3_Init(glsl_version);
#endif

}

void App::onExit() {
	Services::destroy();
}

void App::refreshElapsedTime() {
	vui::MainGame::refreshElapsedTime();
	sTotalTimeSeconds = m_curTime.totalSec;
	// Loss of precision here should be fine
	sElapsedSecondsSinceLastFrame = (f32)m_curTime.elapsedSec;
}
