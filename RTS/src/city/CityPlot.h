#pragma once

#include "CityConst.h"

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
    CityPlot(const ui32AABB2& aabb, CityPlotIndex plotIndex, CityDistrict* parentDistrict) :
        aabb(aabb), plotIndex(plotIndex), parentDistrict(parentDistrict) {
    };

    void setNeighborRoad(Cartesian dir, RoadID id) {
        neighborRoads[enum_cast(dir)] = id;
    }

    RoadID getNeighborRoad(Cartesian dir) {
        return neighborRoads[enum_cast(dir)];
    }

    int getAdjacentRoadCount() const {
        int count = 0;
        for (int i = 0; i < CARTESIAN_COUNT; ++i) {
            count += (int)(neighborRoads[i] != INVALID_ROAD_ID);
        }
        return count;
    }

    ui32AABB2 aabb;
    CityPlotIndex plotIndex = INVALID_PLOT_INDEX;
    CityDistrict* parentDistrict = nullptr;
    bool isFree = true;

    // TODO: More?
    RoadID neighborRoads[CARTESIAN_COUNT] = { INVALID_ROAD_ID, INVALID_ROAD_ID, INVALID_ROAD_ID, INVALID_ROAD_ID };
};
