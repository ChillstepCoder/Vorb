#pragma once

#include "rendering/model/StaticModelBatchData.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "resources/asset/AssetHandleBundle.h"

class Camera3D;
class World;
class WeatherManager;
class MaterialShaderDef;
class CubemapDef;
struct ShadowPassShaderData;

DECL_VG(class GLProgram);

class InstancedStaticModelRenderer {
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void setActiveWorld(World& world);

    void renderModelPass(const ModelInstanceMap& modelInstances, const Camera3D& camera, MaterialRenderPassType passType, const CubemapDef* skyCubeMap);
    void renderModelShadows(const ModelInstanceMap& modelInstances, const ShadowPassShaderData& shaderData, const Camera3D& camera);

private:

    const MaterialShaderDef* mStandardShader = nullptr;
    const MaterialShaderDef* mShadowMapperShader = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    const MaterialShaderDef* mWaterShader = nullptr;
    AssetHandleBundle mShaderAssets;
    WeatherManager* mWeatherManager = nullptr;
};

