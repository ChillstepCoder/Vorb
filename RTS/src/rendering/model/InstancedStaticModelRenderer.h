#pragma once

#include "rendering/model/StaticMeshInstanceData.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "resources/asset/AssetHandleBundle.h"

class Camera3D;
class MaterialShaderDef;
struct ShadowPassShaderData;

DECL_VG(class GLProgram);

class InstancedStaticModelRenderer {
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void renderModelPass(const ModelInstanceMap& modelInstances, const Camera3D& camera);
    void renderModelShadows(const ModelInstanceMap* allModelPasses, const ShadowPassShaderData& shaderData, const Camera3D& camera);

private:

    const MaterialShaderDef* mStandardMaterial = nullptr;
    const MaterialShaderDef* mShadowMapperMaterial = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    AssetHandleBundle mShaderAssets;
};

