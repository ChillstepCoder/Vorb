#include "stdafx.h"
#include "SmudgeRenderer.h"

#include "options/DebugOptions.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "camera/Camera3D.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/FullQuadVBO.h>

SmudgeRenderer::SmudgeRenderer(const ui32v2& screenResolution) {
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(screenResolution);
        // Lower color precision since we don't need the hdr color
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGBA8);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::NORMALS, vg::TextureInternalFormat::RGB10);
        // No depth as we will share the depth texture with the main attachment
    }


    mSmudgeShader = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("smudge");
}

SmudgeRenderer::~SmudgeRenderer() {
}

void SmudgeRenderer::beginSmudgePass(vg::GBuffer* activeGBuffer) {
    assert(activeGBuffer->getSize() == mGBuffers[0]->getSize());
    if (sDebugOptions.mSmudgeTestDisable) {
        return;
    }
    if (activeGBuffer->hasStencil()) {
        // All GBuffers will use same depth/stencil
        mGBuffers[0]->setSharedDepthStencilTexture(activeGBuffer->getDepthTexture());
        mGBuffers[1]->setSharedDepthStencilTexture(activeGBuffer->getDepthTexture());
        // Enable stencil buffer to set 1s whenever we add a fragment
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }
    else {
        mGBuffers[0]->setSharedDepthTexture(activeGBuffer->getDepthTexture());
        //mGBuffers[0]->use();
    }

    // TODO: If smudge test passes is always 1, we only need a single gbuffer
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::ALBEDO);
        mGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::NORMALS);
    }
}

void SmudgeRenderer::renderSmudge(vg::GBuffer* activeGBuffer, const Camera3D& camera) {
    if (sDebugOptions.mSmudgeTestDisable) {
        return;
    }

    ui32 freeTextureIndex;
    MaterialRenderer::bindMaterialForRender(*mSmudgeShader, &freeTextureIndex);

    const VGUniform& albedoUniform = mSmudgeShader->mProgram.getUniform("unAlbedoFbo");
    const VGUniform& normalUniform = mSmudgeShader->mProgram.getUniform("unNormalFbo");
    const VGUniform& depthUniform = mSmudgeShader->mProgram.getUniform("unDepthFbo");
    const VGUniform& dirUniform = mSmudgeShader->mProgram.getUniform("unDirection");
    glUniform1i(albedoUniform, freeTextureIndex);
    glUniform1i(normalUniform, freeTextureIndex + 1);
    glUniform1i(depthUniform, freeTextureIndex + 2);
    glUniform1f(mSmudgeShader->mProgram.getUniform("unNormalThreshold"), sDebugOptions.mSmudgeTestNormThreshold);
    glUniform1f(mSmudgeShader->mProgram.getUniform("unDepthThreshold"), sDebugOptions.mSmudgeTestDepthThreshold);
    glUniform1i(mSmudgeShader->mProgram.getUniform("unShowVariance"), sDebugOptions.mSmudgeTestShowVariance);
    glUniform1i(mSmudgeShader->mProgram.getUniform("unShowEdges"), sDebugOptions.mSmudgeTestShowEdges);
    glUniform2f(mSmudgeShader->mProgram.getUniform("unScreenResolution"), mGBuffers[0]->getWidth(), mGBuffers[0]->getHeight());
    glUniform2f(mSmudgeShader->mProgram.getUniform("unCameraZRange"), camera.getZNear(), camera.getZFar());
    activeGBuffer->bindDepthTexture(freeTextureIndex + 2);
    vg::DepthState::NONE.set();

    if (activeGBuffer->hasStencil()) {
        // Enable stencil buffer only pass where we have 1s
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        // Disable stencil modification
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }

    activeGBuffer->bindAlbedoTexture(freeTextureIndex);
    activeGBuffer->bindNormalTexture(freeTextureIndex + 1);

    assert(sDebugOptions.mSmudgeTestPasses > 0);
    for (int i = 0;; ++i) {
        // Horizontal
        mGBuffers[1]->use();
        // Replace normals TODO: Build into gbuffer
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
        glUniform2f(dirUniform, sDebugOptions.mSmudgeTestRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        // Replace normals TODO: Build into gbuffer
        mGBuffers[1]->bindAlbedoTexture(freeTextureIndex);
        mGBuffers[1]->bindNormalTexture(freeTextureIndex + 1);
        // Last pass composites onto main scene
        const bool isLastPass = (i == sDebugOptions.mSmudgeTestPasses - 1);
        if (isLastPass) {
            activeGBuffer->use();
        }
        else {
            mGBuffers[0]->use();
        }
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mSmudgeTestRadius);
        sGlobalFullQuadVBO.draw();

        if (isLastPass) {
            break;
        }
        else {
            mGBuffers[0]->bindAlbedoTexture(freeTextureIndex);
            mGBuffers[0]->bindNormalTexture(freeTextureIndex + 1);
        }
    }
    vg::DepthState::restorePrevious();

    if (activeGBuffer->hasStencil()) {
        glDisable(GL_STENCIL_TEST);
    }
}
