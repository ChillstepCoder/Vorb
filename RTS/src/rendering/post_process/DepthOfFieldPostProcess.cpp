#include "stdafx.h"
#include "DepthOfFieldPostProcess.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"

#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

#include <Vorb/graphics/GBuffer.h>

DepthOfFieldPostProcess::DepthOfFieldPostProcess(const ui32v2& gbufferDims) {
    // Blending the HDR post lightingcolor hence RGB16F
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(gbufferDims);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB16F);
    }

    mMaterial = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("depth_of_field");

    checkGlError("init DepthOfFieldPostProcess");
}

vg::GBuffer* DepthOfFieldPostProcess::render(vg::GBuffer* prevGBuffer) {

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
    mGBuffers[1]->use();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[0]->use();
    glClear(GL_COLOR_BUFFER_BIT);

    const VGUniform& fboUniform = mMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mMaterial->mProgram.getUniform("unDirection");
    glUniform1i(fboUniform, nextTexture);
    prevGBuffer->bindAlbedoTexture(nextTexture);
    for (int i = 0; i < sDebugOptions.mDepthOfFieldBlurPasses; ++i) {

        // Horizontal
        mGBuffers[1]->use();
        glUniform2f(dirUniform, sDebugOptions.mDepthOfFieldBlurRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        mGBuffers[1]->bindAlbedoTexture(nextTexture);
        mGBuffers[0]->use();
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mDepthOfFieldBlurRadius);
        sGlobalFullQuadVBO.draw();

        mGBuffers[0]->bindAlbedoTexture(nextTexture);
    }

    // Share textures with previous gbuffer since this will become new active gbuffer
    mGBuffers[0]->setSharedDepthTexture(prevGBuffer->getDepthTexture());
    mGBuffers[0]->setNormalTexture(prevGBuffer->getNormalTexture());
    mGBuffers[0]->setTertiaryTexture(prevGBuffer->getTertiaryTexture());

    vg::BlendState::restorePrevious();

    return mGBuffers[0].get();
}
