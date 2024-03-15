#include "stdafx.h"
#include "DebugOptions.h"

#include "LightingOptions.h"

DebugOptions sDebugOptions;

// Lower for faster loading in test
#ifdef DEBUG
constexpr f32 CHUNK_LOAD_RANGE_MULT = 0.35f /*0.35*/;
#else
constexpr f32 CHUNK_LOAD_RANGE_MULT = 0.55f /*0.65*/;
#endif

DebugOptions::DebugOptions() :
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
    mGrassScale(1.0f, 1.0f),
    mGrassLeanVariance(1.0f),
    mGrassColorMapScale(0.005f),
    mDebugGrassLod(false),
    mHideGrass(false),
    // Structures
    mWallWoobleChance(0.3f),
    mWallWoobleIntensity(0.15f),
    mStructureDebug(false),
    mBlueprintDebug(true),
    // Terrain
    mTerrainLodDistanceOffset(540.0f), // 1500 for ultra
    mDebugTerrainLod(false),
    mTerrainHeightColorMult(0.22f),
    mTerrainWavyColorMult(0.167f),
    mTerrainSquaresColorPeriod(0.187f),
    mTerrainSquaresIntensity(0.0f),
    mTerrainBlendMult(0.037f),
    mTerrainDetailTextureStrength(1.0f),
    mTerrainCliffBlendHardness(100.0),
    mTerrainCliffAmount(0.284),
    mTerrainCliffZMult(1.5),
    mBiomeBlendScale(1.0f),
    mBiomeBlendFrequency(1.0f),
    mDisableTerrain(false),
    mDebugToggle0(false),
    // Fish
    mFishRenderDistance(128.0f),
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
    mSmudgeTestRadius(4.0f),
    mSmudgeTestNormThreshold(0.05f), //0.016f
    mSmudgeTestDepthThreshold(0.104f),
    mSmudgeTestShowVariance(0),
    mSmudgeTestShowEdges(0),
    mSmudgeTestDisable(0),
    mSmudgePaintNoisePasses(1),
    mSmudgePaintNoiseIntensity(2.0f),
    mSmudgePaintNoiseOffset(-0.544f),
    mSmudgePaintNoiseFrequency(0.007f),
    mSmudgePaintNoiseAmplitude(4.05f),
    mSmudgePaintNoiseDisable(false),
    mSmudgePaintNoiseDebug(false),
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
    mShowSettlementDebug(true),
    mShowEditor(false),
    mShowPaths(true),
    mDebugFishEcosystem(false),
    mShowFish(true),
    mShowEntityQueries(false),
    mShowPhysicsQueries(false),
    mShowCombatQueries(false),
    mEnableVisualLogs(true),
    mShowDevHud(true),
    mHideModels(false),
    mHideDynamicModels(false),
    mDisableLOD(false),
    mDisableGPUCulling(true), // GPU CULLING HAS A BUG IT CAUSES SHADOW FKERY
    // Water
    mIsCameraUnderwater(false),
    mUnderwaterHazeDivisor(70.0f),
    mUnderwaterOverlayColor(10.0f / 255.0f, 24.0f / 255.0f, 61.0f / 255.0f, 175.0f / 255.0f),
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
