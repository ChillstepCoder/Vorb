#include "stdafx.h"

#include "App.h"
#include "MainMenuScreen.h"

#include "services/Services.h"

#include <Vorb/Delegate.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/ui/ScreenList.h>
#include <Vorb/sound/SoundEngine.h>

App::App() {
}

App::~App() {
}

void App::addScreens() {
	m_mainMenuScreen = std::make_unique<MainMenuScreen>(this);

	m_screenList.addScreen(m_mainMenuScreen.get());

	m_screenList.setScreen(m_mainMenuScreen->getIndex());
}

void App::onInit() {
	Services::init();
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
