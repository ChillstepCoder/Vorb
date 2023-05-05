#include "stdafx.h"
#include "SpatialGrid2D.h"

ui32 SpatialGrid2D::getIDAtWorldPos(const i32v2& worldPos) const {
    assert(worldPos.x >= 0 && worldPos.y >= 0);
    return ((int)worldPos.y / mCellWidth) * mGridWidthCells + (int)worldPos.x / mCellWidth;
}

i32v2 SpatialGrid2D::getWorldPosXYFromID(ui32 id) const {
    return i32v2((id % mGridWidthCells) * mCellWidth, (id / mGridWidthCells) * mCellWidth);
}

bool SpatialGrid2D::isSentinelID(ui32 id) const {
    const i32v2 gridXY = getGridXYFromID(id);
    return (gridXY.x == 0 || gridXY.y == 0 || gridXY.x == mGridWidthCells - 1 || gridXY.y == mGridWidthCells - 1);
}

i32v2 SpatialGrid2D::getGridXYFromID(ui32 id) const {
    return i32v2((id % mGridWidthCells), (id / mGridWidthCells));
}
