#include "stdafx.h"
#include "TerrainSurfaceGrid.h"

// For ROAD_VERTEX_STRIDE
#include "world/TerrainConstants.h"

TerrainSurfaceGrid::TerrainSurfaceGrid(ui32 worldWidthTiles) : mWorldWidthDTiles(worldWidthTiles / 2) {
    mWidthCells = worldWidthTiles / ROAD_GRID_CELL_WIDTH_POINTS;
    mSpatialGrid.init(ROAD_GRID_CELL_WIDTH_POINTS, mWidthCells);
    mTotalCells = SQ(mWidthCells);
    mRoadData = std::make_unique<Cell[]>(mTotalCells);
    LOG_DEBUG("Terrain surface grid allocated {} mb data",
        (mTotalCells * sizeof(Cell)) / 1024.f / 1024.f);

    static_assert(e_count(TerrainSurfaceType) == 3);

}

TerrainSurfaceGrid::~TerrainSurfaceGrid() = default;

template <bool THREAD_SAFE>
TerrainSurfacePoint TerrainSurfaceGrid::getSurfacePoint(DTileCoord worldPos) const {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return TerrainSurfacePoint();
    }
    const auto [cellId, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(worldPos.v);
    Cell& cell = mRoadData[cellId];
    if constexpr (THREAD_SAFE) {
        std::shared_lock lock(cell.mutex);
        if (cell.points == nullptr) {
            return TerrainSurfacePoint();
        }
        return cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x];
    } else {
        if (cell.points == nullptr) {
            return TerrainSurfacePoint();
        }
        return cell.points[cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x];
    }
}
DECL_BOOL_TEMPLATE(TerrainSurfacePoint TerrainSurfaceGrid::getSurfacePoint, (DTileCoord worldPos) const)

void TerrainSurfaceGrid::setSurfacePoint(DTileCoord worldPos, TerrainSurfacePoint point) {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return;
    }
    const auto [cellId, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(worldPos.v);
    Cell& cell = mRoadData[cellId];
    const i32 pointIndex = cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x;

    std::lock_guard lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<TerrainSurfacePoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    cell.points[pointIndex] = point;
}

void TerrainSurfaceGrid::setBaseSurfaceType(DTileCoord worldPos, TerrainSurfaceType type) {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return;
    }
    const auto [cellId, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(worldPos.v);
    Cell& cell = mRoadData[cellId];
    const i32 pointIndex = cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x;

    std::lock_guard lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<TerrainSurfacePoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    cell.points[pointIndex].baseType = type;
}

bool TerrainSurfaceGrid::setBaseSurfaceTypeIfEmpty(DTileCoord worldPos, TerrainSurfaceType type) {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return false;
    }
    const auto [cellId, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(worldPos.v);
    Cell& cell = mRoadData[cellId];
    const i32 pointIndex = cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x;

    std::lock_guard lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<TerrainSurfacePoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    if (cell.points[pointIndex].baseType == TerrainSurfaceType::None) {
        cell.points[pointIndex].baseType = type;
        return true;
    }
    return false;
}

bool TerrainSurfaceGrid::trySetOverlaySurfaceType(DTileCoord worldPos, TerrainSurfaceOverlayType type) {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorldWidthDTiles || worldPos.y >= mWorldWidthDTiles) [[unlikely]] {
        return false;
    }
    const auto [cellId, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(worldPos.v);
    Cell& cell = mRoadData[cellId];
    const i32 pointIndex = cellOffset.y * ROAD_GRID_CELL_WIDTH_POINTS + cellOffset.x;

    std::lock_guard lock(mRoadData[cellId].mutex);
    if (cell.points == nullptr) {
        // Lazy allocate to reduce memory usage
        cell.points = std::make_unique<TerrainSurfacePoint[]>(ROAD_GRID_CELL_SIZE_POINTS);
    }
    cell.points[pointIndex].overlayType = type;
    return true;
}
