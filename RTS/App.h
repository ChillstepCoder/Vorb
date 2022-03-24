#pragma once

#include <Vorb/ui/MainGame.h>

#include "FeatureConst.h"

class GameplayScreen;
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

    std::unique_ptr<GameplayScreen> mMainMenuScreen;
protected:
    void onUpdateFrame() override;

};

