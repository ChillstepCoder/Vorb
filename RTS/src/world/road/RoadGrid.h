#pragma once

#include "util/SpatialGrid2D.h"

#include <shared_mutex>

constexpr ui8 MAX_ROAD_THICKNESS = UINT8_MAX;

constexpr ui32 ROAD_POINT_STRIDE = 2;
constexpr ui32 ROAD_GRID_CELL_WIDTH_TILES = CHUNK_WIDTH;
constexpr ui32 ROAD_GRID_CELL_WIDTH_POINTS = ROAD_GRID_CELL_WIDTH_TILES / ROAD_POINT_STRIDE;
constexpr ui32 ROAD_GRID_CELL_SIZE_POINTS = SQ(ROAD_GRID_CELL_WIDTH_POINTS);

struct RoadPoint {
    ui8 thickness = 0;
    ui8 type = 0;
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
    RoadPoint getRoadPointFloored(i32v2 worldPos) const;

    void setRoadPointFloored(i32v2 worldPos, RoadPoint point);
    void adjustRoadPointFloored(i32v2 worldPos, i32 adjust);

private:
    ui32 mWorldWidthTiles;
    ui32 mTotalCells;
    ui32 mWidthCells = 0;
    SpatialGrid2D mSpatialGrid;
    std::unique_ptr<RoadGridCell[]> mRoadData;
};

