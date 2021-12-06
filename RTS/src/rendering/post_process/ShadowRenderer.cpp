#include "stdafx.h"
#include "ShadowRenderer.h"

#include "camera/Camera3D.h"

#include "ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"

#include <Vorb/graphics/FullQuadVBO.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/BlendState.h>

#include "options/DebugOptions.h"

//#include "DebugRenderer.h"

constexpr int DEPTH_MAP_RESOLUTION = 4096;


constexpr int MIP_LEVELS = 4; // TODO: Make this dynamic

// TODO: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-summed-area-variance-shadow-maps
// https://docs.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps

ShadowRenderer::ShadowRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims) :
    mResourceManager(resourceManager), mMaterialRenderer(materialRenderer), mGBufferDims(gbufferDims)
{

    {// Shadow map gbuffers
        vg::GBufferAttachment attachment;
        // Color
        attachment.format = vg::TextureInternalFormat::RG32F;
        attachment.number = FBO_GEOMETRY_COLOR;
        attachment.pixelFormat = vg::TextureFormat::RG;
        attachment.pixelType = vg::TexturePixelType::FLOAT;

        mShadowMapGBuffer.setSize(ui32v2(DEPTH_MAP_RESOLUTION));
        mShadowMapGBuffer.init(attachment, nullptr, nullptr, vg::TextureInternalFormat::NONE, MAX_SHADOW_CASCADE_LEVELS + 1);
        mShadowMapGBuffer.bindGeometryTexture(0, GL_TEXTURE_2D_ARRAY);
        constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);
        vg::SamplerState::LINEAR_CLAMP.set(GL_TEXTURE_2D_ARRAY);

        mShadowMapGBuffer.initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT32, MAX_SHADOW_CASCADE_LEVELS + 1);
        checkGlError("Shadow FBO init");
    }

    {// Shadow mip gbuffer
        //vg::GBufferAttachment attachment;
        //// Color
        //attachment.format = vg::TextureInternalFormat::RG8;
        //attachment.number = FBO_GEOMETRY_COLOR;
        //attachment.pixelFormat = vg::TextureFormat::RG;
        //attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
        //mShadowMipGBuffer.setSize(ui32v2(mGBufferDims));
        //mShadowMipGBuffer.init(attachment, nullptr, nullptr);
        //mShadowMipGBuffer.initMipLevelsGeom(attachment, MIP_LEVELS/*TODO: dynamic*/);

        //checkGlError("Shadow mips init");
    }

    {// Shadow blur gbuffer
        vg::GBufferAttachment attachment;
        // Color
        attachment.format = vg::TextureInternalFormat::RG8;
        attachment.number = FBO_GEOMETRY_COLOR;
        attachment.pixelFormat = vg::TextureFormat::RG;
        attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
        for (int i = 0; i < 2; ++i) {
            mShadowBlurGBuffers[i].setSize(ui32v2(mGBufferDims));
            mShadowBlurGBuffers[i].init(attachment, nullptr, nullptr);
        }

        checkGlError("Shadow FBO 2 init");
    }

    { // Shadow apply gbuffer
        vg::GBufferAttachment attachment;
        // Color
        attachment.format = vg::TextureInternalFormat::RGB8;
        attachment.number = FBO_GEOMETRY_COLOR;
        attachment.pixelFormat = vg::TextureFormat::RGB;
        attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
        mShadowMapApplyGBuffer.setSize(ui32v2(mGBufferDims));
        mShadowMapApplyGBuffer.init(attachment, nullptr, nullptr);

        checkGlError("Shadow FBO 2 init");
    }

    // Materials
    mShadowMapperMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_mapper");
    mShadowVarianceMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_variance");
    mShadowApplyMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_apply");
    mBlurMaterial = mResourceManager.getMaterialManager().getMaterial("gaussian_blur_shadows");
    mShadowMipMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_mipmap");

}

constexpr f32 SUN_POSITION_UPDATE_THRESH_SQ = SQ(0.001f);
constexpr f32 CAMERA_POSITION_UPDATE_THRESH_SQ = SQ(1.0f);

