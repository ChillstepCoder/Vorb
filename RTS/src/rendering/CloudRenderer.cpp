#include "stdafx.h"
#include "CloudRenderer.h"

#include "weather/CloudManager.h"

#include "camera/Camera3D.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialUtils.h"
#include "rendering/StencilBufferIDs.h"
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

#include <Vorb/graphics/GBuffer.h>

CloudRenderer::CloudRenderer(const ui32v2& gbufferDims) {

    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mCloudMaterial = materialManager.getMaterialShader("cloud");
    mPostMaterial = materialManager.getMaterialShader("cloud_post");
    mBlurMaterial = materialManager.getMaterialShader("gaussian_blur_rgb");
    mCloudShadowMaterial = materialManager.getMaterialShader("cloud_shadow_mapper");

    for (int i = 0; i < 2; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(gbufferDims);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGBA8);
    }
    checkGlError("CloudRenderer GBuffer init");
}

void CloudRenderer::renderClouds(const CloudManager& cloudManager, VGTexture sharedDepthStencilTexture, vg::GBuffer* outputGBuffer, const Camera3D& camera) {
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::CLOUD_OR_WATER), 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    assert(outputGBuffer);
    const vg::DepthState prevDepthState = vg::DepthState::CURR;
    const vg::BlendState prevBlendState = vg::BlendState::CURR;
    //vg::DepthState::WRITE.set();
    vg::DepthState::FULL.set();

    mGBuffers[0]->setSharedDepthStencilTexture(sharedDepthStencilTexture);
    mGBuffers[0]->clearAttachment(vg::GBufferAttachmentIndex::ALBEDO);
    mGBuffers[1]->clearAttachment(vg::GBufferAttachmentIndex::ALBEDO);
    mGBuffers[0]->use();

    vg::BlendState::set(vg::BlendStateType::ALPHA);
    MaterialRenderer::bindMaterialForRender(*mCloudMaterial);

    glUniform1f(glGetUniformLocation(mCloudMaterial->mProgram.getID(), "UnYOffset"), 0.0f); // No billboard offset
    const GLuint rootPosUniform = glGetUniformLocation(mCloudMaterial->mProgram.getID(), "UnRootPos");

    for (auto&& batch : cloudManager.mCloudBatches) {
        if (camera.sphereIsVisible(batch.mRootPos, batch.mBoundsRadius)) {
            glUniform3fv(rootPosUniform, 1, &batch.mRootPos.x);
            batch.mMesh->draw();
        }
    }

    // Enable stencil buffer only pass where we have CLOUD
    glStencilFunc(GL_EQUAL, e_cast(StencilBufferIDs::CLOUD_OR_WATER), 0xFF);
    // Disable stencil modification
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    blurNormals();

    outputGBuffer->use();
    renderToOutput();

    // Restore previous
    prevDepthState.set();
    prevBlendState.set();

    glDisable(GL_STENCIL_TEST);
}

void CloudRenderer::renderCloudShadows(const CloudManager& cloudManager, const Camera3D& camera, f32 maxDistance) {
    MaterialRenderer::bindMaterialForRender(*mCloudShadowMaterial);
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);
    glUniform1f(glGetUniformLocation(mCloudShadowMaterial->mProgram.getID(), "UnYOffset"), 0.0f); // No billboard offset
    const GLuint rootPosUniform = glGetUniformLocation(mCloudShadowMaterial->mProgram.getID(), "UnRootPos");

    // All clouds are rendered for shadows
    for (auto&& batch : cloudManager.mCloudBatches) {
        glUniform3fv(rootPosUniform, 1, &batch.mRootPos.x);
        batch.mMesh->draw();
    }
}

void CloudRenderer::blurNormals() {

    ui32 textureUnit = 0;
    MaterialRenderer::bindMaterialForRender(*mBlurMaterial, &textureUnit);

    vg::DepthState::NONE.set();

    const VGUniform& fboUniform = mBlurMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mBlurMaterial->mProgram.getUniform("unDirection");
    glBindTextureUnit(textureUnit, mGBuffers[0]->getAlbedoTexture());
    glBindTextureUnit(textureUnit + 1, mGBuffers[1]->getAlbedoTexture());
    for (int i = 0; i < sDebugOptions.mCloudBlurPasses; ++i) {

        // Horizontal
        mGBuffers[1]->use();
        glUniform1i(fboUniform, textureUnit);
        glUniform2f(dirUniform, sDebugOptions.mCloudBlurRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        mGBuffers[0]->use();
        glUniform1i(fboUniform, textureUnit + 1);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mCloudBlurRadius);
        sGlobalFullQuadVBO.draw();
    }
}

void CloudRenderer::renderToOutput()
{
    //vg::sBlendStates.REPLACE.set();
    ui32 nextTexture = 0;
    MaterialRenderer::bindMaterialForRender(*mPostMaterial, &nextTexture);
    MaterialUtils::uploadLightingUniforms(*mPostMaterial);
    if (const VGUniform* inputUniform = mPostMaterial->mProgram.tryGetUniform("CloudFbo")) {
        mGBuffers[0]->bindAlbedoTexture(nextTexture);
        glUniform1i(*inputUniform, nextTexture++);
    }

    vg::DepthState::NONE.set();
    sGlobalFullQuadVBO.draw();
}
