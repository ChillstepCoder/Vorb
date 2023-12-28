#include "stdafx.h"
#include "UIContext.h"

#include "ui/TileInspectionPanel.h"
#include "ui/PauseMenuPanel.h"

#include "options/DebugOptions.h"

#include "world/World.h"
#include "world/WorldDestroyer.h"
#include "ui/editor/EditorRoot.h"
#include "ui/editor/IEditorViewportPanel.h"
#include "ui/minigame/LocalMinigameContext.h"

#include <Vorb/ui/GameWindow.h>

#include "screens/ScreenState.h"

#include <imgui.h>
#include <imgui_internal.h>

UIContext* UIContext::sInstance = nullptr;

UIContext::UIContext(const f32v2& screenResolution, SDL_Window* window) : mScreenResolution(screenResolution), mWindow(window) {

    // Enable docking
    // https://github.com/ocornut/imgui/issues/2109
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.ConfigWindowsResizeFromEdges = true;

    mEditorRoot = std::make_unique<EditorRoot>();
    mMinigameContext = std::make_unique<LocalMinigameContext>();
}

UIContext::~UIContext() {

}

void UIContext::updateEditors(World* world, const Camera3D& camera, const f32v3& mousePickRay) {
    mEditorRoot->updateEditors(world, camera, mousePickRay);
}

void UIContext::updateAndRenderUI(const vg::GBuffer* activeGBuffer, f32 elapsedSec) {
    
    mEditorRoot->updateAndRenderUI(activeGBuffer, elapsedSec);
    
    if (mTileInspectionPanel) {
        mTileInspectionPanel->updateAndRender();
    }

    if (mPauseMenuPanel) {
        bool destroyWorld = false;
        PauseMenuPanelResult result = mPauseMenuPanel->updateAndRender();
        switch (result) {
            case PauseMenuPanelResult::RESUME:
                break;
            case PauseMenuPanelResult::EXIT_TO_MENU:
                GameplayScreenGlobalState::isQuittingToMenu = true;
                destroyWorld = true;
                break;
            case PauseMenuPanelResult::EXIT_TO_DESKTOP:
                GameplayScreenGlobalState::isQuittingToDesktop = true;
                destroyWorld = true;
                break;
            default:
                break;

        }
        if (result != PauseMenuPanelResult::NONE) {
            mPauseMenuPanel.reset();
        }
        if (destroyWorld) {
            World::shutdownAllWorlds();
        }
    }

    mMinigameContext->updateAndRender(mScreenResolution, elapsedSec);
}

void UIContext::renderEditorBrushDecals(const Camera3D& camera) {
    mEditorRoot->renderEditorBrushDecals(camera);
}

void UIContext::activateTileInspectionPanel(const f32v2& screenPos, const TileHandle& tileHandle) {
    if (tileHandle.isValid()) {
        mTileInspectionPanel = std::make_unique<TileInspectionPanel>(tileHandle.getWorld(), screenPos, tileHandle);
    }
}

void UIContext::closeTileInspectionPanel() {
    mTileInspectionPanel.reset();
}

bool UIContext::isEditorCameraActive() {
    return sDebugOptions.mShowEditor && mEditorRoot->hasActiveCenterPanel();
}

f32v3 UIContext::getEditorCameraPosition() {
    IEditorViewportPanel* centerPanel = mEditorRoot->getActiveCenterPanel();
    if (centerPanel) {
        return centerPanel->getCameraPosition();
    }
    return f32v3(0);
}

f32v3 UIContext::getEditorCameraDirection() {
    IEditorViewportPanel* centerPanel = mEditorRoot->getActiveCenterPanel();
    if (centerPanel) {
        return centerPanel->getCameraDirection();
    }
    return f32v3(1.0f, 0.0f, 0.0f);
}

f32v3 UIContext::getEditorCameraRight()
{
    IEditorViewportPanel* centerPanel = mEditorRoot->getActiveCenterPanel();
    if (centerPanel) {
        return centerPanel->getCameraRight();
    }
    return f32v3(0.0f, 1.0f, 0.0f);

}

f32v3 UIContext::getEditorCameraUp()
{
    IEditorViewportPanel* centerPanel = mEditorRoot->getActiveCenterPanel();
    if (centerPanel) {
        return centerPanel->getCameraUp();
    }
    return f32v3(0.0f, 0.0f, 1.0f);

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

ui32v2 UIContext::getWindowDims() {
    if (sMainGameWindowHandle) {
        return sMainGameWindowHandle->getViewportDims();
    }
    return ui32v2(0);
}

bool UIContext::shouldPauseGameRendering() const {
    return sDebugOptions.mShowEditor && mEditorRoot && mEditorRoot->hasActiveCenterPanel();
}
