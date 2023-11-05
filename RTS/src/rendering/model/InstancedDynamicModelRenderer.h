#pragma once

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "rendering/model/DynamicModelInstanceData.h"
#include "resources/asset/AssetHandleBundle.h"

#include "rendering/renderstate/DynamicModelRenderState.h"

class Camera3D;
class MaterialShaderDef;

// Stores all specific instances of a given model in the world
typedef std::unordered_map<ModelID, DynamicModelInstanceData> DynamicModelInstanceMap;

class InstancedDynamicModelRenderer
{
public:
    InstancedDynamicModelRenderer();
    ~InstancedDynamicModelRenderer();

    void renderModelPass(const std::vector<DynamicModelRenderState>& dynamicModels, const Camera3D& camera);
    // TODO: Shadows?

protected:
    const MaterialShaderDef* mStandardMaterial = nullptr;
    const MaterialShaderDef* mShadowMapperMaterial = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    AssetHandleBundle mShaderAssets;

    DynamicModelInstanceMap mModelInstancesThisFrame;
    std::unique_ptr<GpuStreamingDataBuffer> mTransformsBuffer;
    std::vector<f32m4> mInstanceTransforms;
    ui32 mNumTransforms = 0;
};

