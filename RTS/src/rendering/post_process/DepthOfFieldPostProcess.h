#pragma once

class MaterialShaderDef;

DECL_VG(class GBuffer);

class DepthOfFieldPostProcess {
public:
    DepthOfFieldPostProcess(const ui32v2& gbufferDims);

    // Returns target gbuffer
    vg::GBuffer* render(vg::GBuffer* prevGBuffer);

private:

    // TODO: Maybe shared g buffer? :thinkies:
    std::unique_ptr<vg::GBuffer> mGBuffers[2];

    AssetHandlePtr<MaterialShaderDef> mMaterial;
};

