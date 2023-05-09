#pragma once

#include <Vorb/ui/MainGame.h>

#include "FeatureConst.h"

class MainMenuScreen;
class GameplayScreen;

class App : public vui::MainGame
{
public:
    App();
    ~App();

    virtual void addScreens() override;
    virtual void onInit() override;
    virtual void onExit() override;
    virtual void refreshElapsedTime() override;

    std::unique_ptr<MainMenuScreen> mMainMenuScreen;
    std::unique_ptr<GameplayScreen> mGameplayScreen;
protected:
    void onUpdateFrame() override;

};

extern App* sApp;
