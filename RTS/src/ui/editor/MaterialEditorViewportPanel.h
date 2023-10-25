#pragma once

#include "AssetEditorViewportPanel.h"
#include "rendering/mesh/PrimitiveShapeMeshes.h"

#include "rendering/material/MaterialData.h"

class MaterialEditorViewportPanel : public AssetEditorViewportPanel<MaterialDef>
{
public:
    MaterialEditorViewportPanel();
    ~MaterialEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;

    void setMaterial(AssetID assetId);

    const char* getViewportWindowName() const override { return "Material Editor"; }

private:
    void updateAndRenderInternal(f32 elapsedSec) override;


    const MaterialShaderDef* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    PrimitiveShapeType mShapeType = PrimitiveShapeType::Cube;
    float mUvScale = 2.0f;

    AssetHandlePtr<MaterialShaderDef> mPbrMaterial;
    AssetHandlePtr<MaterialShaderDef> mEditorMaterial;
    AssetHandlePtr<MaterialShaderDef> mWireframeMaterial;
};

