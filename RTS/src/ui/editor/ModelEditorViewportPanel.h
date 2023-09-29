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

    void setModel(ModelID modelId);

private:
    const MaterialShaderDef* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    AssetHandlePtr<ModelDef> mModelHandle;
    ModelDef* mCurrentModel = nullptr;

    AssetHandlePtr<MaterialShaderDef> mPbrMaterial;
    AssetHandlePtr<MaterialShaderDef> mEditorMaterial;
    AssetHandlePtr<MaterialShaderDef> mWireframeMaterial;
   
    bool mDirtyModelData = false;
    int mLod = 0;

};

