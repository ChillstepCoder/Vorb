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
    DebugOptions();
    f64 mTimeOffset;
    // Clouds
    bool mDebugClouds;
    bool mDisableClouds;
    int mCloudBlurPasses;
    float mCloudBlurRadius;
    float mCloudAmbient;
    float mCloudSpeed;
    float mCloudMetallic;
    float mCloudRoughness;
    // Grass
    QuadtreeSettings mGrassSettings;
    bool mDebugGrassLod;
    bool mHideGrass;
    // Structures
    float mWallWoobleChance;
    float mWallWoobleIntensity;
    bool mStructureDebug;
    // Terrain
    f32 mTerrainLodDistanceOffset; // 1500 for ultra
    bool mDebugTerrainLod;
    f32 mTerrainHeightColorMult;
    f32 mTerrainWavyColorMult;
    f32 mTerrainSquaresColorPeriod;
    f32 mTerrainSquaresIntensity;
    f32 mTerrainBlendMult;
    bool mDisableTerrain;
    // DOF
    float mDepthOfFieldBlurRadius;
    int mDepthOfFieldBlurPasses;
    f32v2 mDepthOfFieldRangeNear;
    f32v2 mDepthOfFieldRangeFar;
    float mDepthOfFieldExponent;
    bool mDepthOfFieldDebugRender;
    // Ambient occlusion
    bool mSSAODisabled;
    float mSSAORadius;
    float mSSAOBias;
    float mSSAOBlurRadius;
    int mSSAOBlurPasses;
    float mSSAORangeCheckMult;
    f32v3 mSSAOColor;
    // Shadows
    float mShadowZMult;
    float mShadowNearSize;
    f32v3 mShadowColor;
    f32 mShadowUpdateRateSeconds;
    int mShadowBlurPasses; // 2
    float mShadowBlurRadius;
    bool mDisableShadows;
    // Smudge
    int mSmudgeTestPasses;
    f32 mSmudgeTestRadius;
    f32 mSmudgeTestNormThreshold;
    f32 mSmudgeTestDepthThreshold;
    bool mSmudgeTestShowVariance;
    bool mSmudgeTestShowEdges;
    bool mSmudgeTestDisable;
    // Toggles
    bool mPauseFrustum;
    bool mWireframe;
    std::atomic_bool mChunkBoundaries;
    bool mCities;
    bool mRoofDebug;
    bool mShowNavGraph;
    bool mShowNavGraphUpdates;
    bool mHideCharacters;
    bool mShowTerrainPhysics;
    bool mShowStaticPhysics;
    bool mShowDynamicPhysics;
    bool mShowPhysicsActions;
    bool mShowBusinessDebug;
    bool mShowEditor;
    bool mShowPaths;
    bool mShowEntityQueries;
    bool mEnableVisualLogs;
    bool mShowDevHud;
    bool mHideModels;
    bool mDisableLOD;
    bool mDisableGPUCulling;
    // Water
    f32v4 mShallowWaterColor;
    f32v4 mDeepWaterColor;
    f32v4 mWaterFoamColor;
    f32 mWaterSurfaceDistortAmount;
    f32 mWaterSurfaceMoveSpeed;
    f32v2 mWaterFoamDistanceRange;
    f32 mWaterSurfaceNoiseCutoff;
    f32 mWaterSmoothstepAA;
    f32 mWaterColorNoiseIntensity;
    f32 mWaterDistortTiling;
    f32 mWaterNoiseTiling;
    f32 mWaterMetallic;
    f32 mWaterRoughness;
    bool mDisableWater;
    // Lighting
    bool mUsingPBR;
    LightingOptions* mLightingOptions;
    LightingOptions* mLightingOptionsSplit;
    int mLightingPreset;
    int mLightingPresetSplit;
    bool mLightPresetSplitView;
    float mLightPresetSplitAmount;
    f32 unUchMaxDisplayBrightness;
    f32 unUchContrast;
    f32 unUchLinearSectionStart;
    f32 unUchLinearSectionLength;
    f32 unUchBlack;
    f32 unUchPedestal;
    f32v3 mSunColorPeak;
    f32v3 mSunColorSunset;
    // Game settings
    f32 mLoadRangeSq;
    f32 mLoadRange;
    f32 mLodDistances[3];
    // Camera settings
    f32 mFoV;
    f32 mZFar;
    f32 mCameraZHeight;
    f32 mCameraXYDistance;
    CameraMode mCameraMode;
    // Shader debug
    f32v3 mDebugColor01;
    f32v3 mDebugColor02;
    f32 mDebugFloat01;
    f32 mDebugFloat02;
    f32 mDebugFloat03;
    f32 mDebugFloat04;

    // TODO: FILE CONFIG
    bool mUseCompressedAtlas;
    bool mVSYNC;
    // TODO: somewhere else?
    f32v2 mScreenResolution; // Currently set in  App::onInit
};

extern DebugOptions sDebugOptions;
