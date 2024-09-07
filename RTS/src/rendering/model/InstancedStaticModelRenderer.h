#pragma once

#include "rendering/model/MaterialRenderPassType.h"
#include "resources/asset/AssetHandleBundle.h"

class Camera3D;
class World;
class WeatherManager;
class MaterialShaderDef;
class CubemapDef;
struct ShadowPassShaderData;
class InstancedStaticModelManager;

DECL_VG(class GLProgram);

class InstancedStaticModelRenderer {
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void setActiveWorld(World& world);

    void renderModelPass(const InstancedStaticModelManager& modelManager, const Camera3D& camera, MaterialRenderPassType passType, const CubemapDef* skyCubeMap);
    void renderModelShadows(const InstancedStaticModelManager& modelManager, const ShadowPassShaderData& shaderData, const Camera3D& camera);

private:

    const MaterialShaderDef* mStandardShader = nullptr;
    const MaterialShaderDef* mShadowMapperShader = nullptr;
    // TODO: USE
    //const MaterialShaderDef* mCutoutShadowMapperShader = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    const MaterialShaderDef* mWaterShader = nullptr;
    AssetHandleBundle mShaderAssets;
    WeatherManager* mWeatherManager = nullptr;
};

