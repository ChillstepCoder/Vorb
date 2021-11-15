#include "stdafx.h"
#include "ShadowRenderer.h"

#include "camera/Camera3D.h"

constexpr int DEPTH_MAP_RESOLUTION = 2048;
constexpr int SHADOW_CASCADE_LEVELS = 3;

ShadowRenderer::ShadowRenderer() {
    glGenFramebuffers(1, &mLightFBO);

    glGenTextures(1, &mLightDepthMaps);
    glBindTexture(GL_TEXTURE_2D_ARRAY, mLightDepthMaps);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        GL_DEPTH_COMPONENT32F,
        DEPTH_MAP_RESOLUTION,
        DEPTH_MAP_RESOLUTION,
        SHADOW_CASCADE_LEVELS + 1,
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

    glBindFramebuffer(GL_FRAMEBUFFER, mLightFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mLightDepthMaps, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!";
        throw 0;
    }
    checkGlError("Shadow FBO init");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowRenderer::beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld) {
    // Average to the center of the frustum so we can look at it with the light source
    const f32v4* cornersWorldSpace = camera.getFrustum().getFrustumCornersWorldSpace();
    f32v3 center(0);
    for (int i = 0; i < FRUSTUM_CORNER_COUNT; ++i) {
        center += f32v3(cornersWorldSpace[i]);
    }
    center /= FRUSTUM_CORNER_COUNT;

    // View matrix
    mLightV = glm::lookAt(
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
    for (int i = 0; i < FRUSTUM_CORNER_COUNT; ++i)
    {
        const f32v4& v = cornersWorldSpace[i];
        const auto trf = mLightV * v;
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // Tune this parameter according to the scene
    constexpr float zMult = 10.0f;
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

    mLightP = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    mLightVP = mLightP * mLightV;
}