void ShadowRenderer::beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld) {

    f32 camZNear = camera.getZNear();
    f32 camZAngle = camera.getZAngle();
    {   // Check if we need to update this frame based on camera motion and time
        mLastCameraPos = camera.getPosition();
        mLastSunPosition = sunPositionWorld;

        if (glm::distance2(sunPositionWorld, mLastUpdatedSunPosition) < SUN_POSITION_UPDATE_THRESH_SQ &&
            glm::distance2(camera.getPosition(), mLastUpdatedCameraPos) < CAMERA_POSITION_UPDATE_THRESH_SQ &&
            camZAngle == mLastCamZAngle &&
            camZNear == mLastCamZNear &&
            sTotalTimeSeconds - mLastUpdateTime < sDebugOptions.mShadowUpdateRateSeconds) {
            mShouldUpdateShadowsThisFrame = false;
            return;
        }
        mLastUpdatedCameraPos = mLastCameraPos;
        mLastUpdatedSunPosition = mLastSunPosition;
        mLastCamZAngle = camZAngle;
        mLastCamZNear = camZNear;
        mLastUpdateTime = sTotalTimeSeconds;
        mShouldUpdateShadowsThisFrame = true;
    }

    mPlaneDistances[0] = camZNear + sDebugOptions.mShadowNearSize;
    mPlaneDistances[1] = mPlaneDistances[0] + 40.0f;
    mPlaneDistances[2] = mPlaneDistances[1] + 100.0f;
    mPlaneDistances[3] = mPlaneDistances[2] + 600.0f;
    const float zPlanes[MAX_SHADOW_CASCADE_LEVELS + 1] = {
        camZNear,
        mPlaneDistances[0],
        mPlaneDistances[1],
        mPlaneDistances[2],
        mPlaneDistances[3]
    };

    for (int i = 0; i < MAX_SHADOW_CASCADE_LEVELS; ++i) {
        const f32m4 proj = glm::perspective(
            glm::radians(camera.getFieldOfView()),
            camera.getAspectRatio(),
            zPlanes[i],
            zPlanes[i + 1]
        );
        updateFrustumCorners(proj, camera.getViewMatrix());

        f32v3 center(0);
        for (int i = 0; i < SHADOW_FRUSTUM_CORNER_COUNT; ++i) {
            center += f32v3(mFrustumCornersWorldSpace[i]);
        }
        center /= SHADOW_FRUSTUM_CORNER_COUNT;

        // View matrix
        f32m4 lightV = glm::lookAt(
            center + sunPositionWorld,
            center,
            f32v3(0.0f, 1.0f, 0.0f)
        );

        // Ortho projection matrix
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::min();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::min();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::min();
        for (int i = 0; i < SHADOW_FRUSTUM_CORNER_COUNT; ++i)
        {
            const f32v4 v = mFrustumCornersWorldSpace[i];
            const auto trf = lightV * v;
            minX = std::min(minX, trf.x);
            maxX = std::max(maxX, trf.x);
            minY = std::min(minY, trf.y);
            maxY = std::max(maxY, trf.y);
            minZ = std::min(minZ, trf.z);
            maxZ = std::max(maxZ, trf.z);
        }

        // Tune this parameter according to the scene
        const float zMult = sDebugOptions.mShadowZMult;
        if (minZ < 0) {
            minZ *= zMult;
        }
        else {
            minZ /= zMult;
        }
        if (maxZ < 0) {
            maxZ /= zMult;
        }
        else {
            maxZ *= zMult;
        }

        // Quantize positions to texel sizes to reduce flicker
        f32 xSpan = maxX - minX;
        f32 ySpan = maxY - minY;
        f32 xTexelSize = xSpan / DEPTH_MAP_RESOLUTION;
        f32 yTexelSize = ySpan / DEPTH_MAP_RESOLUTION;

        f32 offsetX = ceilf(camera.getPosition().x / xTexelSize);
        f32 offsetY = ceilf(camera.getPosition().y / yTexelSize);

        offsetX = (camera.getPosition().x / xTexelSize - offsetX) * xTexelSize;
        offsetY = (camera.getPosition().y / yTexelSize - offsetY) * yTexelSize;

        /*minX += camera.getPosition().x;
        minY += camera.getPosition().y;
        maxX += camera.getPosition().x;
        maxY += camera.getPosition().y;*/

        // Quantize scale (reduce flicker)
        //f32 scaleX = 2.0f / (maxX - minX);
        //f32 scaleY = 2.0f / (maxY - minY);
        //f32 scaleQuantizer = sDebugOptions.mShadowScaleQuantizer;
        //scaleX = 1.0f / ceilf(1.0f / scaleX * scaleQuantizer) * scaleQuantizer;
        //scaleY = 1.0f / ceilf(1.0f / scaleY * scaleQuantizer) * scaleQuantizer;

        //float offsetX = -0.5f * (maxX + minX) * scaleX; // Offset value for x dimension
        //float offsetY = -0.5f * (maxY + minY) * scaleY; // Offset value for y dimension

        //float halfTextureSize = 0.5f * DEPTH_MAP_RESOLUTION;
        //offsetX = ceilf(offsetX) / halfTextureSize;
        //offsetY = ceilf(offsetY) / halfTextureSize;/*
        /*minX -= camera.getPosition().x;
        maxX -= camera.getPosition().x;
        minY -= camera.getPosition().y;
        maxY -= camera.getPosition().y;
        minZ -= camera.getPosition().z;
        maxZ -= camera.getPosition().z;*/

        const f32m4 lightP = glm::ortho(minX - offsetX, maxX - offsetX, minY - offsetY, maxY - offsetY, minZ, maxZ);
        mLightVP[i] = lightP * lightV;
    }
}

