#include "stdafx.h"
#include "UIContext.h"

#include "ui/DebugTweakerPanel.h"
#include "ui/TileInspectionPanel.h"

#include "options/DebugOptions.h"

#include "World.h"
#include "editor/WorldEditor.h"

UIContext* UIContext::sInstance = nullptr;

UIContext::UIContext(World& world, const f32v2& screenResolution, SDL_Window* window) : mScreenResolution(screenResolution), mWindow(window) {
    mDebugTweakerPanel = std::make_unique<DebugTweakerPanel>(screenResolution);
    mEditor = std::make_unique<WorldEditor>(world, screenResolution);
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

UIContext& UIContext::initInstance(World& world, const f32v2& screenResolution, SDL_Window* window) {
    if (!sInstance) {
        sInstance = new UIContext(world, screenResolution, window);
    }
    return *sInstance;
}

UIContext& UIContext::getInstance() {
    assert(sInstance);
    return *sInstance;
}