#pragma once

enum class LIGHTING_MODEL {
    PHONG = 0,
    BLINN_PHONG = 1,
    COUNT
};

enum LIGHT_PRESETS {
    LIGHT_PRESET_CUSTOM,
    LIGHT_PRESET_UCHIMURA,
    LIGHT_PRESET_LOTTES,
    LIGHT_PRESET_REINARD,
    LIGHT_PRESET_UNCHARTED2,
    LIGHT_PRESET_COUNT
};

constexpr const char* const LIGHT_PRESET_NAMES[LIGHT_PRESET_COUNT] = {
    "CUSTOM",
    "UCHIMURA",
    "LOTTES",
    "REINARD",
    "UNCHARTED2"
};
static_assert(LIGHT_PRESET_COUNT == 5);

enum TONEMAP_OPERATORS {
    TONEMAP_NONE,
    TONEMAP_REINARD,
    TONEMAP_LOTTES,
    TONEMAP_UCHIMURA,
    TONEMAP_UNREAL,
    TONEMAP_FILMIC,
    TONEMAP_UNCHARTED2,
    TONEMAP_COUNT
};



struct LightingOptions {
    LightingOptions() {};
    LightingOptions(f32 gamma, f32 exposure, f32 hazeExponent, int tonemapOperator, f32 hazeDivizor, f32 ambient, f32 sunIntensity, LIGHTING_MODEL lightingModel) :
        mGamma(gamma), mExposure(exposure), mHazeExponent(hazeExponent), mToneMapOperator(tonemapOperator), mHazeDivisor(hazeDivizor),
        mAmbient(ambient), mSunIntensity(sunIntensity), mLightingModel(e_cast(lightingModel)) {
    }

    f32 mGamma = 1.0f;
    f32 mExposure = 1.0f;
    f32 mHazeExponent = 0.45f;
    int mToneMapOperator = TONEMAP_NONE;
    f32 mHazeDivisor = 10000.0f;
    f32 mAmbient = 0.1f;
    f32 mSunIntensity = 1.0f;
    int mLightingModel = e_cast(LIGHTING_MODEL::PHONG);
};

extern LightingOptions sLightingPresets[LIGHT_PRESET_COUNT];
extern LightingOptions sLightingPresetDefaults[LIGHT_PRESET_COUNT];