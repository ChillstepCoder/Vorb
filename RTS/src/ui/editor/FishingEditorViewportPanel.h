#pragma once
#include "IEditorViewportPanel.h"

// For FishData TODO: FishRepository
#include "ui/minigame/FishingMinigame.h"

class FishingMinigame;

class FishingEditorViewportPanel :  public IEditorViewportPanel {
public:
    bool updateAndRender() override;
    void updateAndRenderControls(f32 ySize) override;

    void renderMesh() override;

    FishDef mTestFishData;
    std::unique_ptr<FishingMinigame> mCurrentFishingMinigame;
    f32v2 mViewportDims;

};

