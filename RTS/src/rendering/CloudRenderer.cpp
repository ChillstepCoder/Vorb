#include "stdafx.h"
#include "CloudRenderer.h"

#include "weather/CloudManager.h"

#include "camera/Camera3D.h"
#include "ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/QuadMesh.h"
#include "rendering/MaterialUtils.h"
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

CloudRenderer::CloudRenderer(const MaterialRenderer& materialRenderer, const f32v2& gbufferDims) :
    mMaterialRenderer(materialRenderer),
    mGbufferDims(gbufferDims)
{
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mCloudMaterial = materialManager.getMaterial("cloud");
    mPostMaterial = materialManager.getMaterial("cloud_post");
    mBlurMaterial = materialManager.getMaterial("gaussian_blur_rgb");
    mCloudShadowMaterial = materialManager.getMaterial("cloud_shadow_mapper");

    vg::GBufferAttachment mainAttachment;
    // Color
    mainAttachment.format = vg::TextureInternalFormat::RGBA8;
    mainAttachment.number = FBO_GEOMETRY_COLOR;
    mainAttachment.pixelFormat = vg::TextureFormat::RGBA;
    mainAttachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i].setSize(ui32v2(mGbufferDims));
        mGBuffers[i].init(mainAttachment, nullptr, nullptr);
    }
    checkGlError("CloudRenderer GBuffer init");
}

void CloudRenderer::renderClouds(const CloudManager& cloudManager, vg::GBuffer* activeGbuffer, const Camera3D& camera) {
    assert(activeGbuffer);
    const vg::DepthState prevDepthState = vg::DepthState::CURR;
    //vg::DepthState::WRITE.set();
    vg::DepthState::FULL.set();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    mGBuffers[1].useGeometry();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[0].useGeometry();
    glClear(GL_COLOR_BUFFER_BIT);
    // Depth share
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, activeGbuffer->getDepthTexture(), 0);

    vg::BlendState::set(vg::BlendStateType::ALPHA);
    mMaterialRenderer.bindMaterialForRender(*mCloudMaterial);

    glUniform1f(glGetUniformLocation(mCloudMaterial->mProgram.getID(), "UnYOffset"), 0.0f); // No billboard offset
    const GLuint rootPosUniform = glGetUniformLocation(mCloudMaterial->mProgram.getID(), "UnRootPos");

    for (auto&& batch : cloudManager.mCloudBatches) {
        if (camera.sphereIsVisible(batch.mRootPos, batch.mBoundsRadius)) {
            glUniform3fv(rootPosUniform, 1, &batch.mRootPos.x);
            batch.mMesh->draw(mCloudMaterial->mProgram);
        }
    }

    blurNormals();

    if (activeGbuffer) {
        activeGbuffer->useGeometry();
    }
    else {
        vg::GBuffer::unuse();
    }

    renderFboToScreen();

    // Restore previous
    prevDepthState.set();
}

void CloudRenderer::renderCloudShadows(const CloudManager& cloudManager, const Camera3D& camera, f32 maxDistance) {
    mMaterialRenderer.bindMaterialForRender(*mCloudShadowMaterial);
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);
    glUniform1f(glGetUniformLocation(mCloudShadowMaterial->mProgram.getID(), "UnYOffset"), 0.0f); // No billboard offset
    const GLuint rootPosUniform = glGetUniformLocation(mCloudShadowMaterial->mProgram.getID(), "UnRootPos");

    // All clouds are rendered for shadows
    for (auto&& batch : cloudManager.mCloudBatches) {
        glUniform3fv(rootPosUniform, 1, &batch.mRootPos.x);
        batch.mMesh->draw(mCloudShadowMaterial->mProgram);
    }
}

void CloudRenderer::blurNormals() {

    ui32 nextTexture = 0;
    mMaterialRenderer.bindMaterialForRender(*mBlurMaterial, &nextTexture);

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    const VGUniform& fboUniform = mBlurMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mBlurMaterial->mProgram.getUniform("unDirection");
    for (int i = 0; i < sDebugOptions.mCloudBlurPasses; ++i) {

        // Horizontal
        mGBuffers[0].bindGeometryTexture(nextTexture);
        mGBuffers[1].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, sDebugOptions.mCloudBlurRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        mGBuffers[1].bindGeometryTexture(nextTexture);
        mGBuffers[0].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mCloudBlurRadius);
        sGlobalFullQuadVBO.draw();
    }
}

void CloudRenderer::renderFboToScreen()
{

    ui32 nextTexture = 0;
    mMaterialRenderer.bindMaterialForRender(*mPostMaterial, &nextTexture);
    MaterialUtils::uploadLightingUniforms(*mPostMaterial);
    if (const VGUniform* inputUniform = mPostMaterial->mProgram.tryGetUniform("CloudFbo")) {
        mGBuffers[0].bindGeometryTexture(nextTexture);
        glUniform1i(*inputUniform, nextTexture++);
    }

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);
    sGlobalFullQuadVBO.draw();
}
