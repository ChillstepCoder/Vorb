#include "stdafx.h"
#include "DepthOfFieldPostProcess.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"

#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

DepthOfFieldPostProcess::DepthOfFieldPostProcess(const f32v2& gbufferDims) :
    mGbufferDims(gbufferDims)
{
    vg::GBufferAttachment attachment;
    // Color
    // TODO: SWAP CHAIN
    attachment.format = vg::TextureInternalFormat::RGB8;
    attachment.number = FBO_GEOMETRY_COLOR;
    attachment.pixelFormat = vg::TextureFormat::RGB;
    attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    mGBuffers[0].setSize(ui32v2(mGbufferDims));
    mGBuffers[0].init(attachment, nullptr, nullptr);
    mGBuffers[1].setSize(ui32v2(mGbufferDims));
    mGBuffers[1].init(attachment, nullptr, nullptr);

    mMaterial = Services::ResourceManager::ref().getMaterialManager().getMaterial("depth_of_field");

    checkGlError("init DepthOfFieldPostProcess");
}

vg::GBuffer* DepthOfFieldPostProcess::render(vg::GBuffer* prevGBuffer)
{
    if (sDebugOptions.mDepthOfFieldBlurPasses == 0) {
        return prevGBuffer;
    }
    assert(prevGBuffer);

    ui32 nextTexture;
    MaterialRenderer::bindMaterialForRender(*mMaterial, &nextTexture);
    glUniform2fv(mMaterial->getUniform("unBlurRangeNear"), 1, &sDebugOptions.mDepthOfFieldRangeNear.x);
    glUniform2fv(mMaterial->getUniform("unBlurRangeFar"), 1, &sDebugOptions.mDepthOfFieldRangeFar.x);
    glUniform1f(mMaterial->getUniform("unBlurExponent"), sDebugOptions.mDepthOfFieldExponent);
    if (sDebugOptions.mDepthOfFieldDebugRender) {
        glUniform1f(mMaterial->getUniform("unDebugRender"), 1.0f);
    }
    else {
        glUniform1f(mMaterial->getUniform("unDebugRender"), 0.0f);
    }

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    // Clearing in this order to suppress REDUNDANT_FBO_BIND warning
    mGBuffers[1].useGeometry();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[0].useGeometry();
    glClear(GL_COLOR_BUFFER_BIT);

    const VGUniform& fboUniform = mMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mMaterial->mProgram.getUniform("unDirection");
    prevGBuffer->bindGeometryTexture(nextTexture);
    for (int i = 0; i < sDebugOptions.mDepthOfFieldBlurPasses; ++i) {

        // Horizontal
        mGBuffers[1].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, sDebugOptions.mDepthOfFieldBlurRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        mGBuffers[1].bindGeometryTexture(nextTexture);
        mGBuffers[0].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mDepthOfFieldBlurRadius);
        sGlobalFullQuadVBO.draw();

        mGBuffers[0].bindGeometryTexture(nextTexture);
    }

    // Share textures with previous gbuffer since this will become new active gbuffer
    mGBuffers[0].setDepthTexture(prevGBuffer->getDepthTexture());
    mGBuffers[0].setNormalTexture(prevGBuffer->getNormalTexture());
    mGBuffers[0].setRoughnessTexture(prevGBuffer->getRoughnessTexture());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, prevGBuffer->getDepthTexture(), 0);

    return &mGBuffers[0];
}
