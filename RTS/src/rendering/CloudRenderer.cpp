#include "stdafx.h"
#include "CloudRenderer.h"

#include "weather/CloudMeshManager.h"

#include "camera/Camera3D.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshDrawer.h"
#include "rendering/MaterialUtils.h"
#include "rendering/StencilBufferIDs.h"
#include "definitions/rendering/CubemapDef.h"
#include "rendering/material/BrdfLUT.h"
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>

#include "rendering/post_process/ShadowPassShaderData.h"

#include "options/LightingOptions.h"
#include "options/DebugOptions.h"

#include <Vorb/graphics/GBuffer.h>

CloudRenderer::CloudRenderer(const ui32v2& gbufferDims) {

    MaterialShaderRepository& materialShaderRepo = MaterialShaderRepository::get();

    mCloudMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssetHandles, CStrToken("cloud"));
    mPostMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssetHandles, CStrToken("cloud_post"));
    mPostPbrMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssetHandles, CStrToken("cloud_post_pbr"));
    mBlurMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssetHandles, CStrToken("gaussian_blur_rgb"));
    mCloudShadowMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssetHandles, CStrToken("cloud_shadow_mapper"));

    for (int i = 0; i < 2; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(gbufferDims);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGBA8);
    }
    checkGlError("CloudRenderer GBuffer init");
}

void CloudRenderer::renderClouds(const CloudMeshManager& cloudManager, VGTexture sharedDepthStencilTexture, vg::GBuffer* outputGBuffer, const Camera3D& camera, const CubemapDef& skyCubeMap) {
    if (!mShaderAssetHandles.areAllAssetsLoaded()) {
        return;
    }
    
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
    MaterialRenderer::bindMaterialShaderForRender(*mCloudMaterial);

    glUniform1f(glGetUniformLocation(mCloudMaterial->mProgram.getID(), "UnYOffset"), 0.0f); // No billboard offset
    const GLuint rootPosUniform = glGetUniformLocation(mCloudMaterial->mProgram.getID(), "UnRootPos");

    for (auto&& batch : cloudManager.mCloudBatches) {
        if (camera.sphereIsVisible(batch.mRootPos, batch.mBoundsRadius)) {
            glUniform3fv(rootPosUniform, 1, &batch.mRootPos.x);
            MeshDrawer::draw(batch.mMesh->mGpuData);
        }
    }

    // Enable stencil buffer only pass where we have CLOUD
    glStencilFunc(GL_EQUAL, e_cast(StencilBufferIDs::CLOUD_OR_WATER), 0xFF);
    // Disable stencil modification
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    blurNormals();

    outputGBuffer->use();
    renderToOutput(skyCubeMap);

    // Restore previous
    prevDepthState.set();
    prevBlendState.set();

    glDisable(GL_STENCIL_TEST);
}

void CloudRenderer::renderCloudShadows(const ShadowPassShaderData& shaderData, const CloudMeshManager& cloudManager, const Camera3D& camera, f32 maxDistance) {
    if (!mShaderAssetHandles.areAllAssetsLoaded()) {
        return;
    }
    
    MaterialRenderer::bindMaterialShaderForRender(*mCloudShadowMaterial);
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);
    glUniform1f(glGetUniformLocation(mCloudShadowMaterial->mProgram.getID(), "UnYOffset"), 0.0f); // No billboard offset
    const GLuint rootPosUniform = glGetUniformLocation(mCloudShadowMaterial->mProgram.getID(), "UnRootPos");
    glUniformMatrix4fv(mCloudShadowMaterial->getUniform("unShadowFrustumMatrices[0]"), MAX_SHADOW_CASCADE_LEVELS, false, &(*shaderData.shadowFrustumMatrices)[0][0]);

    // All clouds are rendered for shadows
    for (auto&& batch : cloudManager.mCloudBatches) {
        glUniform3fv(rootPosUniform, 1, &batch.mRootPos.x);
        MeshDrawer::draw(batch.mMesh->mGpuData);
    }
}

