#pragma once

class MaterialShader;

DECL_VG(class GBuffer);

class AmbientOcclusionPostProcess {
public:
    AmbientOcclusionPostProcess(const ui32v2& gbufferDims);

    // Returns target gbuffer
    void render(vg::GBuffer* activeGBuffer);

    VGTexture getSSAOTexture() const;

private:

    // TODO: Maybe shared g buffer? :thinkies:
    std::unique_ptr<vg::GBuffer> mGBuffers[2];

    VGTexture mNoiseTexture;
    std::vector<f32v3> mSsaoKernel;

    const MaterialShader* mMaterial = nullptr;
    const MaterialShader* mApplyMaterial = nullptr;
    const MaterialShader* mBlurMaterial = nullptr;
};
