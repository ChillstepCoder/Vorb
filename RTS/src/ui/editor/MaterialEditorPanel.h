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
    void renderModelToTexture();

    MaterialHandle mCurrentMaterial;
    PrimitiveShapeType mShapeType = PrimitiveShapeType::Sphere;
};

