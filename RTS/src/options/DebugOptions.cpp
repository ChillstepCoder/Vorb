#include "stdafx.h"
#include "DebugOptions.h"

DebugOptions sDebugOptions;

// Lower for faster loading in test
constexpr f32 CHUNK_LOAD_RANGE_MULT = 0.15f;

DebugOptions::DebugOptions() :
    mTimeOffset(0.0f),
    // Clouds
    mDebugClouds(false),
    mDisableClouds(false),
    mCloudBlurPasses(3),
    mCloudBlurRadius(1.25f),
    mCloudAmbient(0.5f),
    mCloudSpeed(0.05f),
    // Grass
    mGrassSettings{ DEFAULT_GRASS_DISTANCE, SQ(DEFAULT_GRASS_DISTANCE), DEFAULT_GRASS_DISTANCE * GRASS_FADE_MULT, 50.0f },
    mDebugGrassLod(false),
    mHideGrass(true),
    // Terrain
    mTerrainLodDistanceOffset(540.0f), // 1500 for ultra
    mDebugTerrainLod(false),
    mTerrainHeightColorMult(0.22f),
    mTerrainWavyColorMult(0.167f),
    mTerrainSquaresColorPeriod(0.187f),
    mTerrainSquaresIntensity(0.0f),
    mTerrainBlendMult(0.037f),
    mDisableTerrain(false),
    // DOF
    mDepthOfFieldBlurRadius(0.6f),
    mDepthOfFieldBlurPasses(1),
    mDepthOfFieldRangeNear(0.0f, 2.0f),
    mDepthOfFieldRangeFar(10.0f, 1000.0f),
    mDepthOfFieldExponent(1.0f),
    mDepthOfFieldDebugRender(false),
    // Ambient occlusion
    mSSAODisabled(true),
    mSSAORadius(1.0f),
    mSSAOBias(0.008f),
    mSSAOBlurRadius(0.77f),
    mSSAOBlurPasses(2),
    mSSAORangeCheckMult(0.5f),
    mSSAOColor(14.0f / 255.0f, 0.0f / 255.0f, 25.0f / 255.0f),
    // Shadows
    mShadowZMult(6.0f),//2.50f),
    mShadowNearSize(17.0f),
    mShadowColor(204.0f / 255.0f, 230.0f / 255.0f, 243.0f / 255.0f),
    mShadowUpdateRateSeconds(0.022f),
    mShadowBlurPasses(3), // 2
    mShadowBlurRadius(0.45f), //1.5f),
    mDisableShadows(true),
    // Toggles
    mPauseFrustum(false),
    mWireframe(false),
    mChunkBoundaries(false),
    mCities(false),
    mRoofDebug(false),
    mShowNavGraph(false),
    mShowNavGraphUpdates(false),
    mHideCharacters(false),
    mShowTerrainPhysics(false),
    mShowStaticPhysics(false),
    mShowDynamicPhysics(false),
    mShowPhysicsActions(true),
    mShowBusinessDebug(true),
    mShowTweaker(false),
    mShowEditor(false),
    mShowPaths(true),
    mShowEntityQueries(false),
    mEnableVisualLogs(true),
    mShowDevHud(true),
    mHideModels(false),
    // Water
    mShallowWaterColor(159.0f / 255.0f, 194.0f / 255.0f, 206.0f / 255.0f, 185.0f / 255.0f),
    mDeepWaterColor(57.0f / 255.0f, 83.0f / 255.0f, 122.0f / 255.0f, 191.0f / 255.0f),
    mWaterFoamColor(111.0f / 255.0f, 148.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f),
    mWaterSurfaceDistortAmount(0.27f),
    mWaterSurfaceMoveSpeed(0.03f),
    mWaterFoamDistanceRange(0.4f, 1.0f),
    mWaterSurfaceNoiseCutoff(0.777f),
    mWaterSmoothstepAA(0.037f),
    mWaterColorNoiseIntensity(0.085f),
    mWaterDistortTiling(1.0f),
    mWaterNoiseTiling(1.0f),
     mDisableWater(false),
    // Lighting
    mLightingOptions(&sLightingPresets[LIGHT_PRESET_UCHIMURA]),
    mLightingOptionsSplit(&sLightingPresets[LIGHT_PRESET_CUSTOM]),
    mLightingPreset(LIGHT_PRESET_UCHIMURA),
    mLightingPresetSplit(LIGHT_PRESET_CUSTOM),
     mLightPresetSplitView(false),
    mLightPresetSplitAmount(0.43f),
    unUchMaxDisplayBrightness(1.1f),
    unUchContrast(0.7f),
    unUchLinearSectionStart(0.06f),
    unUchLinearSectionLength(0.6f),
    unUchBlack(1.33f),
    unUchPedestal(0.0f),
    // Game settings
    mLoadRangeSq(SQ(CHUNK_LOAD_RANGE * CHUNK_LOAD_RANGE_MULT)),
    mLoadRange(CHUNK_LOAD_RANGE * CHUNK_LOAD_RANGE_MULT),
    // Camera settings
    mFoV(75.0f),
    mZFar(200000.0f),
    mCameraZHeight(1.5f),
    mCameraXYDistance(1.0f),
    mCameraMode(CameraMode::MMO),
    // Shader debug
    mDebugColor01(0.0f, 0.0f, 1.0f),
    mDebugColor02(0.0f, 1.0f, 0.0f),
    mDebugFloat01(0.0f),
    mDebugFloat02(0.0f),
    mDebugFloat03(0.0f),
    mDebugFloat04(0.0f),

    // TODO: FILE CONFIG
    mUseCompressedAtlas(false),
    mVSYNC(true),
    // TODO: somewhere else?
    mScreenResolution(1600.0f, 900.0f) // Currently set in  App::onInit
{

}
