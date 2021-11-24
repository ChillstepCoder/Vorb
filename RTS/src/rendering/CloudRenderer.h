#pragma once

class CloudManager;
class ResourceManager;
class MaterialRenderer;
class Material;
class Camera3D;

#include <Vorb/graphics/GBuffer.h>

// TODO: Read this : https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.17.2030&rep=rep1&type=pdf
class CloudRenderer
{
public:
    CloudRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims);

    void renderClouds(const CloudManager& cloudManager, vg::GBuffer* activeGbuffer, const Camera3D& camera);
    void renderCloudShadows(const CloudManager& cloudManager);

private:
    void blurNormals();
    void renderFboToScreen();

    ResourceManager& mResourceManager;
    const MaterialRenderer& mMaterialRenderer;

    vg::GBuffer mGBuffers[2];
    f32v2 mGbufferDims;

    const Material* mCloudMaterial = nullptr;
    const Material* mPostMaterial = nullptr;
    const Material* mBlurMaterial = nullptr;
    const Material* mCloudShadowMaterial = nullptr;
};

