#pragma once

#include "IEditorViewportPanel.h"

struct ModelDef;

class ModelEditorPanel : public IEditorViewportPanel
{
public:
    ModelEditorPanel();
    ~ModelEditorPanel();

    bool updateAndRender() override;
    void updateAndRenderControls(f32 ySize) override;

    void setModel(ModelDef& model) { mCurrentModel = &model; }

private:
    void renderModelToTexture();
    void renderModelToTextureBlendTest();

    ModelDef* mCurrentModel = nullptr;
   
    bool mDirtyModelData = false;
    int mLod = 0;

};

