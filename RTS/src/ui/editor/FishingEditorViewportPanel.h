#pragma once
#include "IEditorViewportPanel.h"
class FishingEditorViewportPanel :  public IEditorViewportPanel {
public:
    bool updateAndRender() override;
    void updateAndRenderControls(f32 ySize) override;
};

