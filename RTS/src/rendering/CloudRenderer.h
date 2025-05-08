#pragma once

class CloudMeshManager;
class MaterialShaderDef;
class Camera3D;
class CubemapDef;
struct ShadowPassShaderData;

#include "resources/asset/AssetHandleBundle.h"

DECL_VG(class GBuffer);

// TODO: Read this : https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.17.2030&rep=rep1&type=pdf
class CloudRenderer
{
public:
    CloudRenderer(const ui32v2& gbufferDims);

    void renderClouds(const CloudMeshManager& cloudManager, VGTexture sharedDepthStencilTexture, vg::GBuffer* outputGBuffer, const Camera3D& camera, const CubemapDef& skyCubeMap);
    void renderCloudShadows(const ShadowPassShaderData& shaderData, const CloudMeshManager& cloudManager, const Camera3D& camera, f32 maxDistance);

private:
    void blurNormals();
    void renderToOutput(const CubemapDef& skyCubeMap);

    std::unique_ptr<vg::GBuffer> mGBuffers[2];

    const MaterialShaderDef* mCloudMaterial = nullptr;
    const MaterialShaderDef* mPostMaterial = nullptr;
    const MaterialShaderDef* mPostPbrMaterial = nullptr;
    const MaterialShaderDef* mBlurMaterial = nullptr;
    const MaterialShaderDef* mCloudShadowMaterial = nullptr;

    AssetHandleBundle mShaderAssetHandles;
};

