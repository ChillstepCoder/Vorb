#pragma once

class Camera3D;
class MaterialShaderDef;
struct SelectedObjectData;

class ModelHighlightRenderer {
public:
    ModelHighlightRenderer();

    void renderModelHighlight(const SelectedObjectData& selectedObject, const Camera3D& camera);

private:
    const MaterialShaderDef* mHighlightShader = nullptr;
    AssetHandleBasePtr mHighlightShaderHandle;
};

