#pragma once

constexpr float CHUNKS_LOAD_RANGE_MULT = 15.0f;

#ifdef USE_SMALL_CHUNK_WIDTH
constexpr float CHUNK_LOAD_RANGE = CHUNK_WIDTH * CHUNKS_LOAD_RANGE_MULT * 2.0f;
#else
constexpr float CHUNK_LOAD_RANGE = CHUNK_WIDTH * CHUNKS_LOAD_RANGE_MULT;
#endif

struct DebugOptions {
    f64 mTimeOffset = 0.0f;
    // Clouds
    bool mDisableClouds = true;// false;
    int mCloudBlurPasses = 3;
    float mCloudBlurRadius = 2.25f;
    float mCloudAmbient = 0.5f;
    float mCloudSpeed = 0.0f;//0.05f;
    // DOF
    float mDepthOfFieldBlurRadius = 0.6f;
    int mDepthOfFieldBlurPasses = 3;
    // Ambient occlusion
    bool mAmbientOcclusionDisabled = false;
    float mAmbientOcclusionRadius = 0.7f;
    float mAmbientOcclusionBias = 0.008f;
    float mAmbientOcclusionBlurRadius = 0.77f;
    int mAmbientOcclusionBlurPasses = 2;
    // Shadows
    float mShadowZMult = 6.0f;//2.50f;
    float mShadowNearSize = 17.0f;
    f32v3 mShadowColor = f32v3(204.0f / 255.0f, 230.0f / 255.0f, 243.0f / 255.0f);
    f32 mShadowUpdateRateSeconds = 0.022f;
    int mShadowBlurPasses = 2;
    float mShadowBlurRadius = 1.5f;
    bool mDisableShadows = false;
    // Toggles
    bool mPauseFrustum = false;
    bool mWireframe = false;
    bool mChunkBoundaries = false;
    bool mCities = false;
    bool mNavGraph = false;
    bool mShowTweaker = false;
    // Game settings
    f32 mLoadRangeSq = SQ(CHUNK_LOAD_RANGE);
    f32 mLoadRange = CHUNK_LOAD_RANGE;
    // Camera settings
    f32 mFoV = 75.0f;
    f32 mZFar = 200000.0f;

    // TODO: FILE CONFIG
    bool mUseCompressedAtlas = false;
    bool mVSYNC = false;
};

extern DebugOptions sDebugOptions;