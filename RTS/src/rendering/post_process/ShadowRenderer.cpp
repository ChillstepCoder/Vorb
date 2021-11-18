#include "stdafx.h"
#include "ShadowRenderer.h"

#include "camera/Camera3D.h"

#include "ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"

#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

//#include "DebugRenderer.h"

constexpr int DEPTH_MAP_RESOLUTION = 4096;// 8192;

// TODO: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-summed-area-variance-shadow-maps
// https://docs.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps

ShadowRenderer::ShadowRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims) :
    mResourceManager(resourceManager), mMaterialRenderer(materialRenderer), mGBufferDims(gbufferDims)
{

    // Shadowmap buffer
    glGenFramebuffers(1, &mShadowMapFBO);

    glGenTextures(1, &mShadowDepthMaps);
    glBindTexture(GL_TEXTURE_2D_ARRAY, mShadowDepthMaps);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        GL_DEPTH_COMPONENT32F,
        DEPTH_MAP_RESOLUTION,
        DEPTH_MAP_RESOLUTION,
        MAX_SHADOW_CASCADE_LEVELS + 1,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);

    glBindFramebuffer(GL_FRAMEBUFFER, mShadowMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mShadowDepthMaps, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!";
        throw 0;
    }
    checkGlError("Shadow FBO init");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    vg::GBufferAttachment attachment;
    // Color
    // TODO: SWAP CHAIN
    attachment.format = vg::TextureInternalFormat::RGB8;
    attachment.number = FBO_GEOMETRY_COLOR;
    attachment.pixelFormat = vg::TextureFormat::RGB;
    attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    mShadowApplyGBuffer.setSize(ui32v2(mGBufferDims));
    mShadowApplyGBuffer.init(attachment, nullptr, nullptr);

    // Materials
    mShadowMapperMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_mapper");
    mShadowApplyMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_apply");
}

void ShadowRenderer::beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld) {

    const float zNear = camera.getZNear();

    mPlaneDistances[0] = zNear + sDebugOptions.mShadowNearSize;
    mPlaneDistances[1] = mPlaneDistances[0] + 40.0f;
    mPlaneDistances[2] = mPlaneDistances[1] + 100.0f;
    mPlaneDistances[3] = mPlaneDistances[2] + 600.0f;
    const float zPlanes[MAX_SHADOW_CASCADE_LEVELS + 1] = {
        zNear,
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

        // Round to nearest texel (doesnt seem to work lol)
        const f32 f = (zPlanes[i + 1] - zPlanes[i]) / f32(DEPTH_MAP_RESOLUTION);
        center.x = glm::round(center.x / f) * f;
        center.y = glm::round(center.y / f) * f;

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
            const f32v4& v = mFrustumCornersWorldSpace[i];
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

        const f32m4 lightP = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
        mLightVP[i] = lightP * lightV;
    }
}

void ShadowRenderer::useShadowBuffer() {
    assert(mShadowMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mShadowMapFBO);
    glViewport(0, 0, DEPTH_MAP_RESOLUTION, DEPTH_MAP_RESOLUTION);
    glClear(GL_DEPTH_BUFFER_BIT);
}

vg::GBuffer* ShadowRenderer::renderShadows(vg::GBuffer* activeGBuffer) {
    assert(activeGBuffer);

    mShadowApplyGBuffer.useGeometry();

    mMaterialRenderer.bindMaterialForRender(*mShadowApplyMaterial);

    sGlobalFullQuadVBO.draw();

    // Share textures with previous gbuffer since this will become new active gbuffer
    mShadowApplyGBuffer.setDepthTexture(activeGBuffer->getDepthTexture());
    mShadowApplyGBuffer.setLightTexture(activeGBuffer->getLightTexture());
    mShadowApplyGBuffer.setNormalTexture(activeGBuffer->getNormalTexture());
    mShadowApplyGBuffer.setRoughnessTexture(activeGBuffer->getRoughnessTexture());
    mShadowApplyGBuffer.setFboLight(activeGBuffer->getFboLight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, activeGBuffer->getDepthTexture(), 0);

    return &mShadowApplyGBuffer;
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