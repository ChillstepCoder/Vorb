#pragma once

class ResourceManager;
class MaterialRenderer;
class Material;

#include <Vorb/graphics/GBuffer.h>

class DepthOfFieldPostProcess {
public:
    DepthOfFieldPostProcess(const MaterialRenderer& materialRenderer, const f32v2& gbufferDims);

    // Returns target gbuffer
    vg::GBuffer* render(vg::GBuffer* prevGBuffer);

private:
    const MaterialRenderer& mMaterialRenderer;

    // TODO: Maybe shared g buffer? :thinkies:
    vg::GBuffer mGBuffers[2];
    f32v2 mGbufferDims;

    const Material* mMaterial = nullptr;
};

