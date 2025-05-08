#pragma once

#include "AssetEditorViewportPanel.h"
#include "rendering/mesh/PrimitiveShapeMeshes.h"

#include "rendering/material/MaterialDef.h"

class MaterialEditorViewportPanel : public AssetEditorViewportPanel<MaterialDef>
{
public:
    MaterialEditorViewportPanel();
    ~MaterialEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Material Editor"; }

private:

    const MaterialShaderDef* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    PrimitiveShapeType mShapeType = PrimitiveShapeType::Cube;
    float mUvScale = 2.0f;
};

