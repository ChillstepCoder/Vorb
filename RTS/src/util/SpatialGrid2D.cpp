#include "stdafx.h"
#include "SpatialGrid2D.h"

ui32 SpatialGrid2D::getIDAtWorldPos(i32v2 worldPos) const {
    assert(worldPos.x >= 0 && worldPos.y >= 0);
    return (worldPos.y / mCellWidth) * mGridWidthCells + worldPos.x / mCellWidth;
}

ui32 SpatialGrid2D::getIDAndCellOffsetAtWorldPos(i32v2 worldPos, OUT i32v2& cellOffsetTiles) const {
    const i32v2 cellXY = worldPos / mCellWidth;
    cellOffsetTiles = worldPos - cellXY * mCellWidth;
    const ui32 id = cellXY.y * mGridWidthCells + cellXY.x;
    return id;
}

i32v2 SpatialGrid2D::getWorldPosXYFromID(ui32 id) const {
    return i32v2(((i32)id % mGridWidthCells) * mCellWidth, ((i32)id / mGridWidthCells) * mCellWidth);
}

bool SpatialGrid2D::isSentinelID(ui32 id) const {
    const i32v2 gridXY = getGridXYFromID(id);
    return (gridXY.x == 0 || gridXY.y == 0 || gridXY.x == mGridWidthCells - 1 || gridXY.y == mGridWidthCells - 1);
}

i32v2 SpatialGrid2D::getGridXYFromID(ui32 id) const {
    return i32v2(((i32)id % mGridWidthCells), ((i32)id / mGridWidthCells));
}

ui32 SpatialGrid2D::getIDfromGridXY(i32v2 gridXY) const {
    return gridXY.y * mGridWidthCells + gridXY.x;
}
