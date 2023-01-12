#include "stdafx.h"
#include "CloudRenderer.h"

#include "weather/CloudManager.h"

#include "camera/Camera3D.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialUtils.h"
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

void CloudRenderer::renderClouds(const CloudManager& cloudManager, vg::GBuffer* activeGbuffer, const Camera3D& camera) {
    assert(activeGbuffer);
    const vg::DepthState prevDepthState = vg::DepthState::CURR;
    //vg::DepthState::WRITE.set();
    vg::DepthState::FULL.set();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    mGBuffers[1]->use();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[0]->use();;
    glClear(GL_COLOR_BUFFER_BIT);
    // Depth share
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, activeGbuffer->getDepthTexture(), 0);

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

    blurNormals();

    if (activeGbuffer) {
        activeGbuffer->use();
    }
    else {
        vg::GBuffer::unuse();
    }

    renderFboToScreen();

    // Restore previous
    prevDepthState.set();
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

    ui32 nextTexture = 0;
    MaterialRenderer::bindMaterialForRender(*mBlurMaterial, &nextTexture);

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    const VGUniform& fboUniform = mBlurMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mBlurMaterial->mProgram.getUniform("unDirection");
    for (int i = 0; i < sDebugOptions.mCloudBlurPasses; ++i) {

        // Horizontal
        mGBuffers[0]->bindAlbedoTexture(nextTexture);
        mGBuffers[1]->use();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, sDebugOptions.mCloudBlurRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        mGBuffers[1]->bindAlbedoTexture(nextTexture);
        mGBuffers[0]->use();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mCloudBlurRadius);
        sGlobalFullQuadVBO.draw();
    }
}

void CloudRenderer::renderFboToScreen()
{

    ui32 nextTexture = 0;
    MaterialRenderer::bindMaterialForRender(*mPostMaterial, &nextTexture);
    MaterialUtils::uploadLightingUniforms(*mPostMaterial);
    if (const VGUniform* inputUniform = mPostMaterial->mProgram.tryGetUniform("CloudFbo")) {
        mGBuffers[0]->bindAlbedoTexture(nextTexture);
        glUniform1i(*inputUniform, nextTexture++);
    }

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);
    sGlobalFullQuadVBO.draw();
}
