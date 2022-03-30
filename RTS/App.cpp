#include "stdafx.h"

#include "App.h"
#include "GameplayScreen.h"

#include "services/Services.h"
#include "Random.h"

#include <Vorb/Delegate.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/ui/ScreenList.h>
#include <Vorb/sound/SoundEngine.h>

// TODO: Config
#include "options/DebugOptions.h"

// Use dedicated GPUs
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// size of global cached random table
const unsigned CACHED_RANDOM_SIZE = 65536;

App::App() {
}

App::~App() {
}

void App::addScreens() {
    mMainMenuScreen = std::make_unique<GameplayScreen>(this);
	m_screenList.addScreen(mMainMenuScreen.get());
	m_screenList.setScreen(mMainMenuScreen->getIndex());
}

void setPriorityToMax() {
#ifdef VORB_OS_WINDOWS
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
    struct sched_param params;

    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);
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
    setPriorityToMax();

    sDebugOptions.mScreenResolution = f32v2(m_window.getWidth(), m_window.getHeight());

	Services::init();

    Random::initCachedRandom(CACHED_RANDOM_SIZE);

    // Init events
    vui::InputDispatcher::key.onFocusGained.addFunctor([](Sender) {
        setPriorityToMax();
    });
    vui::InputDispatcher::key.onFocusLost.addFunctor([](Sender) {
        setPriorityToNormal();
    });

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

void App::onUpdateFrame() {
    MainGame::onUpdateFrame();
    // Update window settings
    getWindow().setSwapInterval(sDebugOptions.mVSYNC ? vorb::ui::GameSwapInterval::V_SYNC : vorb::ui::GameSwapInterval::UNLIMITED_FPS);
}
