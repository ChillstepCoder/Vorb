#include "stdafx.h"
#include "DepthOfFieldPostProcess.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"

#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>

#include "options/DebugOptions.h"

#include <Vorb/graphics/GBuffer.h>

DepthOfFieldPostProcess::DepthOfFieldPostProcess(const ui32v2& gbufferDims) {
    // Blending the HDR post lightingcolor hence RGB16F
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(gbufferDims);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB16F);
    }

    mMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("depth_of_field"));

    checkGlError("init DepthOfFieldPostProcess");
}

vg::GBuffer* DepthOfFieldPostProcess::render(vg::GBuffer* prevGBuffer) {

    const MaterialShaderDef* shaderDef = mMaterial->tryGetLoadedAsset();
    if (!shaderDef) {
        return prevGBuffer;
    }

    if (sDebugOptions.mDepthOfFieldBlurPasses == 0) {
        return prevGBuffer;
    }
    assert(prevGBuffer);

    ui32 nextTexture;
    MaterialRenderer::bindMaterialShaderForRender(*shaderDef, &nextTexture);
    glUniform2fv(shaderDef->getUniform("unBlurRangeNear"), 1, &sDebugOptions.mDepthOfFieldRangeNear.x);
    glUniform2fv(shaderDef->getUniform("unBlurRangeFar"), 1, &sDebugOptions.mDepthOfFieldRangeFar.x);
    glUniform1f(shaderDef->getUniform("unBlurExponent"), sDebugOptions.mDepthOfFieldExponent);
    if (sDebugOptions.mDepthOfFieldDebugRender) {
        glUniform1f(shaderDef->getUniform("unDebugRender"), 1.0f);
    }
    else {
        glUniform1f(shaderDef->getUniform("unDebugRender"), 0.0f);
    }

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    // Clearing in this order to suppress REDUNDANT_FBO_BIND warning
    mGBuffers[1]->use();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[0]->use();
    glClear(GL_COLOR_BUFFER_BIT);

    const VGUniform& fboUniform = shaderDef->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = shaderDef->mProgram.getUniform("unDirection");
    glUniform1i(fboUniform, nextTexture);
    prevGBuffer->bindAlbedoTexture(nextTexture);
    for (int i = 0; i < sDebugOptions.mDepthOfFieldBlurPasses; ++i) {

        // Horizontal
        mGBuffers[1]->use();
        glUniform2f(dirUniform, sDebugOptions.mDepthOfFieldBlurRadius, 0.0f);
        sGlobalFullTriangleVAO.draw();

        // Vertical
        mGBuffers[1]->bindAlbedoTexture(nextTexture);
        mGBuffers[0]->use();
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mDepthOfFieldBlurRadius);
        sGlobalFullTriangleVAO.draw();

        mGBuffers[0]->bindAlbedoTexture(nextTexture);
    }

    // Share textures with previous gbuffer since this will become new active gbuffer
    mGBuffers[0]->setSharedDepthTexture(prevGBuffer->getDepthTexture());
    mGBuffers[0]->setNormalTexture(prevGBuffer->getNormalTexture());
    mGBuffers[0]->setTertiaryTexture1(prevGBuffer->getTertiaryTexture1());

    vg::BlendState::restorePrevious();

    return mGBuffers[0].get();
}
