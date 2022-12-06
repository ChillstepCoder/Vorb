#pragma once

class CloudManager;
class ResourceManager;
class MaterialShader;
class Camera3D;

#include <Vorb/graphics/GBuffer.h>

// TODO: Read this : https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.17.2030&rep=rep1&type=pdf
class CloudRenderer
{
public:
    CloudRenderer(const f32v2& gbufferDims);

    void renderClouds(const CloudManager& cloudManager, vg::GBuffer* activeGbuffer, const Camera3D& camera);
    void renderCloudShadows(const CloudManager& cloudManager, const Camera3D& camera, f32 maxDistance);

private:
    void blurNormals();
    void renderFboToScreen();

    vg::GBuffer mGBuffers[2];
    f32v2 mGbufferDims;

    const MaterialShader* mCloudMaterial = nullptr;
    const MaterialShader* mPostMaterial = nullptr;
    const MaterialShader* mBlurMaterial = nullptr;
    const MaterialShader* mCloudShadowMaterial = nullptr;
};

