#pragma once

#include "rendering/model/StaticMeshInstanceData.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "resources/asset/AssetHandleBundle.h"

class Camera3D;
class World;
class WeatherManager;
class MaterialShaderDef;
struct ShadowPassShaderData;

DECL_VG(class GLProgram);

class InstancedStaticModelRenderer {
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void onWorldBegin(World& world);

    void renderModelPass(const ModelInstanceMap& modelInstances, const Camera3D& camera, MaterialRenderPassType passType);
    void renderModelShadows(const ModelInstanceMap& modelInstances, const ShadowPassShaderData& shaderData, const Camera3D& camera);

private:

    const MaterialShaderDef* mStandardShader = nullptr;
    const MaterialShaderDef* mShadowMapperShader = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    AssetHandleBundle mShaderAssets;
    WeatherManager* mWeatherManager = nullptr;
};

