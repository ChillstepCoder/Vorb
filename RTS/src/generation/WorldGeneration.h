#pragma once

#include "NoiseFunction.hpp"

struct WorldGeneration {
    NoiseFunction mBaseNoise             = { "Base", 11, 0.7, 0.00008, {-1098, -3228} };
    NoiseFunction mContinentOutlineNoise = { "Continent Outline", 9, 0.65, 0.0001, {-2000.0, 0.0} }; 
    NoiseFunction mTemperatureNoise      = { "Temperature", 4, 0.7, 0.00005, {0.0, 0.0} };
    NoiseFunction mHumidityNoise         = { "Humidity", 4, 0.7, 0.00008, {0.0, 0.0} };
    NoiseFunction mGrassNoise            = { "Grass", 9, 0.68, 0.0005, {0.0, 0.0} };
    NoiseFunction mFlowerNoise           = { "Flowers", 5, 0.7, 0.01, {0.0, 0.0} };
    NoiseFunction mCloudsNoise           = { "Clouds", 5, 0.7, 0.001, {0.0, 0.0} };
    NoiseFunction mCloudHeightNoise      = { "CloudsHeight", 2, 0.8, 0.001, {4000.0, -5000.0} };

    // === Continent noise modifiers ===
    // Configurable
    static constexpr f64 CONTINENT_RADIUS = 10000.0;
    static constexpr f64 CONTINENT_OUTLINE_SCALE = SQ(20000.0);
    // Constant
    static constexpr f64 CONTINENT_RADIUS_SQ = SQ(CONTINENT_RADIUS);

    f32 getHeightAtPos(const f32v2& worldPos);

    bool mIsDirty = false;
};
extern WorldGeneration sWorldGen;