#pragma once

DECL_VG(class GLProgram);
DECL_VG(class SamplerState);

class NormalMapGenerator
{
public:
    NormalMapGenerator();
    ~NormalMapGenerator();
    void init();
    VGTexture generateNormalTexture(VGTexture input, const ui32v2& dims, const vg::SamplerState& samplerState);

private:
    VGUniform mUvRectUniform;
    VGUniform mTextureUniform;
    VGUniform mPixelDimsUniform;
    std::unique_ptr<vg::GLProgram> mNormalProgram;
    std::unique_ptr<vg::GLProgram> mStencilProgram;
    VGFramebuffer mFramebufferID;
};

