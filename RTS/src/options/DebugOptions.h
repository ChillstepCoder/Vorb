#pragma once

struct DebugOptions {
    f64 mTimeOffset = 0.0f;
    bool mWireframe = false;
    bool mChunkBoundaries = false;
    bool mCities = false;
    bool mNavGraph = false;
    int mCloudBlurPasses = 3;
    float mCloudBlurRadius = 2.25f;
    float mCloudAmbient = 0.5f;
    float mDepthOfFieldBlurRadius = 0.6f;
    int mDepthOfFieldBlurPasses = 3;
    float mShadowZMult = 2.50f;
    float mShadowNearSize = 17.0f;
    bool mShowTweaker = false;
};

extern DebugOptions sDebugOptions;