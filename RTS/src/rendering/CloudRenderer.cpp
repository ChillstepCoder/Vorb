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

#include "options/DebugOptions.h"

CloudRenderer::CloudRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer),
    mGbufferDims(gbufferDims)
{
    mCloudMaterial = mResourceManager.getMaterialManager().getMaterial("cloud");
    mPostMaterial = mResourceManager.getMaterialManager().getMaterial("cloud_post");
    mBlurMaterial = mResourceManager.getMaterialManager().getMaterial("gaussian_blur_rgb");

    // TODO: Shared
    mFullQuadVbo.init();

    vg::GBufferAttachment attachments[1];
    // Color
    attachments[FBO_GEOMETRY_COLOR].format = vg::TextureInternalFormat::RGBA16F;
    attachments[FBO_GEOMETRY_COLOR].number = FBO_GEOMETRY_COLOR;
    attachments[FBO_GEOMETRY_COLOR].pixelFormat = vg::TextureFormat::RGBA;
    attachments[FBO_GEOMETRY_COLOR].pixelType = vg::TexturePixelType::HALF_FLOAT;
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i].setSize(ui32v2(mGbufferDims));
        mGBuffers[i].init(Array<vg::GBufferAttachment>(attachments, 1), vg::TextureInternalFormat::RGBA16F);
    }
    //mGBuffer.initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT24);
    checkGlError("CloudRenderer GBuffer init");
}

void CloudRenderer::renderClouds(const CloudManager& cloudManager, vg::GBuffer* activeGbuffer, const Camera3D& camera) {
    const vg::DepthState prevDepthState = vg::DepthState::CURR;
    //vg::DepthState::WRITE.set();
    vg::DepthState::FULL.set();

    if (!cloudManager.mCloudMesh) {
        cloudManager.mCloudMesh = std::make_unique<BillboardMesh>();
        const SpriteData& spriteData = mResourceManager.getSprite("cloud");
        for (auto&& cloud : cloudManager.mClouds) {
            cloudManager.mCloudMesh->addQuad(cloud.pos, f32v2(cloud.size), f32v2(0.0f), spriteData.atlasPage, spriteData.uvs, COLOR_WHITE, true, 0);
        }
        cloudManager.mCloudMesh->finishMesh(MeshDrawMode::STREAM);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    mGBuffers[1].useGeometry();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[0].useGeometry();
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, activeGbuffer ? activeGbuffer->getDepthTexture() : 0, 0);
    checkGlError("AttachDepthParticle");

    vg::BlendState::set(vg::BlendStateType::ALPHA);
    mMaterialRenderer.renderMesh(*cloudManager.mCloudMesh, *mCloudMaterial);
    

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

void CloudRenderer::blurNormals() {

    ui32 nextTexture = 0;
    mMaterialRenderer.bindMaterialForRender(*mBlurMaterial, &nextTexture);

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    glUniform2f(mBlurMaterial->mProgram.getUniform("unPixelDims"), mGbufferDims.x, mGbufferDims.y);
    const VGUniform& fboUniform = mBlurMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mBlurMaterial->mProgram.getUniform("unDirection");
    for (int i = 0; i < sDebugOptions.mCloudBlurPasses; ++i) {

        // Horizontal
        mGBuffers[0].bindGeometryTexture(0, nextTexture);
        mGBuffers[1].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, sDebugOptions.mCloudBlurRadius, 0.0f);
        mFullQuadVbo.draw();

        // Vertical
        mGBuffers[1].bindGeometryTexture(0, nextTexture);
        mGBuffers[0].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mCloudBlurRadius);
        mFullQuadVbo.draw();
    }
}

void CloudRenderer::renderFboToScreen()
{

    ui32 nextTexture = 0;
    mMaterialRenderer.bindMaterialForRender(*mPostMaterial, &nextTexture);

    glUniform1f(mPostMaterial->mProgram.getUniform("unAmbient"), sDebugOptions.mCloudAmbient);

    if (const VGUniform* inputUniform = mPostMaterial->mProgram.tryGetUniform("CloudFbo")) {
        mGBuffers[0].bindGeometryTexture(0, nextTexture);
        glUniform1i(*inputUniform, nextTexture);
    }

    if (const VGUniform* inputUniform = mPostMaterial->mProgram.tryGetUniform("unPixelDims")) {
        glUniform2f(*inputUniform, mGbufferDims.x, mGbufferDims.y);
    }

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);
    mFullQuadVbo.draw();
}
