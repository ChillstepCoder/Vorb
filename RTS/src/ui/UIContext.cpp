#include "stdafx.h"
#include "UIContext.h"

#include "ui/TileInspectionPanel.h"
#include "ui/PauseMenuPanel.h"

#include "options/DebugOptions.h"

#include "world/IWorld.h"
#include "ui/editor/EditorRoot.h"

#include "screens/ScreenState.h"

UIContext* UIContext::sInstance = nullptr;

UIContext::UIContext(const f32v2& screenResolution, SDL_Window* window) : mScreenResolution(screenResolution), mWindow(window) {
    mEditorRoot = std::make_unique<EditorRoot>();
}

UIContext::~UIContext() {

}

void UIContext::updateEditors(const Camera3D& camera, const f32v3& mousePickRay) {
    mEditorRoot->updateEditors(camera, mousePickRay);
}

void UIContext::updateAndRenderUI(const vg::GBuffer* activeGBuffer) {
    
    mEditorRoot->updateAndRenderUI(activeGBuffer);
    
    if (mTileInspectionPanel) {
        mTileInspectionPanel->updateAndRender();
    }

    if (mPauseMenuPanel) {
        PauseMenuPanelResult result = mPauseMenuPanel->updateAndRender();
        switch (result) {
            case PauseMenuPanelResult::RESUME:
                break;
            case PauseMenuPanelResult::EXIT_TO_MENU:
                GameplayScreenGlobalState::isQuittingToMenu = true;
                break;
            case PauseMenuPanelResult::EXIT_TO_DESKTOP:
                GameplayScreenGlobalState::isQuittingToDesktop = true;
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
    mEditorRoot->renderEditorBrushDecals(camera);
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