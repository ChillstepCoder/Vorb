#include "stdafx.h"
#include "DebugOptions.h"

DebugOptions sDebugOptions;

// Lower for faster loading in test
#ifdef DEBUG
constexpr f32 CHUNK_LOAD_RANGE_MULT = 0.35f /*0.35*/;
#else
constexpr f32 CHUNK_LOAD_RANGE_MULT = 0.65f /*0.65*/;
#endif

DebugOptions::DebugOptions() :
    mTimeOffset(0.0f),
    // Clouds
    mDebugClouds(false),
    mDisableClouds(false),
    mCloudBlurPasses(3),
    mCloudBlurRadius(1.25f),
    mCloudAmbient(0.5f),
    mCloudSpeed(0.05f),
    mCloudMetallic(0.0f),
    mCloudRoughness(1.0f),
    // Grass
    mGrassSettings{ DEFAULT_GRASS_DISTANCE, SQ(DEFAULT_GRASS_DISTANCE), DEFAULT_GRASS_DISTANCE * GRASS_FADE_MULT, 50.0f },
    mDebugGrassLod(false),
    mHideGrass(false),
    // Structures
    mWallWoobleChance(0.3f),
    mWallWoobleIntensity(0.15f),
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
    mDepthOfFieldRangeNear(0.0f, 1.0f),
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
    mShadowColor(89.0f / 255.0f, 147.0f / 255.0f, 255.0f / 255.0f),
    mShadowUpdateRateSeconds(0.022f),
    mShadowBlurPasses(3), // 2
    mShadowBlurRadius(0.4f),  // (0.045f) //1.5f),
    mDisableShadows(false),
    // Smudge
    mSmudgeTestPasses(1),
    mSmudgeTestRadius(7.0f),
    mSmudgeTestNormThreshold(0.05f), //0.016f
    mSmudgeTestDepthThreshold(0.104f),
    mSmudgeTestShowVariance(0),
    mSmudgeTestShowEdges(0),
    mSmudgeTestDisable(0),
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
    mShowPhysicsActions(false),
    mShowBusinessDebug(true),
    mShowEditor(false),
    mShowPaths(true),
    mShowEntityQueries(false),
    mEnableVisualLogs(true),
    mShowDevHud(true),
    mHideModels(false),
    mDisableLOD(false),
    mDisableGPUCulling(false),
    // Water
    mShallowWaterColor(65.0f / 255.0f, 127.0f / 255.0f, 173.0f / 255.0f, 185.0f / 255.0f),
    mDeepWaterColor(6.0f / 255.0f, 13.0f / 255.0f, 24.0f / 255.0f, 191.0f / 255.0f),
    mWaterFoamColor(126.0f / 255.0f, 164.0f / 255.0f, 216.0f / 255.0f, 255.0f / 255.0f),
    mWaterSurfaceDistortAmount(0.27f),
    mWaterSurfaceMoveSpeed(0.03f),
    mWaterFoamDistanceRange(0.4f, 1.0f),
    mWaterSurfaceNoiseCutoff(0.777f),
    mWaterSmoothstepAA(0.037f),
    mWaterColorNoiseIntensity(0.037f),
    mWaterDistortTiling(1.0f),
    mWaterNoiseTiling(1.0f),
    mWaterMetallic(0.51f),
    mWaterRoughness(0.05f),
    mDisableWater(false),
    // Lighting
    mUsingPBR(true),
    mLightingOptions(&sLightingPresets[mUsingPBR][LIGHT_PRESET_UCHIMURA]),
    mLightingOptionsSplit(&sLightingPresets[mUsingPBR][LIGHT_PRESET_CUSTOM]),
    mLightingPreset(LIGHT_PRESET_UCHIMURA),
    mLightingPresetSplit(LIGHT_PRESET_CUSTOM),
     mLightPresetSplitView(false),
    mLightPresetSplitAmount(0.5f),
    unUchMaxDisplayBrightness(1.1f),
    unUchContrast(0.7f),
    unUchLinearSectionStart(0.06f),
    unUchLinearSectionLength(0.6f),
    unUchBlack(1.33f),
    unUchPedestal(0.0f),
    mSunColorPeak(1.0f, 1.0f, 1.0f),
    mSunColorSunset(1.0f, 0.5f, 0.0f),
    // Game settings
    mLoadRangeSq(SQ(CHUNK_LOAD_RANGE * CHUNK_LOAD_RANGE_MULT)),
    mLoadRange(CHUNK_LOAD_RANGE * CHUNK_LOAD_RANGE_MULT),
    mLodDistances{65.0f, 125.0f, 500.0f},
    // Camera settings
    mFoV(80.0f),
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
