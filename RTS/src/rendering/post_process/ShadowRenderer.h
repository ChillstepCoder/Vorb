#pragma once

class Camera3D;
class MaterialShaderDef;

#include "rendering/post_process/ShadowDetail.h"
#include "ShadowPassShaderData.h"
#include "resources/asset/AssetHandleBundle.h"

DECL_VG(class GBuffer);

constexpr int SHADOW_FRUSTUM_CORNER_COUNT = 8;


// Cascading shadow maps
// https://learnopengl.com/Guest-Articles/2021/CSM
// TODO: read https://alextardif.com/shadowmapping.html
// TODO: read http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/#light-space-perspective-shadow-maps
class ShadowRenderer
{
public:
    ShadowRenderer(const ui32v2& gbufferDims);

    void beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld);

    void useShadowBuffer();
    void clearShadowTexture();

    void renderShadows(const f32v3 cameraPos);

    const f32m4* getShadowFrustumMatrices() const { return mLightVP; }
    const f32* getShadowCascadePlaneDistances() const { return mPlaneDistances; }
    const VGTexture getShadowMap() const;
    const f32 getMaxDistance(ShadowDetail detail) const;
    const f32v3& getLastUpdatedSunPosition() const { return mLastUpdatedSunPosition; }

    bool shouldUpdateShadowsThisFrame() const { return mShouldUpdateShadowsThisFrame; }
    VGTexture getShadowTexture() const;

    const f32* getShadowPlaneDistances() const { return mPlaneDistances; }
    const ShadowPassShaderData& getShaderData() const { return mShaderData; }

private:
    void updateFrustumCorners(const f32m4& projection, const f32m4& view);
    void generateMipmaps();
    void blurShadowMap();

    const MaterialShaderDef* mShadowMapperMaterial = nullptr;
    const MaterialShaderDef* mShadowVarianceMaterial = nullptr;
    const MaterialShaderDef* mShadowApplyMaterial = nullptr;
    const MaterialShaderDef* mShadowMipMaterial = nullptr;
    const MaterialShaderDef* mBlurMaterial = nullptr;
    AssetHandleBundle mShaderAssets;

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

