#pragma once

#include "IEditorViewportPanel.h"
#include "rendering/mesh/PrimitiveShapeMeshes.h"

#include "rendering/material/MaterialData.h"

class MaterialEditorPanel : public IEditorViewportPanel
{
public:
    MaterialEditorPanel();
    ~MaterialEditorPanel();

    bool updateAndRender() override;
    void updateAndRenderControls(f32 ySize) override;

    void setMaterial(MaterialHandle& materialData) { mCurrentMaterial = materialData; }

private:
    const MaterialShader* getShader() override;
    void uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) override;
    void renderMesh() override;

    MaterialHandle mCurrentMaterial;
    PrimitiveShapeType mShapeType = PrimitiveShapeType::UVSphere;
    float mUvScale = 2.0f;
};

