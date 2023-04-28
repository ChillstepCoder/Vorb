#pragma once

class Camera3D;
class MaterialShader;

#include "rendering/post_process/ShadowLodDetail.h"
#include "ShadowPassShaderData.h"

DECL_VG(class GBuffer);

constexpr int SHADOW_FRUSTUM_CORNER_COUNT = 8;


// Cascading shadow maps
// https://learnopengl.com/Guest-Articles/2021/CSM
class ShadowRenderer
{
public:
    ShadowRenderer(const ui32v2& gbufferDims);

    void beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld);

    void useShadowBuffer();
    void clearShadowTexture();

    void renderShadows(const f32v3& cameraPos);

    const f32m4* getShadowFrustumMatrices() const { return mLightVP; }
    const f32* getShadowCascadePlaneDistances() const { return mPlaneDistances; }
    const VGTexture getShadowMap() const;
    const f32 getMaxDistance(ShadowLodDetail detail) const;
    const f32v3& getLastUpdatedSunPosition() const { return mLastUpdatedSunPosition; }

    bool shouldUpdateShadowsThisFrame() const { return mShouldUpdateShadowsThisFrame; }
    VGTexture getShadowTexture() const;

    const f32* getShadowPlaneDistances() const { return mPlaneDistances; }
    const ShadowPassShaderData& getShaderData() const { return mShaderData; }

private:
    void updateFrustumCorners(const f32m4& projection, const f32m4& view);
    void generateMipmaps();
    void blurShadowMap();

    const MaterialShader* mShadowMapperMaterial = nullptr;
    const MaterialShader* mShadowVarianceMaterial = nullptr;
    const MaterialShader* mShadowApplyMaterial = nullptr;
    const MaterialShader* mShadowMipMaterial = nullptr;
    const MaterialShader* mBlurMaterial = nullptr;
    std::unique_ptr<vg::GBuffer> mShadowMipGBuffer; // TODO: Can we combine this with the blur gbuffer?
    std::unique_ptr<vg::GBuffer> mShadowBlurGBuffers[2];
    std::unique_ptr<vg::GBuffer> mShadowMapGBuffer;
    f32 mLastCamZAngle = 0.0f;
    f32 mLastCamZNear = 0.0f;
    f32v3 mLastCameraPos = f32v3(0.0f);
    f32v3 mLastUpdatedCameraPos = f32v3(0.0f);
    f32v3 mLastSunPosition = f32v3(0.0f);
    f32v3 mLastUpdatedSunPosition = f32v3(0.0f);
    f32v4 mFrustumCornersWorldSpace[SHADOW_FRUSTUM_CORNER_COUNT];
    f32m4 mLightVP[MAX_SHADOW_CASCADE_LEVELS];
    f32 mPlaneDistances[MAX_SHADOW_CASCADE_LEVELS];
    f32 mLastUpdateTime = 0.0f;
    bool mShouldUpdateShadowsThisFrame = true;

    ShadowPassShaderData mShaderData;
};

