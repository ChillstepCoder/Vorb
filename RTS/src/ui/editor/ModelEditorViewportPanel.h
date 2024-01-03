#pragma once

#include "AssetEditorViewportPanel.h"

#include "definitions/ModelDef.h"

class LineMesh;

class ModelEditorViewportPanel : public AssetEditorViewportPanel<ModelDef>
{
public:
    ModelEditorViewportPanel();
    ~ModelEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Model Editor"; }

private:
    const MaterialShaderDef* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;
   
    bool mDirtyModelData = false;
    int mLod = 0;
    bool mShowSingle = false;
    int mSingleIndex = 0;
    int mVariantIndex = 0;

    std::unique_ptr<LineMesh> mAABBMesh;
};

