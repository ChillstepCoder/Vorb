#pragma once

#include "rendering/model/StaticModelInstanceData.h"
#include "rendering/model/MaterialRenderPassType.h"

class Camera3D;
class MaterialShader;
struct ShadowPassShaderData;

DECL_VG(class GLProgram);

class InstancedStaticModelRenderer {
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void renderModelPass(const ModelInstanceMap& modelInstances, const Camera3D& camera);
    void renderModelShadows(const ModelInstanceMap* allModelPasses, const ShadowPassShaderData& shaderData, const Camera3D& camera);

private:

    const MaterialShader* mStandardMaterial = nullptr;
    const MaterialShader* mShadowMapperMaterial = nullptr;
    const MaterialShader* mSmudgeShader = nullptr;
};

