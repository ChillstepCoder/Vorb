#pragma once

class Camera3D;
class ModelBillboardLodManager;

#include "rendering/MaterialShaderDef.h"

class ModelBillboardRenderer {
public:
    ModelBillboardRenderer();

    void renderBillboards(const ModelBillboardLodManager& billboardManager);

private:
    AssetHandlePtr<MaterialShaderDef> mShader;
};

