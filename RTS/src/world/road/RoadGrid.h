#pragma once

#include "util/SpatialGrid2D.h"

#include <shared_mutex>

constexpr ui8 MAX_ROAD_STRENGTH = UINT8_MAX;

constexpr ui32 ROAD_POINT_STRIDE = DTILE_WIDTH;
constexpr ui32 ROAD_GRID_CELL_WIDTH_POINTS = CHUNK_WIDTH / ROAD_POINT_STRIDE;
constexpr ui32 ROAD_GRID_CELL_SIZE_POINTS = SQ(ROAD_GRID_CELL_WIDTH_POINTS);

struct RoadPoint {
    ui8 strength = 0;
    ui8 type = {};
};
static_assert(sizeof(RoadPoint) == 2);

struct RoadGridCell {
    std::unique_ptr<RoadPoint[]> points = nullptr;
    std::shared_mutex mutex;
};

class RoadGrid {
public:
    RoadGrid(ui32 worldWidthTiles);
    ~RoadGrid();

    template <bool THREAD_SAFE>
    RoadPoint getRoadPoint(DTileCoord worldPos) const;

    void setRoadPoint(DTileCoord worldPos, RoadPoint point);
    // Only sets the road point if it is either a different type, or higher intensity that what already exists
    bool setRoadPointIfHigherIntensity(DTileCoord worldPos, RoadPoint point);
    void adjustRoadPoint(DTileCoord worldPos, i32 adjust);

    MaterialID getRoadMaterialFromType(ui8 type) const;

private:
    ui32 mWorldWidthDTiles;
    ui32 mTotalCells;
    ui32 mWidthCells = 0;
    SpatialGrid2D mSpatialGrid;
    std::unique_ptr<RoadGridCell[]> mRoadData;
};

