#pragma once

#include "util/SpatialGrid2D.h"

#include <shared_mutex>
#include "TerrainSurfaceType.h"

constexpr ui8 MAX_ROAD_STRENGTH = UINT8_MAX;

constexpr ui32 ROAD_POINT_STRIDE = DTILE_WIDTH;
constexpr ui32 ROAD_GRID_CELL_WIDTH_POINTS = CHUNK_WIDTH / ROAD_POINT_STRIDE;
constexpr ui32 ROAD_GRID_CELL_SIZE_POINTS = SQ(ROAD_GRID_CELL_WIDTH_POINTS);

struct TerrainSurfacePoint {
    TerrainSurfaceType baseType = {};
    TerrainSurfaceOverlayType overlayType = {};
};
static_assert(sizeof(TerrainSurfacePoint) == 2);

// Roads, farm plots, ect
class TerrainSurfaceGrid {
public:
    TerrainSurfaceGrid(ui32 worldWidthTiles);
    ~TerrainSurfaceGrid();

    template <bool THREAD_SAFE>
    TerrainSurfacePoint getSurfacePoint(DTileCoord worldPos) const;

    void setSurfacePoint(DTileCoord worldPos, TerrainSurfacePoint point);
    void setBaseSurfaceType(DTileCoord worldPos, TerrainSurfaceType type);
    bool setBaseSurfaceTypeIfEmpty(DTileCoord worldPos, TerrainSurfaceType type);

    // Return false if fail to place, such as if we tried to place seeds on a road
    bool trySetOverlaySurfaceType(DTileCoord worldPos, TerrainSurfaceOverlayType type);

private:

    struct Cell {
        std::unique_ptr<TerrainSurfacePoint[]> points = nullptr;
        std::shared_mutex mutex;
    };

    ui32 mWorldWidthDTiles;
    ui32 mTotalCells;
    ui32 mWidthCells = 0;
    SpatialGrid2D mSpatialGrid;
    std::unique_ptr<Cell[]> mRoadData;
};

