#pragma once

#include "AssetEditorViewportPanel.h"

#include "definitions/ModelDef.h"

class LineMesh;
class SkeletalAnimator;

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
    void renderMeshStatic();
    void renderMeshSkeletal();
   
    bool mSkeletalEditMode = true;
    bool mDirtyModelData = false;
    int mLod = 0;
    bool mShowSingle = false;
    int mSingleIndex = 0;
    int mVariantIndex = 0;

    std::unique_ptr<LineMesh> mAABBMesh;

    // Skeletal
    std::unique_ptr<SkeletalAnimator> mSkeletalAnimator;
    SoftAssetReference mPreviewAnim = SoftAssetReference(AssetType::Animation);
    f32 mPreviewAnimTime = 0.0f;
};

