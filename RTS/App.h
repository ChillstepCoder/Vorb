#pragma once

#include "stdafx.h"
#include <Vorb/ui/MainGame.h>

class MainMenuScreen;

class App : public vui::MainGame
{
public:
    App();
    ~App();

    virtual void addScreens() override;
    virtual void onInit() override;
    virtual void onExit() override;

	std::unique_ptr<MainMenuScreen> m_mainMenuScreen;
};

