#pragma once

#include "IEditorViewportPanel.h"

class ModelDef;

class ModelEditorViewportPanel : public IEditorViewportPanel
{
public:
    ModelEditorViewportPanel();
    ~ModelEditorViewportPanel();

    bool updateAndRender() override;
    void updateAndRenderControls(f32 ySize) override;

    void setModel(ModelDef& model) { mCurrentModel = &model; }

private:
    const MaterialShader* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    ModelDef* mCurrentModel = nullptr;
   
    bool mDirtyModelData = false;
    int mLod = 0;

};

