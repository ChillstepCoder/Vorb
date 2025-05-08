#pragma once

class Camera3D;
class ModelImpostorManager;

#include "rendering/MaterialShaderDef.h"

class ModelImpostorRenderer {
public:
    ModelImpostorRenderer();

    void renderBillboards(const ModelImpostorManager& billboardManager);

private:
    AssetHandlePtr<MaterialShaderDef> mShader;
};

