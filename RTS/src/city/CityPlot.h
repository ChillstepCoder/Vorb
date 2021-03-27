#pragma once

// TODO: non power of two
enum class CityBlockSize {
    SMALL = 32,
    MEDIUM = 64,
    LARGE = 128,
    HUGE = 256
};

struct CityDistrict;
typedef ui32 CityPlotIndex;

struct CityPlot {
    CityPlot() {};
    CityPlot(const ui32v4& aabb, CityPlotIndex plotIndex, CityDistrict* parentDistrict) :
        aabb(aabb), plotIndex(plotIndex), parentDistrict(parentDistrict) {
    };

    ui32v4 aabb;
    CityPlotIndex plotIndex = UINT32_MAX;
    CityDistrict* parentDistrict = nullptr;
    bool isFree = true;
};
