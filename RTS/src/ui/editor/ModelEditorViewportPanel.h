#pragma once

#include "IEditorViewportPanel.h"

class ModelDef;

class ModelEditorViewportPanel : public IEditorViewportPanel
{
public:
    ModelEditorViewportPanel();
    ~ModelEditorViewportPanel();

    bool updateAndRender(f32 elapsedSec) override;
    void updateAndRenderPrimaryControls(f32 ySize) override;

    void setModel(ModelDef& model) { mCurrentModel = &model; }

private:
    const MaterialShaderDef* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    ModelDef* mCurrentModel = nullptr;
   
    bool mDirtyModelData = false;
    int mLod = 0;

};

