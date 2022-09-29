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
struct BuildingBlueprint;
typedef ui32 CityPlotIndex;
#define INVALID_PLOT_INDEX UINT32_MAX;

struct CityPlot {
    CityPlot();
    CityPlot(const i32AABB2& aabb, CityPlotIndex plotIndex, CityDistrict* parentDistrict);
    ~CityPlot();

    void setNeighborRoad(Cartesian dir, RoadID id) {
        neighborRoads[e_cast(dir)] = id;
    }

    RoadID getNeighborRoad(Cartesian dir) {
        return neighborRoads[e_cast(dir)];
    }

    int getAdjacentRoadCount() const {
        int count = 0;
        for (int i = 0; i < CARTESIAN_COUNT; ++i) {
            count += (int)(neighborRoads[i] != INVALID_ROAD_ID);
        }
        return count;
    }

    i32AABB2 aabb;
    CityPlotIndex plotIndex = INVALID_PLOT_INDEX;
    BuildingID buildingId = INVALID_BUILDING_ID;
    CityDistrict* parentDistrict = nullptr;
    // Entity owning this plot, can be a person or a business
    entt::entity mOwnerEntity = INVALID_ENTITY;
    bool isFree = true;
    std::unique_ptr<BuildingBlueprint> mPendingBlueprint = nullptr;

    // TODO: More?
    RoadID neighborRoads[CARTESIAN_COUNT] = { INVALID_ROAD_ID, INVALID_ROAD_ID, INVALID_ROAD_ID, INVALID_ROAD_ID };
};
