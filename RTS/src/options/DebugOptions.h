#pragma once

struct DebugOptions {
    f64 mTimeOffset = 0.0f;
    int mCloudBlurPasses = 3;
    float mCloudBlurRadius = 2.25f;
    float mCloudAmbient = 0.5f;
    float mDepthOfFieldBlurRadius = 0.6f;
    int mDepthOfFieldBlurPasses = 3;
    float mShadowZMult = 2.50f;
    float mShadowNearSize = 17.0f;
    f32v3 mShadowColor = f32v3(186.0f / 255.0f, 197.0f / 255.0f, 202.0f / 255.0f);
    bool mDisableShadows = false;
    bool mPauseFrustum = false;
    bool mWireframe = false;
    bool mChunkBoundaries = false;
    bool mCities = false;
    bool mNavGraph = false;
    bool mShowTweaker = false;
    f32 mLoadRangeSq;
    f32 mLoadRange;
};

extern DebugOptions sDebugOptions;