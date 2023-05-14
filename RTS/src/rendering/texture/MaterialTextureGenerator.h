#pragma once

DECL_VG(class GLProgram);
DECL_VG(class SamplerState);

#include <gli/texture2d.hpp>

class MaterialTextureGenerator
{
public:
    MaterialTextureGenerator();
    ~MaterialTextureGenerator();
    void init();
    VGTexture generateNormalTexture(VGTexture input, const ui32v2& dims, const vg::SamplerState& samplerState);
    VGTexture generateAoRoughnessMetallicTexture(const gli::texture2d& ao, const gli::texture2d& roughness, const gli::texture2d& metallic, const ui32v2& dims, const vg::SamplerState& samplerState);

private:
    VGUniform mUvRectUniform;
    VGUniform mTextureUniform;
    VGUniform mPixelDimsUniform;
    std::unique_ptr<vg::GLProgram> mNormalProgram;
    std::unique_ptr<vg::GLProgram> mStencilProgram;
    VGFramebuffer mFramebufferID;
};

