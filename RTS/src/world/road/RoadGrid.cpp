#include "stdafx.h"
#include "RoadGrid.h"

// For ROAD_VERTEX_STRIDE
#include "world/TerrainConstants.h"

RoadGrid::RoadGrid(ui32 worldWidthTiles) : mWorldWidthTiles(worldWidthTiles) {
    mWidthCells = worldWidthTiles / ROAD_GRID_CELL_WIDTH_TILES;
    mSpatialGrid.init(ROAD_GRID_CELL_WIDTH_TILES, mWidthCells);
    mTotalCells = SQ(mWidthCells);
    mRoadData = std::make_unique<RoadGridCell[]>(mTotalCells);
    LOG_DEBUG("Road grid allocated {} mb data",
        (mTotalCells * sizeof(RoadGridCell)) / 1024.f / 1024.f);

}

RoadGrid::~RoadGrid() = default;

template <bool THREAD_SAFE>
RoadPoint RoadGrid::getRoadPointFloored(i32v2 worldPos) const {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthTiles || worldPos.y >= mWorldWidthTiles) [[unlikely]] {
        return RoadPoint();
    }
    i32v2 cellOffset;
    const ui32 cellId = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos, cellOffset);
    cellOffset /= ROAD_POINT_STRIDE;
    RoadGridCell& cell = mRoadData[cellId];
    if constexpr (THREAD_SAFE) {
        std::shared_lock lock(cell.mutex);
        if (cell.points == nullptr) {
            return RoadPoint();
        }
        return cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x];
    } else {
        if (cell.points == nullptr) {
            return RoadPoint();
        }
        return cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x];
    }
}
DECL_BOOL_TEMPLATE(RoadPoint RoadGrid::getRoadPointFloored, (i32v2 worldPos) const)

void RoadGrid::setRoadPointFloored(i32v2 worldPos, RoadPoint point) {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthTiles || worldPos.y >= mWorldWidthTiles) [[unlikely]] {
        return;
    }
    i32v2 cellOffset;
    const ui32 cellId = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos, cellOffset);
    cellOffset /= ROAD_POINT_STRIDE;
    RoadGridCell& cell = mRoadData[cellId];
    std::shared_lock lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<RoadPoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x] = point;
}

void RoadGrid::adjustRoadPointFloored(i32v2 worldPos, i32 adjust) {
    i32v2 cellOffset;
    const ui32 cellId = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos, cellOffset);
    cellOffset /= ROAD_POINT_STRIDE;
    RoadGridCell& cell = mRoadData[cellId];
    std::shared_lock lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<RoadPoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    RoadPoint& point = cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x];
    point.thickness = (ui8)glm::clamp((i32)point.thickness + adjust, 0, (i32)MAX_ROAD_THICKNESS);
}

