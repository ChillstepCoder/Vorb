#include "stdafx.h"
#include "DepthOfFieldPostProcess.h"

#include "ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"

#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

DepthOfFieldPostProcess::DepthOfFieldPostProcess(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims) :
    mResourceManager(resourceManager), mMaterialRenderer(materialRenderer), mGbufferDims(gbufferDims)
{
    vg::GBufferAttachment attachment;
    // Color
    // TODO: SWAP CHAIN
    attachment.format = vg::TextureInternalFormat::RGB8;
    attachment.number = FBO_GEOMETRY_COLOR;
    attachment.pixelFormat = vg::TextureFormat::RGB;
    attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    mGBuffers[0].setSize(ui32v2(mGbufferDims));
    mGBuffers[0].init(attachment, nullptr);
    mGBuffers[1].setSize(ui32v2(mGbufferDims));
    mGBuffers[1].init(attachment, nullptr);

    mMaterial = mResourceManager.getMaterialManager().getMaterial("depth_of_field");

    checkGlError("init DepthOfFieldPostProcess");
}

vg::GBuffer* DepthOfFieldPostProcess::render(vg::GBuffer* prevGBuffer)
{
    if (sDebugOptions.mDepthOfFieldBlurPasses == 0) {
        return prevGBuffer;
    }
    assert(prevGBuffer);
    mGBuffers[0].useGeometry();

    ui32 nextTexture;
    mMaterialRenderer.bindMaterialForRender(*mMaterial, &nextTexture);

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    mGBuffers[0].useGeometry();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[1].useGeometry();
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
    mGBuffers[0].setLightTexture(prevGBuffer->getLightTexture());
    mGBuffers[0].setNormalTexture(prevGBuffer->getNormalTexture());
    mGBuffers[0].setFboLight(prevGBuffer->getFboLight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, prevGBuffer->getDepthTexture(), 0);

    return &mGBuffers[0];
}
