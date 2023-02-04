#pragma once

class CloudManager;
class ResourceManager;
class MaterialShader;
class Camera3D;
class Cubemap;

DECL_VG(class GBuffer);

// TODO: Read this : https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.17.2030&rep=rep1&type=pdf
class CloudRenderer
{
public:
    CloudRenderer(const ui32v2& gbufferDims);

    void renderClouds(const CloudManager& cloudManager, VGTexture sharedDepthStencilTexture, vg::GBuffer* outputGBuffer, const Camera3D& camera, const Cubemap& skyCubeMap);
    void renderCloudShadows(const CloudManager& cloudManager, const Camera3D& camera, f32 maxDistance);

private:
    void blurNormals();
    void renderToOutput(const Cubemap& skyCubeMap);

    std::unique_ptr<vg::GBuffer> mGBuffers[2];

    const MaterialShader* mCloudMaterial = nullptr;
    const MaterialShader* mPostMaterial = nullptr;
    const MaterialShader* mPostPbrMaterial = nullptr;
    const MaterialShader* mBlurMaterial = nullptr;
    const MaterialShader* mCloudShadowMaterial = nullptr;
};

