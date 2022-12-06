#pragma once

class MaterialShader;

#include <Vorb/graphics/GBuffer.h>

class AmbientOcclusionPostProcess {
public:
    AmbientOcclusionPostProcess(const f32v2& gbufferDims);

    // Returns target gbuffer
    void render(vg::GBuffer* activeGBuffer);

    VGTexture getSSAOTexture() const;

private:

    // TODO: Maybe shared g buffer? :thinkies:
    vg::GBuffer mGBuffers[2];
    f32v2 mGbufferDims;

    VGTexture mNoiseTexture;
    std::vector<f32v3> mSsaoKernel;

    const MaterialShader* mMaterial = nullptr;
    const MaterialShader* mApplyMaterial = nullptr;
    const MaterialShader* mBlurMaterial = nullptr;
};