void ShadowRenderer::useShadowBuffer() {
    assert(mShadowMapGBuffer.getFboGeometry());
    glBindFramebuffer(GL_FRAMEBUFFER, mShadowMapGBuffer.getFboGeometry());
    glViewport(0, 0, DEPTH_MAP_RESOLUTION, DEPTH_MAP_RESOLUTION);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

vg::GBuffer* ShadowRenderer::renderShadows(vg::GBuffer* activeGBuffer, const f32v3& cameraPos) {

    assert(activeGBuffer);

    f32v3 offset = cameraPos - mLastUpdatedCameraPos;

    mShadowBlurGBuffers[0].useGeometry();
    mMaterialRenderer.bindMaterialForRender(*mShadowVarianceMaterial);

    glUniform3fv(glGetUniformLocation(mShadowVarianceMaterial->mProgram.getID(), "CameraOffset"), 1, &offset[0]);

    sGlobalFullQuadVBO.draw();

    blurShadowMap();

    // Apply shadows
    mShadowMapApplyGBuffer.useGeometry();
    ui32 nextTextureIndex = 0;
    mMaterialRenderer.bindMaterialForRender(*mShadowApplyMaterial, &nextTextureIndex);

    mShadowBlurGBuffers[0].bindGeometryTexture(nextTextureIndex, GL_TEXTURE_2D);
    glUniform1i(glGetUniformLocation(mShadowApplyMaterial->mProgram.getID(), "unShadowFbo"), nextTextureIndex);

    sGlobalFullQuadVBO.draw();

    // Share textures with previous gbuffer since this will become new active gbuffer
    mShadowMapApplyGBuffer.setDepthTexture(activeGBuffer->getDepthTexture());
    mShadowMapApplyGBuffer.setLightTexture(activeGBuffer->getLightTexture());
    mShadowMapApplyGBuffer.setNormalTexture(activeGBuffer->getNormalTexture());
    mShadowMapApplyGBuffer.setRoughnessTexture(activeGBuffer->getRoughnessTexture());
    mShadowMapApplyGBuffer.setFboLight(activeGBuffer->getFboLight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, activeGBuffer->getDepthTexture(), 0);

    return &mShadowMapApplyGBuffer;
}

const f32 ShadowRenderer::getMaxDistance() const {
    return mPlaneDistances[MAX_SHADOW_CASCADE_LEVELS-1];
}

void ShadowRenderer::updateFrustumCorners(const f32m4& projection, const f32m4& view) {
    const auto inv = glm::inverse(projection * view);
    unsigned i = 0;
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                const glm::vec4 pt =
                    inv * glm::vec4(
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        2.0f * z - 1.0f,
                        1.0f);
                mFrustumCornersWorldSpace[i++] = pt / pt.w;
            }
        }
    }
}

void ShadowRenderer::generateMipmaps() {

    ui32 nextTextureIndex = 0;

    mMaterialRenderer.bindMaterialForRender(*mShadowMipMaterial, &nextTextureIndex);
    VGUniform inputUniform = glGetUniformLocation(mShadowMipMaterial->mProgram.getID(), "unInputTexture");

    for (int i = 0; i < MIP_LEVELS; ++i) {

        sGlobalFullQuadVBO.draw();
    }

}

void ShadowRenderer::blurShadowMap()
{
    ui32 nextTexture = 0;
    mMaterialRenderer.bindMaterialForRender(*mBlurMaterial, &nextTexture);

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    const VGUniform& fboUniform = mBlurMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mBlurMaterial->mProgram.getUniform("unDirection");
    for (int i = 0; i < sDebugOptions.mShadowBlurPasses; ++i) {

        // Horizontal
        mShadowBlurGBuffers[0].bindGeometryTexture(nextTexture, GL_TEXTURE_2D);
        mShadowBlurGBuffers[1].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, sDebugOptions.mShadowBlurRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        mShadowBlurGBuffers[1].bindGeometryTexture(nextTexture, GL_TEXTURE_2D);
        mShadowBlurGBuffers[0].useGeometry();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mShadowBlurRadius);
        sGlobalFullQuadVBO.draw();
    }
}
