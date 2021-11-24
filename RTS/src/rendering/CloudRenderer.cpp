#include "stdafx.h"
#include "CloudRenderer.h"

#include "weather/CloudManager.h"

#include "ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/QuadMesh.h"
#include "rendering/SpriteData.h"
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

CloudRenderer::CloudRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer),
    mGbufferDims(gbufferDims)
{

    mCloudMaterial = mResourceManager.getMaterialManager().getMaterial("cloud");
    mPostMaterial = mResourceManager.getMaterialManager().getMaterial("cloud_post");
    mBlurMaterial = mResourceManager.getMaterialManager().getMaterial("gaussian_blur_rgb");
    mCloudShadowMaterial = mResourceManager.getMaterialManager().getMaterial("cloud_shadow_mapper");

    vg::GBufferAttachment attachment;
    // Color
    attachment.format = vg::TextureInternalFormat::RGBA8;
    attachment.number = FBO_GEOMETRY_COLOR;
    attachment.pixelFormat = vg::TextureFormat::RGBA;
    attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i].setSize(ui32v2(mGbufferDims));
        mGBuffers[i].init(attachment, nullptr, nullptr);
    }
    checkGlError("CloudRenderer GBuffer init");
}

void CloudRenderer::renderClouds(const CloudManager& cloudManager, vg::GBuffer* activeGbuffer, const Camera3D& camera) {
    assert(activeGbuffer);
    const vg::DepthState prevDepthState = vg::DepthState::CURR;
    //vg::DepthState::WRITE.set();
    vg::DepthState::FULL.set();

    if (!cloudManager.mCloudMesh) {
        cloudManager.mCloudMesh = std::make_unique<TBOBillboardMesh>();
        //cloudManager.mCloudMesh->setDepthSortMode(DepthSortMode::BACK_TO_FRONT);
        const SpriteData& spriteData = mResourceManager.getSprite("cloud");
        for (auto&& cloud : cloudManager.mClouds) {
            cloudManager.mCloudMesh->addQuad(cloud.pos, f32v2(cloud.size), f32v2(0.0f, -cloud.size * 0.5f), spriteData.atlasPage, spriteData.uvs, COLOR_WHITE, true, 0u, 240u);
        }
        cloudManager.mCloudMesh->finishMesh(MeshDrawMode::STREAM);
    }

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
    cloudManager.mCloudMesh->draw(mCloudMaterial->mProgram);

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

void CloudRenderer::renderCloudShadows(const CloudManager& cloudManager) {
    mMaterialRenderer.bindMaterialForRender(*mCloudShadowMaterial);
    glUniform1f(glGetUniformLocation(mCloudShadowMaterial->mProgram.getID(), "UnYOffset"), 0.0f); // No billboard offset
    if (cloudManager.mCloudMesh) {
        cloudManager.mCloudMesh->draw(mCloudShadowMaterial->mProgram);
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

    glUniform1f(mPostMaterial->mProgram.getUniform("unAmbient"), sDebugOptions.mCloudAmbient);

    if (const VGUniform* inputUniform = mPostMaterial->mProgram.tryGetUniform("CloudFbo")) {
        mGBuffers[0].bindGeometryTexture(nextTexture);
        glUniform1i(*inputUniform, nextTexture);
    }

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);
    sGlobalFullQuadVBO.draw();
}
