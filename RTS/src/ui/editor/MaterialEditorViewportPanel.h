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

    void setMaterial(EditorMaterialHandle& materialData) { mCurrentMaterial = materialData; }

private:
    const MaterialShaderDef* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    EditorMaterialHandle mCurrentMaterial;
    PrimitiveShapeType mShapeType = PrimitiveShapeType::Cube;
    float mUvScale = 2.0f;
};

