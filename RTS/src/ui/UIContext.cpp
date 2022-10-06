#include "stdafx.h"
#include "UIContext.h"

#include "ui/DebugTweakerPanel.h"
#include "ui/TileInspectionPanel.h"
#include "ui/PauseMenuPanel.h"

#include "options/DebugOptions.h"

#include "world/IWorld.h"
#include "editor/WorldEditor.h"

#include "screens/ScreenState.h"

UIContext* UIContext::sInstance = nullptr;

UIContext::UIContext(const f32v2& screenResolution, SDL_Window* window) : mScreenResolution(screenResolution), mWindow(window) {
    mDebugTweakerPanel = std::make_unique<DebugTweakerPanel>(screenResolution);
    mEditor = std::make_unique<WorldEditor>(screenResolution);
}

UIContext::~UIContext() {

}

void UIContext::updateEditors(const Camera3D& camera) {
    if (sDebugOptions.mShowEditor) {
        mEditor->update(camera);
    }
}

void UIContext::updateAndRenderUI(EntityComponentSystem& ecs, const vg::GBuffer* activeGBuffer, float aspectRatio) {
    if (sDebugOptions.mShowTweaker) {
        mDebugTweakerPanel->updateAndRender(ecs, activeGBuffer, aspectRatio);
    }
    if (sDebugOptions.mShowEditor) {
        mEditor->renderUI();
    }
    if (mTileInspectionPanel) {
        mTileInspectionPanel->updateAndRender();
    }
    if (mPauseMenuPanel) {
        PauseMenuPanelResult result = mPauseMenuPanel->updateAndRender();
        switch (result) {
            case PauseMenuPanelResult::RESUME:
                break;
            case PauseMenuPanelResult::EXIT_TO_MENU:
                GameplayScreenState::isQuittingToMenu = true;
                break;
            case PauseMenuPanelResult::EXIT_TO_DESKTOP:
                GameplayScreenState::isQuittingToDesktop = true;
                break;
            default:
                break;

        }
        if (result != PauseMenuPanelResult::NONE) {
            mPauseMenuPanel.reset();
        }
    }
}

void UIContext::renderEditorBrushDecals(const Camera3D& camera) {
    if (sDebugOptions.mShowEditor) {
        mEditor->renderBrushDecals(camera);
    }
}

void UIContext::activateTileInspectionPanel(const f32v2& screenPos, const TileHandle& tileHandle) {
    if (tileHandle.isValid()) {
        mTileInspectionPanel = std::make_unique<TileInspectionPanel>(screenPos, tileHandle);
    }
}

void UIContext::closeTileInspectionPanel() {
    mTileInspectionPanel.reset();
}

void UIContext::toggleMainMenu() {
    if (mPauseMenuPanel) {
        mPauseMenuPanel.reset();
    }
    else {
        mPauseMenuPanel = std::make_unique<PauseMenuPanel>();
    }
}

UIContext& UIContext::initInstance(const f32v2& screenResolution, SDL_Window* window) {
    if (!sInstance) {
        sInstance = new UIContext(screenResolution, window);
    }
    return *sInstance;
}

UIContext& UIContext::getInstance() {
    assert(sInstance);
    return *sInstance;
}