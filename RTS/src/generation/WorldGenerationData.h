#pragma once

#include "NoiseFunction.hpp"

struct WorldGenerationData {
    NoiseFunction mBaseNoise             = { "Base", 11, 0.7, 0.00008, {-1098, -3228} };
    NoiseFunction mContinentOutlineNoise = { "Continent Outline", 9, 0.65, 0.0001, {-2000.0, 0.0} }; 
    NoiseFunction mTemperatureNoise      = { "Temperature", 4, 0.7, 0.00005, {0.0, 0.0} };
    NoiseFunction mHumidityNoise         = { "Humidity", 4, 0.7, 0.00008, {0.0, 0.0} };

    bool mIsDirty = false;
};
extern WorldGenerationData sWorldGenData;