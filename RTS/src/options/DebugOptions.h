#pragma once

#include "data_structure/QuadtreeSettings.h"
#include "camera/CameraMode.h"

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
    bool mDisableClouds = true;
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
    // DOF
    float mDepthOfFieldBlurRadius = 0.6f;
    int mDepthOfFieldBlurPasses = 3;
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
    bool mRoofDebug = false;
    bool mShowNavGraph = false;
    bool mShowNavGraphUpdates = false;
    bool mHideCharacters = false;
    bool mShowPhysicsDebug = false;
    bool mShowBusinessDebug = true;
    bool mShowTweaker = false;
    bool mShowEditor = false;
    bool mShowPaths = true;
    bool mShowEntityQueries = false;
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

    // TODO: FILE CONFIG
    bool mUseCompressedAtlas = false;
    bool mVSYNC = true;
    // TODO: somewhere else?
    f32v2 mScreenResolution = f32v2(1600.0f, 900.0f); // Currently set in  App::onInit
    f32v3 mMousePickRay = f32v3(0.0f);
};

extern DebugOptions sDebugOptions;