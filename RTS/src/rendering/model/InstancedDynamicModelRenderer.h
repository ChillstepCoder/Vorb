#pragma once

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "rendering/model/DynamicMeshInstanceData.h"
#include "resources/asset/AssetHandleBundle.h"

class Camera3D;
class MaterialShaderDef;

class InstancedDynamicModelRenderer
{
public:
    InstancedDynamicModelRenderer();
    ~InstancedDynamicModelRenderer();

    void renderModelPass(const DynamicModelInstanceMap& modelInstances, const Camera3D& camera);
    // TODO: Shadows?

protected:
    const MaterialShaderDef* mStandardMaterial = nullptr;
    const MaterialShaderDef* mShadowMapperMaterial = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    AssetHandleBundle mShaderAssets;

    std::unique_ptr<GpuStreamingDataBuffer> mTransformsBuffer;
    std::vector<f32m4> mInstanceTransforms;
    ui32 mNumTransforms = 0;
};

