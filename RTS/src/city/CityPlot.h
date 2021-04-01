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
#define INVALID_PLOT_INDEX UINT32_MAX;

struct CityPlot {
    CityPlot() {};
    CityPlot(const ui32v4& aabb, CityPlotIndex plotIndex, CityDistrict* parentDistrict) :
        aabb(aabb), plotIndex(plotIndex), parentDistrict(parentDistrict) {
    };

    void setNeighborRoad(Cartesian dir, RoadID id) {
        neighborRoads[enum_cast(dir)] = id;
    }

    ui32v4 aabb;
    CityPlotIndex plotIndex = INVALID_PLOT_INDEX;
    CityDistrict* parentDistrict = nullptr;
    bool isFree = true;

    // TODO: More?
    RoadID neighborRoads[CARTESIAN_COUNT] = { INVALID_ROAD_ID, INVALID_ROAD_ID, INVALID_ROAD_ID, INVALID_ROAD_ID };
};
