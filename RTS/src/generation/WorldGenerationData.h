#pragma once

#include "NoiseFunction.hpp"

constexpr f32 MIN_WORLD_GEN_HEIGHT = -200.0f;
constexpr f32 MAX_WORLD_GEN_HEIGHT = 900.0f;

// NoiseFunction(const char* label, int octaves, f64 persistence, f64 frequency, f64v2 posOffset, f64 amplitude = 1.0, f64 heightOffset = 0.0) 
// TIP FOR USING NOISE - Start with a very high frequency to see the shape at a zoomed out scale, then reduce frequency
struct WorldGenerationData {
    // NoiseFunction mBaseNoise = NoiseFunction("Base", 7, 0.7, 0.001, { 0, 0 }, 3.0, 0.4);
    NoiseFunction mBaseNoise             = NoiseFunction("Base", 7, 0.7, 0.001, {0, 0}, 25.0, 0.0);
    NoiseFunction mContinentOutlineNoise = NoiseFunction("Continent Outline", 9, 0.65, 0.0001, {-2000.0, 0.0}, 1.0, 0.0);
    CellularNoiseFunction mMountainsNoise  = CellularNoiseFunction("Mountains", 2, 0.6, 0.00157, { 0, 0 }, 150.0, -0.3);
    //RidgedNoiseFunction mMountainsNoise  = RidgedNoiseFunction("Mountains", 1, 0.6, 0.00157, { 0, 0 }, 100.0, -0.3);
    NoiseFunction mMountainsDistNoise    = NoiseFunction("Mountains Dist", 7, 0.6, 0.000166, { 0, 0 }, 11.0, -0.15);
    NoiseFunction mTemperatureNoise      = NoiseFunction("Temperature", 4, 0.7, 0.00005, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mHumidityNoise         = NoiseFunction("Humidity", 4, 0.7, 0.00008, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mGrassNoise            = NoiseFunction("Grass", 6, 0.7, 0.05, {1200.0, -1200.0}, 1.0, 0.0);
    NoiseFunction mFlowerNoise           = NoiseFunction("Flowers", 5, 0.7, 0.01, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mCloudsNoise           = NoiseFunction("Clouds", 5, 0.7, 0.001, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mCloudHeightNoise      = NoiseFunction("CloudsHeight", 2, 0.8, 0.001, {4000.0, -5000.0}, 1.0, 0.0);
    NoiseFunction mForestNoise           = NoiseFunction("Forest Dist", 2, 0.6, 0.002, { 2000, 0 }, 1.0, 0.0);

    // === Continent noise modifiers ===
    // Configurable
    f64 CONTINENT_RADIUS = 10000.0;
    f64 CONTINENT_OUTLINE_SCALE = SQ(20000.0);
    // Constant
    f64 CONTINENT_RADIUS_SQ = SQ(CONTINENT_RADIUS);

    bool mIsDirty = false;
};