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
#include "ui/debugging/GameplayDebugger.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "resources/TextureRepository.h"


#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/BlendState.h>

#include "ui/GameWindow.h"

#include "ui/noesis/NoesisGuiContext.h"

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

    // TODO: Not in shipping?
    mEditorRoot = std::make_unique<EditorRoot>();
    mMinigameContext = std::make_unique<LocalMinigameContext>();

#ifdef DEBUG
    toggleGameplayDebugger(); // Gameplay debugger by default in debug builds
#endif

    mReticleShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("reticle"));
    mReticleTexture = TextureRepository::get().getAssetHandle(CStrToken("reticle"));

    mNoesisGuiContext = std::make_unique<NoesisGuiContext>(*this);
}

UIContext::~UIContext() {

}

void UIContext::updateEditors(World* world, const Camera3D& camera, const f32v3& mousePickRay) {
    mEditorRoot->updateEditors(world, camera, mousePickRay);
}

void UIContext::updateAndRenderUI(const vg::GBuffer* activeGBuffer, f32 elapsedSec, const Camera3D& camera) {
    
    bool showReticle = sDebugOptions.mShowReticle;

    // Force show editor
    mEditorRoot->updateAndRenderUI(activeGBuffer, elapsedSec, camera);
    
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
            WorldDestroyer::shutdownAllWorlds();
        }

        showReticle = false;
    }

    if (showReticle) {
        renderReticle();
    }

    mMinigameContext->updateAndRender(mScreenResolution, elapsedSec);
    
    if (mGameplayDebugger) {
        mGameplayDebugger->updateAndRenderImGui(camera);
    }

    mNoesisGuiContext->updateAndRender();
}

void UIContext::renderEditorBrushDecals(const Camera3D& camera) {
    mEditorRoot->renderEditorBrushDecals(camera);
}

void UIContext::updateAndRenderNoesisOnly() {
    mNoesisGuiContext->updateAndRender();
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

void UIContext::toggleEscapeMenu() {
    if (mPauseMenuPanel) {
        mPauseMenuPanel.reset();
    }
    else {
        mPauseMenuPanel = std::make_unique<PauseMenuPanel>();
    }
}

void UIContext::toggleGameplayDebugger() {
    if (mGameplayDebugger) {
        mGameplayDebugger.reset();
    }
    else {
        mGameplayDebugger = std::make_unique<GameplayDebugger>();
    }
}

void UIContext::toggleGameUIPanel(GameUIPanel panel) {
    if (mNoesisGuiContext) {
        mNoesisGuiContext->toggleView(panel);
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

bool UIContext::hasInstance() {
    return sInstance != nullptr;
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

void UIContext::renderReticle() {
    if (!mReticleShader->isLoaded() || !mReticleTexture->isLoaded()) {
        return;
    }

    vg::sBlendStates.ALPHA.set();

    ui32 nextTextureIndex = 0;
    const MaterialShaderDef& def = mReticleShader->getLoadedAsset();
    MaterialRenderer::bindMaterialShaderForRender(def, &nextTextureIndex);

    glBindTextureUnit(nextTextureIndex, mReticleTexture->getLoadedAsset().getTextureHandle());
    glUniform1i(def.getUniform("unTexture"), nextTextureIndex);
    glUniform1f(def.getUniform("unSize"), sDebugOptions.mReticleSize);
    glUniform2f(def.getUniform("unScreenDims"), mScreenResolution.x, mScreenResolution.y);
    glUniform4f(def.getUniform("unColor"), sDebugOptions.mReticleColor.r / 255.f, sDebugOptions.mReticleColor.g / 255.f, sDebugOptions.mReticleColor.b / 255.f, sDebugOptions.mReticleColor.a / 255.f);

    sGlobalFullTriangleVAO.drawTwoTriangles();

    vg::BlendState::restorePrevious();
}
