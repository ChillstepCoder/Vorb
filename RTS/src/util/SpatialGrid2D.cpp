#include "stdafx.h"
#include "SpatialGrid2D.h"

i32 SpatialGrid2D::getIDAtPos(i32v2 worldPos) const {
    assert(worldPos.x >= 0 && worldPos.y >= 0);
    return (worldPos.y / mCellWidth) * mGridWidthCells + worldPos.x / mCellWidth;
}

std::pair<i32, i32v2> SpatialGrid2D::getIDAndCellOffsetAtPos(i32v2 worldPos) const {
    const i32v2 cellXY = worldPos / mCellWidth;
    const i32 id = cellXY.y * mGridWidthCells + cellXY.x;
    return std::make_pair(id, worldPos - cellXY * mCellWidth);
}

i32v2 SpatialGrid2D::getPosFromID(i32 id) const {
    return i32v2((id % mGridWidthCells) * mCellWidth, (id / mGridWidthCells) * mCellWidth);
}

bool SpatialGrid2D::isSentinelID(i32 id) const {
    const i32v2 cellCoord = getCellCoordsFromID(id);
    return (cellCoord.x == 0 || cellCoord.y == 0 || cellCoord.x == mGridWidthCells - 1 || cellCoord.y == mGridWidthCells - 1);
}

i32v2 SpatialGrid2D::getCellCoordsFromID(i32 id) const {
    return i32v2((id % mGridWidthCells), (id / mGridWidthCells));
}

i32 SpatialGrid2D::getIDfromCellCoords(i32v2 cellCoord) const {
    return cellCoord.y * mGridWidthCells + cellCoord.x;
}
