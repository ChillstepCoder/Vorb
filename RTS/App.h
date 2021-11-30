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
#if IS_ENABLED(FEATURE_TEST_3D)
    std::unique_ptr<Test3DScreen> mTest3DScreen;
#elif IS_ENABLED(FEATURE_WORLD_EDITOR)
    std::unique_ptr<WorldEditorScreen> mWorldEditorScreen;
#endif
protected:
    void onUpdateFrame() override;

};

