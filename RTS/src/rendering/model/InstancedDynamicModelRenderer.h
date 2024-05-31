#pragma once

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "rendering/model/DynamicModelBatchData.h"
#include "resources/asset/AssetHandleBundle.h"
#include "rendering/model/MaterialRenderPassType.h"

#include "rendering/renderstate/DynamicModelInstanceState.h"

class Camera3D;
class MaterialShaderDef;

// Stores all specific instances of a given model in the world
typedef std::unordered_map<ModelID, DynamicModelBatchData> DynamicModelBatchMap;

class InstancedDynamicModelRenderer
{
public:
    InstancedDynamicModelRenderer();
    ~InstancedDynamicModelRenderer();

    void prepareFrame(const std::vector<DynamicModelInstanceState>& dynamicModels, const Camera3D& camera);
    void renderModelPass(MaterialRenderPassType renderPass);
    // TODO: Shadows?

protected:
    const MaterialShaderDef* mStandardMaterial = nullptr;
    const MaterialShaderDef* mShadowMapperMaterial = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    AssetHandleBundle mShaderAssets;

    DynamicModelBatchMap mModelBatchesThisFrame;
    std::unique_ptr<GpuStreamingDataBuffer> mTransformsBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mVariantsBuffer;
    std::vector<std::pair<GLDrawCommandBuffer*, const Mesh*>> mDrawCommandsThisFrame[e_count(MaterialRenderPassType)];
    std::vector<f32m4> mInstanceTransforms;
    ui32 mNumTransforms = 0;
};

