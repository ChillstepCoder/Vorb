#pragma once

#include "IEditorViewportPanel.h"
#include "rendering/mesh/PrimitiveShapeMeshes.h"

#include "rendering/material/MaterialData.h"

class MaterialEditorViewportPanel : public IEditorViewportPanel
{
public:
    MaterialEditorViewportPanel();
    ~MaterialEditorViewportPanel();

    bool updateAndRender(f32 elapsedSec) override;
    void updateAndRenderPrimaryControls(f32 ySize) override;

    void setMaterial(AssetID assetId);

private:
    const MaterialShaderDef* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    AssetHandlePtr<MaterialDef> mMaterialAsset;
    MaterialDef* mCurrentMaterial = nullptr;
    PrimitiveShapeType mShapeType = PrimitiveShapeType::Cube;
    float mUvScale = 2.0f;

    AssetHandlePtr<MaterialShaderDef> mPbrMaterial;
    AssetHandlePtr<MaterialShaderDef> mEditorMaterial;
    AssetHandlePtr<MaterialShaderDef> mWireframeMaterial;
};

