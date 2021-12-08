#pragma once

class Camera3D;
class ResourceManager;
class MaterialRenderer;
class Material;

#include <Vorb/graphics/GBuffer.h>

constexpr int MAX_SHADOW_CASCADE_LEVELS = 4;
constexpr int SHADOW_FRUSTUM_CORNER_COUNT = 8;

// Cascading shadow maps
// https://learnopengl.com/Guest-Articles/2021/CSM
class ShadowRenderer
{
public:
    ShadowRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims);

    void beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld);

    void useShadowBuffer();

    vg::GBuffer* renderShadows(vg::GBuffer* activeGBuffer, const f32v3& cameraPos);

    const f32m4* getShadowFrustumMatrices() const { return mLightVP; }
    const f32* getShadowCascadePlaneDistances() const { return mPlaneDistances; }
    const VGTexture getShadowMap() const { return mShadowMapGBuffer.getGeometryTexture(); }
    const f32 getMaxDistance() const;
    const f32v3& getLastUpdatedSunPosition() const { return mLastUpdatedSunPosition; }

    bool shouldUpdateShadowsThisFrame() const { return mShouldUpdateShadowsThisFrame; }

private:
    void updateFrustumCorners(const f32m4& projection, const f32m4& view);
    void generateMipmaps();
    void blurShadowMap();

    ResourceManager& mResourceManager;
    const MaterialRenderer& mMaterialRenderer;
    const Material* mShadowMapperMaterial = nullptr;
    const Material* mShadowVarianceMaterial = nullptr;
    const Material* mShadowApplyMaterial = nullptr;
    const Material* mShadowMipMaterial = nullptr;
    const Material* mBlurMaterial = nullptr;
    const Material* mShadowFinalMaterial = nullptr;
    vg::GBuffer mShadowMipGBuffer; // TODO: Can we combine this with the blur gbuffer?
    vg::GBuffer mShadowBlurGBuffers[2];
    vg::GBuffer mShadowMapGBuffer;
    vg::GBuffer mShadowMapApplyGBuffer;
    f32 mLastCamZAngle = 0.0f;
    f32 mLastCamZNear = 0.0f;
    f32v3 mLastCameraPos = f32v3(0.0f);
    f32v3 mLastUpdatedCameraPos = f32v3(0.0f);
    f32v3 mLastSunPosition = f32v3(0.0f);
    f32v3 mLastUpdatedSunPosition = f32v3(0.0f);
    f32v2 mGBufferDims;
    f32v4 mFrustumCornersWorldSpace[SHADOW_FRUSTUM_CORNER_COUNT];
    f32m4 mLightVP[MAX_SHADOW_CASCADE_LEVELS];
    f32 mPlaneDistances[MAX_SHADOW_CASCADE_LEVELS];
    f32 mLastUpdateTime = 0.0f;
    bool mShouldUpdateShadowsThisFrame = true;
};

