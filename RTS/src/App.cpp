#include "stdafx.h"

#include "App.h"
#include "screens/MainMenuScreen.h"
#include "screens/GameplayScreen.h"

#include "math/Random.h"

#include "rendering/GLExtensions.h"

#include <Vorb/Delegate.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/ui/ScreenList.h>
#include <Vorb/sound/SoundEngine.h>
#include <Vorb/graphics/ShaderManager.h>

// TODO: Config
#include "options/DebugOptions.h"

// For log level
#include <yojimbo/yojimbo.h>

#include "rendering/gl/GL.h"
#define VERBOSE_GL_LOG 0

#define SKIP_MAIN_MENU 0

// Use dedicated GPUs
// DOESN'T ALWAYS WORK https://forums.developer.nvidia.com/t/nvoptimusenablement-is-not-working-in-our-opengl-application/41299/3
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// size of global cached random table
constexpr unsigned CACHED_RANDOM_SIZE = 131072; // TODO: Uncomment *2 to break building generation

App* sApp = nullptr;

App::App() {
}

App::~App() {
}

void App::addScreens() {
    mMainMenuScreen = std::make_unique<MainMenuScreen>(this);
    mGameplayScreen = std::make_unique<GameplayScreen>(this);
    m_screenList.addScreen(mMainMenuScreen.get());
	m_screenList.addScreen(mGameplayScreen.get());
#if SKIP_MAIN_MENU == 1
	m_screenList.setScreen(mGameplayScreen->getIndex());
#else
    m_screenList.setScreen(mMainMenuScreen->getIndex());
#endif
}

void setPriorityToNormal() {
#ifdef VORB_OS_WINDOWS
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#else
    struct sched_param params;

    params.sched_priority = (sched_get_priority_max(SCHED_FIFO) + sched_get_priority_min(SCHED_FIFO)) / 2; // No idea if this is even right
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);
#endif
}

void App::onInit() {


    PROFILE_BEGIN_SESSION("Main", "profiling.json");
    // Set as render thread
    RENDER_THREAD_ID = std::this_thread::get_id();
    setThreadName("Render");

    // Set log level for yojimbo
    yojimbo_log_level(YOJIMBO_LOG_LEVEL_ERROR);

    setThreadPriorityToMax();

    sDebugOptions.mScreenResolution = f32v2(m_window.getWidth(), m_window.getHeight());

    // Init resources
    Services::initResources();
    sGlExtensions.init();

    // Initialize GL api
    GetAPI4(&GL, [](const char* func) -> void* { return (void*)wglGetProcAddress(func); });
#if VERBOSE_GL_LOG == 1
    InjectAPITracer4(&GL);
#endif

    Random::initCachedRandom(CACHED_RANDOM_SIZE);

    vg::ShaderManager::setShaderRootDirectory(vio::IOManager::getCurrentWorkingDirectory() / vio::Path("data\\shaders"));

    // Init events
    vui::InputDispatcher::key.addFocusGainedListener([]() {
        setThreadPriorityToMax();
    });
    // TODO: Threadpool/render thread as well?
    vui::InputDispatcher::key.addFocusLostListener([]() {
        setPriorityToNormal();
    });
}

void App::onExit() {
	Services::destroy();
    Services::destroyResources();
    PROFILE_END_SESSION();
}

void App::refreshElapsedTime() {
	vui::MainGame::refreshElapsedTime();
	sTotalTimeSeconds = m_curTime.totalSec;
	// Loss of precision here should be fine
	sElapsedSecondsSinceLastFrame = (f32)m_curTime.elapsedSec;
}

void App::onUpdateFrame() {
    PROFILE_FUNCTION();
    MainGame::onUpdateFrame();
    // Update window settings
    getWindow().setSwapInterval(sDebugOptions.mVSYNC ? vorb::ui::GameSwapInterval::V_SYNC : vorb::ui::GameSwapInterval::UNLIMITED_FPS);
}
