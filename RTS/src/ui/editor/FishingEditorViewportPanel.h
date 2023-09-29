#pragma once
#include "IEditorViewportPanel.h"

// For FishData TODO: FishRepository
#include "ui/minigame/FishingMinigame.h"

class FishingMinigame;

class FishingEditorViewportPanel :  public IEditorViewportPanel {
public:
    FishingEditorViewportPanel();

    bool updateAndRender(f32 elapsedSec) override;
    void updateAndRenderPrimaryControls(f32 ySize) override;

    void renderMesh() override;

    void setFishDef(AssetID fishId);

private:
    const MaterialShaderDef* getShader() override;
    void renderFishModel();
    BitFlags<FishingMinigameFlags> getMinigameFlags();

    AssetHandlePtr<FishDef> mFishAsset;
    FishDef* mFishDef = nullptr;
    std::unique_ptr<FishingMinigame> mCurrentFishingMinigame;
    AssetHandlePtr<MaterialShaderDef> mShader;
    f32v2 mViewportDims;

    bool mDisableDebris = true;
    bool mDisableChests = true;
};

