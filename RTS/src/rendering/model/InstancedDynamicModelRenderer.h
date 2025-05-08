#pragma once

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "resources/asset/AssetHandleBundle.h"
#include "rendering/model/MaterialRenderPassType.h"

#include "rendering/renderstate/DynamicModelInstanceState.h"

#include "definitions/ModelDef.h"

class Camera3D;
class MaterialShaderDef;

class InstancedDynamicModelRenderer
{
public:
    InstancedDynamicModelRenderer();
    ~InstancedDynamicModelRenderer();

    void prepareFrame(const DynamicModelInstanceStateContainer& dynamicModels, const Camera3D& camera);
    void renderModelPass(MaterialRenderPassType renderPass);
    // TODO: Shadows?

protected:
    const MaterialShaderDef* mStandardShader = nullptr;
    const MaterialShaderDef* mShadowMapperShader = nullptr;
    const MaterialShaderDef* mSmudgeShader = nullptr;
    AssetHandleBundle mShaderAssets;

    std::unique_ptr<GpuStreamingDataBuffer> mTransformsBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mVariantIndexBuffer;
    std::array<std::unique_ptr<GLDrawCommandBuffer>, e_count(MaterialRenderPassType)> mDrawCommands;

    UnorderedFlatMap<ModelID, ModelDefRef> mModelDefRefs;
};

