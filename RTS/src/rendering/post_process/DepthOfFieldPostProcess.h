#pragma once

class ResourceManager;
class Material;

#include <Vorb/graphics/GBuffer.h>

class DepthOfFieldPostProcess {
public:
    DepthOfFieldPostProcess(const f32v2& gbufferDims);

    // Returns target gbuffer
    vg::GBuffer* render(vg::GBuffer* prevGBuffer);

private:

    // TODO: Maybe shared g buffer? :thinkies:
    vg::GBuffer mGBuffers[2];
    f32v2 mGbufferDims;

    const Material* mMaterial = nullptr;
};

