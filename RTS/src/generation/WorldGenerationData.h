#pragma once

#include "NoiseFunction.hpp"

constexpr f32 MIN_WORLD_GEN_HEIGHT = -200.0f;
constexpr f32 MAX_WORLD_GEN_HEIGHT = 900.0f;
constexpr int MAX_WORLD_GEN_SEED_SIZE = 16;

// NoiseFunction(const char* label, int octaves, f64 persistence, f64 frequency, f64v2 posOffset, f64 amplitude = 1.0, f64 heightOffset = 0.0) 
// TIP FOR USING NOISE - Start with a very high frequency to see the shape at a zoomed out scale, then reduce frequency
struct WorldGenerationData {
    // NoiseFunction mBaseNoise = NoiseFunction("Base", 7, 0.7, 0.001, { 0, 0 }, 3.0, 0.4);
    NoiseFunction mBaseNoise             = NoiseFunction(CStrToken("Base"), NoiseFunctionType::Standard, 7, 0.7, 0.001, {0, 0}, 25.0, 0.0);
    NoiseFunction mContinentOutlineNoise = NoiseFunction(CStrToken("Continent Outline"), NoiseFunctionType::Standard, 9, 0.65, 0.0001, {-2000.0, 0.0}, 1.0, 0.0);
    NoiseFunction mMountainsNoise  = NoiseFunction(CStrToken("Mountains"), NoiseFunctionType::Cellular, 2, 0.6, 0.00157, { 0, 0 }, 150.0, -0.3);
    //RidgedNoiseFunction mMountainsNoise  = RidgedNoiseFunction("Mountains", 1, 0.6, 0.00157, { 0, 0 }, 100.0, -0.3);
    NoiseFunction mMountainsDistNoise    = NoiseFunction(CStrToken("Mountains Dist"), NoiseFunctionType::Standard, 7, 0.6, 0.000166, { 0, 0 }, 11.0, -0.15);
    NoiseFunction mTemperatureNoise      = NoiseFunction(CStrToken("Temperature"), NoiseFunctionType::Standard, 4, 0.7, 0.00005, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mHumidityNoise         = NoiseFunction(CStrToken("Humidity"), NoiseFunctionType::Standard, 4, 0.7, 0.00008, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mGrassNoise            = NoiseFunction(CStrToken("Grass"), NoiseFunctionType::Standard, 6, 0.7, 0.05, {1200.0, -1200.0}, 1.0, 0.0);
    NoiseFunction mFlowerNoise           = NoiseFunction(CStrToken("Flowers"), NoiseFunctionType::Standard, 5, 0.7, 0.01, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mCloudsNoise           = NoiseFunction(CStrToken("Clouds"), NoiseFunctionType::Standard, 5, 0.7, 0.001, {0.0, 0.0}, 1.0, 0.0);
    NoiseFunction mCloudHeightNoise      = NoiseFunction(CStrToken("CloudsHeight"), NoiseFunctionType::Standard, 2, 0.8, 0.001, {4000.0, -5000.0}, 1.0, 0.0);
    NoiseFunction mForestNoise           = NoiseFunction(CStrToken("Forest Dist"), NoiseFunctionType::Standard, 2, 0.6, 0.002, { 2000, 0 }, 1.0, 0.0);

    // === Continent noise modifiers ===
    // Configurable
    f32v2 mWorldCenter = f32v2(16384.f);
    f32v2 mWorldPosRoot = f32v2(0.0f);
    f32 mContinentRadius = 14000.0f;
    f32 mContinentOutlineScale = SQ(20000.0f);
    // Constant
    f32 mContinentRadiusSq = SQ(mContinentRadius);
    char mSeed[MAX_WORLD_GEN_SEED_SIZE] = "default";

    f32 getSeedHash() const {
        int hash = 425381; // Starting value
        int c;

        // DJB2 Hash
        for (size_t i = 0; i < MAX_WORLD_GEN_SEED_SIZE && mSeed[i] != '\0'; ++i) {
            c = static_cast<unsigned char>(mSeed[i]);
            hash = ((hash << 5) + hash) + c; // hash * 33 + c
        }
        // Bound the seed so it doesn't damage GPU precision with large numbers
        // Returns numbers in (-32768.5, 32768.5)
        return hash / ((f32)INT_MAX / 65535.0f);
    }

    bool mIsDirty = false;
};