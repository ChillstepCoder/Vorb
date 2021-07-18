#include "stdafx.h"

#include "App.h"
#include "MainMenuScreen.h"

#include "services/Services.h"
#include "Random.h"

#include <Vorb/Delegate.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/ui/ScreenList.h>
#include <Vorb/sound/SoundEngine.h>

#if IS_ENABLED(FEATURE_WORLD_EDITOR)
#include "screens/WorldEditorScreen.h"
#endif


// size of global cached random table
const unsigned CACHED_RANDOM_SIZE = 65536;

App::App() {
}

App::~App() {
}

void App::addScreens() {

#if IS_ENABLED(FEATURE_WORLD_EDITOR)
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
