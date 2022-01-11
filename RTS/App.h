#pragma once

#include <Vorb/ui/MainGame.h>

#include "FeatureConst.h"

class MainMenuScreen;
class WorldEditorScreen;
class Test3DScreen;

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
protected:
    void onUpdateFrame() override;

};