void CloudRenderer::blurNormals() {

    ui32 textureUnit = 0;
    MaterialRenderer::bindMaterialShaderForRender(*mBlurMaterial, &textureUnit);

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
        sGlobalFullTriangleVAO.draw();

        // Vertical
        mGBuffers[0]->use();
        glUniform1i(fboUniform, textureUnit + 1);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mCloudBlurRadius);
        sGlobalFullTriangleVAO.draw();
    }
}

void CloudRenderer::renderToOutput(const CubemapDef& skyCubeMap)
{
    //vg::sBlendStates.REPLACE.set();
    ui32 textureUnit = 0;

    if (sDebugOptions.mUsingPBR) {
        MaterialRenderer::bindMaterialShaderForRender(*mPostPbrMaterial, &textureUnit);
        LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
        LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
        glUniform1i(mPostPbrMaterial->getUniform("unIrradianceMap"), textureUnit);
        glBindTextureUnit(textureUnit++, skyCubeMap.getIrradianceTexture());
        glUniform1i(mPostPbrMaterial->getUniform("unPrefilterMap"), textureUnit);
        glBindTextureUnit(textureUnit++, skyCubeMap.getPrefilterMap());
        glUniform1i(mPostPbrMaterial->getUniform("unBrdfLUT"), textureUnit);
        glBindTextureUnit(textureUnit++, BrdfLUT::getTexture());

        glUniform1f(mPostPbrMaterial->getUniform("unCloudMetallic"), sDebugOptions.mCloudMetallic);
        glUniform1f(mPostPbrMaterial->getUniform("unCloudRoughness"), sDebugOptions.mCloudRoughness);

        glUniform2f(mPostPbrMaterial->getUniform("unHazeExponent"), optionsLeft.mHazeExponent, optionsRight.mHazeExponent);
        if (sDebugOptions.mIsCameraUnderwater) {
            glUniform2f(mPostPbrMaterial->getUniform("unHazeDivisor"), sDebugOptions.mUnderwaterHazeDivisor, sDebugOptions.mUnderwaterHazeDivisor);
        }
        else {
            glUniform2f(mPostPbrMaterial->getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
        }
        glUniform2f(mPostPbrMaterial->getUniform("unAmbient"), optionsLeft.mAmbient, optionsRight.mAmbient);
        glUniform2f(mPostPbrMaterial->getUniform("unExposure"), optionsLeft.mExposure, optionsRight.mExposure);
        glUniform2f(mPostPbrMaterial->getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
        glUniform2f(mPostPbrMaterial->getUniform("unGamma"), optionsLeft.mGamma, optionsRight.mGamma);
        if (sDebugOptions.mLightPresetSplitView) {
            glUniform1f(mPostPbrMaterial->getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
        }
        else {
            glUniform1f(mPostPbrMaterial->getUniform("unLightingSplit"), 1.0f);
        }
        if (const VGUniform* inputUniform = mPostPbrMaterial->mProgram.tryGetUniform("unCloudTexture")) {
            mGBuffers[0]->bindAlbedoTexture(textureUnit);
            glUniform1i(*inputUniform, textureUnit++);
        }
        if (const VGUniform* inputUniform = mPostPbrMaterial->mProgram.tryGetUniform("unDepthTexture")) {
            mGBuffers[0]->bindDepthTexture(textureUnit);
            glUniform1i(*inputUniform, textureUnit++);
        }
    }
    else {
        MaterialRenderer::bindMaterialShaderForRender(*mPostMaterial, &textureUnit);
        MaterialUtils::uploadLightingUniforms(*mPostMaterial);
        if (const VGUniform* inputUniform = mPostMaterial->mProgram.tryGetUniform("CloudFbo")) {
            mGBuffers[0]->bindAlbedoTexture(textureUnit);
            glUniform1i(*inputUniform, textureUnit++);
        }

    }

    vg::DepthState::NONE.set();
    sGlobalFullTriangleVAO.draw();
}
