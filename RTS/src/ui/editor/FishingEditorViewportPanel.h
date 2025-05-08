#pragma once
#include "AssetEditorViewportPanel.h"

// For FishData TODO: FishRepository
#include "ui/minigame/FishingMinigame.h"

class FishingMinigame;

class FishingEditorViewportPanel : public AssetEditorViewportPanel<FishDef> {
public:
    FishingEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;

    void renderMesh() override;

    const char* getViewportWindowName() const override { return "Fish Editor"; }

private:
    void updateAndRenderInternal(f32 elapsedSec) override;
    const MaterialShaderDef* getShader() override;
    void renderFishModel();
    BitFlags<FishingMinigameFlags> getMinigameFlags();

    std::unique_ptr<FishingMinigame> mCurrentFishingMinigame;
    AssetHandlePtr<MaterialShaderDef> mShader;

    bool mDisableDebris = true;
    bool mDisableChests = true;
};

