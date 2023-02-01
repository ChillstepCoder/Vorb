#include "stdafx.h"
#include "LightingOptions.h"

LightingOptions sLightingPresets[2][LIGHT_PRESET_COUNT] = {
    { // OLD
        // CUSTOM
        LightingOptions(),
        // UCHIMURA
        LightingOptions(1.0f /*gamma*/, 1.0f /*exposure*/, 0.55f /*hazeExponent*/, TONEMAP_UCHIMURA,
        6000.0f /*hazedivisor*/, 0.1f /*ambient*/, 0.9f /*sunintensity*/, LIGHTING_MODEL::PHONG),
        // LOTTES
        LightingOptions(0.65f /*gamma*/, 1.0f /*exposure*/, 0.45f /*hazeExponent*/, TONEMAP_LOTTES,
        10000.0f /*hazedivisor*/, 0.1f /*ambient*/, 0.707f /*sunintensity*/, LIGHTING_MODEL::PHONG),

        // REINARD
        LightingOptions(1.0f /*gamma*/, 0.375f /*exposure*/, 0.45f /*hazeExponent*/, TONEMAP_REINARD,
        10000.0f /*hazedivisor*/, 0.1f /*ambient*/, 0.707f /*sunintensity*/, LIGHTING_MODEL::PHONG),

        // UNCHARTED2
        LightingOptions(0.748f /*gamma*/, 2.065f /*exposure*/, 0.45f /*hazeExponent*/, TONEMAP_UNCHARTED2,
        10000.0f /*hazedivisor*/, 0.1f /*ambient*/, 1.0f /*sunintensity*/, LIGHTING_MODEL::PHONG),

    }, 
    { // PBR

        // CUSTOM
        LightingOptions(2.2f /*gamma*/, 1.0f /*exposure*/, 0.55f /*hazeExponent*/, TONEMAP_NONE,
        6000.0f /*hazedivisor*/, 1.0f /*ambient*/, 3.0f /*sunintensity*/, LIGHTING_MODEL::PHONG),
        // UCHIMURA
        LightingOptions(2.2f /*gamma*/, 1.0f /*exposure*/, 1.0f /*hazeExponent*/, TONEMAP_UCHIMURA,
        7000.0f /*hazedivisor*/, 1.0f /*ambient*/, 3.0f /*sunintensity*/, LIGHTING_MODEL::PHONG),
        // LOTTES
        LightingOptions(2.2f /*gamma*/, 1.0f /*exposure*/, 0.45f /*hazeExponent*/, TONEMAP_LOTTES,
        10000.0f /*hazedivisor*/, 1.0f /*ambient*/, 1.0f /*sunintensity*/, LIGHTING_MODEL::PHONG),

        // REINARD
        LightingOptions(2.2f /*gamma*/, 1.0f /*exposure*/, 0.45f /*hazeExponent*/, TONEMAP_REINARD,
        10000.0f /*hazedivisor*/, 1.0f /*ambient*/, 3.0f /*sunintensity*/, LIGHTING_MODEL::PHONG),

        // UNCHARTED2
        LightingOptions(2.2f /*gamma*/, 1.0f /*exposure*/, 0.45f /*hazeExponent*/, TONEMAP_UNCHARTED2,
        10000.0f /*hazedivisor*/, 1.0f /*ambient*/, 3.0f /*sunintensity*/, LIGHTING_MODEL::PHONG),

   } 
};
static_assert(LIGHT_PRESET_COUNT == 5, "Update");

LightingOptions sLightingPresetDefaults[2][LIGHT_PRESET_COUNT] = { 
    {
        sLightingPresets[0][0],
        sLightingPresets[0][1],
        sLightingPresets[0][2],
        sLightingPresets[0][3],
        sLightingPresets[0][4],
    },
    {
        sLightingPresets[1][0],
        sLightingPresets[1][1],
        sLightingPresets[1][2],
        sLightingPresets[1][3],
        sLightingPresets[1][4],
    }
};
static_assert(LIGHT_PRESET_COUNT == 5, "Update");