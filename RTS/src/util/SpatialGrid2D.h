#pragma once
class SpatialGrid2D
{
public:
    SpatialGrid2D() {};
    SpatialGrid2D(ui32 cellWidth, ui32 gridWidthCells) : mCellWidth((i32)cellWidth), mGridWidthCells((i32)gridWidthCells) { }

    void init(ui32 cellWidth, ui32 gridWidthCells) { mCellWidth = (i32)cellWidth; mGridWidthCells = (i32)gridWidthCells; }

    ui32 getIDAtWorldPos(const i32v2& worldPos) const;
    i32v2 getWorldPosXYFromID(ui32 id) const;
    ui32 getSouthID(ui32 id) const { return id - mGridWidthCells; }
    ui32 getWestID(ui32 id) const { return id - 1; }
    ui32 getEastID(ui32 id) const { return id + 1; }
    ui32 getNorthID(ui32 id) const { return id + mGridWidthCells; }
    bool isIdValid(ui32 id) const { return id < SQ(mGridWidthCells); }
    bool isSentinelID(ui32 id) const;
    i32v2 getGridXYFromID(ui32 id) const;

private:
    i32 mCellWidth = 0;
    i32 mGridWidthCells = 0;
};

