#pragma once

#include "data_structure/QuadtreeSettings.h"
#include "camera/CameraMode.h"

#include "LightingOptions.h"

constexpr float CHUNKS_LOAD_RANGE_MULT = 15.0f;

#ifdef USE_SMALL_CHUNK_WIDTH
constexpr float CHUNK_LOAD_RANGE = CHUNK_WIDTH * CHUNKS_LOAD_RANGE_MULT * 2.0f;
#else
constexpr float CHUNK_LOAD_RANGE = CHUNK_WIDTH * CHUNKS_LOAD_RANGE_MULT;
#endif

constexpr f32 GRASS_FADE_MULT = 5.55555f;
constexpr f32 DEFAULT_GRASS_DISTANCE = 220.0f;
constexpr f32 DEFAULT_TERRAIN_DISTANCE = 220.0f;


struct DebugOptions {
    f64 mTimeOffset = 0.0f;
    // Clouds
    bool mDisableClouds = false;
    int mCloudBlurPasses = 3;
    float mCloudBlurRadius = 1.25f;
    float mCloudAmbient = 0.5f;
    float mCloudSpeed = 0.05f;
    // Grass
    QuadtreeSettings mGrassSettings = { DEFAULT_GRASS_DISTANCE, SQ(DEFAULT_GRASS_DISTANCE), DEFAULT_GRASS_DISTANCE * GRASS_FADE_MULT, 50.0f };
    bool mDebugGrassLod = false;
    bool mHideGrass = true;
    // Terrain
    f32 mTerrainLodDistanceOffset = 1500.0f;
    bool mDebugTerrainLod = false;
    f32 mTerrainHeightColorMult = 0.22f;
    f32 mTerrainWavyColorMult = 0.167f;
    f32 mTerrainSquaresColorPeriod = 0.187f;
    f32 mTerrainSquaresIntensity = 0.0f;
    f32 mTerrainBlendMult = 0.037f;
    bool mDisableTerrain = false;
    // DOF
    float mDepthOfFieldBlurRadius = 0.6f;
    int mDepthOfFieldBlurPasses = 1;
    f32v2 mDepthOfFieldRangeNear = f32v2(0.0f, 2.0f);
    f32v2 mDepthOfFieldRangeFar = f32v2(10.0f, 1000.0f);
    float mDepthOfFieldExponent = 1.0f;
    bool mDepthOfFieldDebugRender = false;
    // Ambient occlusion
    bool mSSAODisabled = true;
    float mSSAORadius = 1.0f;
    float mSSAOBias = 0.008f;
    float mSSAOBlurRadius = 0.77f;
    int mSSAOBlurPasses = 2;
    float mSSAORangeCheckMult = 0.5f;
    f32v3 mSSAOColor = f32v3(14.0f / 255.0f, 0.0f / 255.0f, 25.0f / 255.0f);
    // Shadows
    float mShadowZMult = 6.0f;//2.50f;
    float mShadowNearSize = 17.0f;
    f32v3 mShadowColor = f32v3(204.0f / 255.0f, 230.0f / 255.0f, 243.0f / 255.0f);
    f32 mShadowUpdateRateSeconds = 0.022f;
    int mShadowBlurPasses = 2;
    float mShadowBlurRadius = 0.45f; //1.5f;
    bool mDisableShadows = true;
    // Toggles
    bool mPauseFrustum = false;
    bool mWireframe = false;
    bool mChunkBoundaries = false;
    bool mCities = false;
    bool mRoofDebug = true;
    bool mShowNavGraph = false;
    bool mShowNavGraphUpdates = false;
    bool mHideCharacters = false;
    bool mShowPhysicsDebug = false;
    bool mShowBusinessDebug = true;
    bool mShowTweaker = false;
    bool mShowEditor = false;
    bool mShowPaths = true;
    bool mShowEntityQueries = false;
    // Water
    f32v4 mShallowWaterColor = f32v4(159.0f / 255.0f, 194.0f / 255.0f, 206.0f / 255.0f, 185.0f / 255.0f);
    f32v4 mDeepWaterColor = f32v4(57.0f / 255.0f, 83.0f / 255.0f, 122.0f / 255.0f, 191.0f / 255.0f);
    f32v4 mWaterFoamColor = f32v4(111.0f / 255.0f, 148.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f);
    f32 mWaterSurfaceDistortAmount = 0.27f;
    f32 mWaterSurfaceMoveSpeed = 0.03f;
    f32v2 mWaterFoamDistanceRange = f32v2(0.4f, 1.0f);
    f32 mWaterSurfaceNoiseCutoff = 0.777;
    f32 mWaterSmoothstepAA = 0.037f;
    f32 mWaterColorNoiseIntensity = 0.085f;
    f32 mWaterDistortTiling = 1.0f;
    f32 mWaterNoiseTiling = 1.0f;
    bool mDisableWater = false;
    // Lighting
    LightingOptions* mLightingOptions = &sLightingPresets[LIGHT_PRESET_UCHIMURA];
    LightingOptions* mLightingOptionsSplit = &sLightingPresets[LIGHT_PRESET_CUSTOM];
    int mLightingPreset = LIGHT_PRESET_UCHIMURA;
    int mLightingPresetSplit = LIGHT_PRESET_CUSTOM;
    bool mLightPresetSplitView = false;
    float mLightPresetSplitAmount = 0.43f;
    f32 unUchMaxDisplayBrightness = 1.1;
    f32 unUchContrast = 0.7;
    f32 unUchLinearSectionStart = 0.06;
    f32 unUchLinearSectionLength = 0.6;
    f32 unUchBlack = 1.33;
    f32 unUchPedestal = 0.0;
    // Game settings
    f32 mLoadRangeSq = SQ(CHUNK_LOAD_RANGE);
    f32 mLoadRange = CHUNK_LOAD_RANGE;
    // Camera settings
    f32 mFoV = 75.0f;
    f32 mZFar = 200000.0f;
    f32 mCameraZHeight = 1.0f;
    f32 mCameraXYDistance = 1.0f;
    CameraMode mCameraMode = CameraMode::MMO;
    // Shader debug
    f32v3 mDebugColor01 = f32v3(0.0f, 0.0f, 1.0f);
    f32v3 mDebugColor02 = f32v3(0.0f, 1.0f, 0.0f);
    f32 mDebugFloat01 = 0.0f;
    f32 mDebugFloat02 = 0.0f;
    f32 mDebugFloat03 = 0.0f;
    f32 mDebugFloat04 = 0.0f;

    // TODO: FILE CONFIG
    bool mUseCompressedAtlas = false;
    bool mVSYNC = true;
    // TODO: somewhere else?
    f32v2 mScreenResolution = f32v2(1600.0f, 900.0f); // Currently set in  App::onInit
    f32v3 mMousePickRay = f32v3(0.0f);
};

extern DebugOptions sDebugOptions;