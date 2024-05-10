#include "stdafx.h"
#include "RoadGrid.h"

// For ROAD_VERTEX_STRIDE
#include "world/TerrainConstants.h"

RoadGrid::RoadGrid(ui32 worldWidthTiles) : mWorldWidthDTiles(worldWidthTiles / 2) {
    mWidthCells = worldWidthTiles / ROAD_GRID_CELL_WIDTH_POINTS;
    mSpatialGrid.init(ROAD_GRID_CELL_WIDTH_POINTS, mWidthCells);
    mTotalCells = SQ(mWidthCells);
    mRoadData = std::make_unique<RoadGridCell[]>(mTotalCells);
    LOG_DEBUG("Road grid allocated {} mb data",
        (mTotalCells * sizeof(RoadGridCell)) / 1024.f / 1024.f);

}

RoadGrid::~RoadGrid() = default;

template <bool THREAD_SAFE>
RoadPoint RoadGrid::getRoadPoint(DTileCoord worldPos) const {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return RoadPoint();
    }
    i32v2 cellOffset;
    const ui32 cellId = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos.v, cellOffset);
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
DECL_BOOL_TEMPLATE(RoadPoint RoadGrid::getRoadPoint, (DTileCoord worldPos) const)

void RoadGrid::setRoadPoint(DTileCoord worldPos, RoadPoint point) {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return;
    }
    i32v2 cellOffset;
    const ui32 cellId = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos.v, cellOffset);
    RoadGridCell& cell = mRoadData[cellId];
    std::lock_guard lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<RoadPoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x] = point;
}

bool RoadGrid::setRoadPointIfHigherIntensity(DTileCoord worldPos, RoadPoint point) {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return false;
    }
    i32v2 cellOffset;
    const ui32 cellId = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos.v, cellOffset);
    RoadGridCell& cell = mRoadData[cellId];
    std::lock_guard lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<RoadPoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    RoadPoint& existing = cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x];
    if (point.strength > existing.strength || point.type != existing.type) {
        existing = point;
        return true;
    }
    return false;
}

void RoadGrid::adjustRoadPoint(DTileCoord worldPos, i32 adjust) {
    i32v2 cellOffset;
    const ui32 cellId = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos.v, cellOffset);
    RoadGridCell& cell = mRoadData[cellId];
    std::lock_guard lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<RoadPoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    RoadPoint& point = cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x];
    point.strength = (ui8)glm::clamp((i32)point.strength + adjust, 0, (i32)MAX_ROAD_STRENGTH);
}

// TODO: Remove
#include "resources/MaterialRepository.h"
MaterialID RoadGrid::getRoadMaterialFromType(ui8 type) const {
    // TODO: Data drive roads!
    return MaterialRepository::get().getMaterialId(CStrToken(/*"dirt_road"*/"window"));
}

