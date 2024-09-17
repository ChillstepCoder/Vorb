#pragma once

// Linearizes a 2D grid, allowing for ID queries
class SpatialGrid2D
{
public:
    SpatialGrid2D() {};
    SpatialGrid2D(i32 cellWidth, i32 gridWidthCells) : mCellWidth((i32)cellWidth), mGridWidthCells((i32)gridWidthCells) { }

    void init(i32 cellWidth, i32 gridWidthCells) { mCellWidth = (i32)cellWidth; mGridWidthCells = (i32)gridWidthCells; }

    i32 getIDAtPos(i32v2 worldPos) const;
    // Returns the ID and the offset into the cell
    i32 getIDAndCellOffsetAtPos(i32v2 worldPos, OUT i32v2& offsetIntoCell) const;
    // Gets the true position of the cell at the given ID
    i32v2 getPosFromID(i32 id) const;
    i32 getSouthID(i32 id) const { return id - mGridWidthCells; }
    i32 getWestID(i32 id) const { return id - 1; }
    i32 getEastID(i32 id) const { return id + 1; }
    i32 getNorthID(i32 id) const { return id + mGridWidthCells; }
    bool isIdValid(i32 id) const { return id < SQ(mGridWidthCells); }
    bool isSentinelID(i32 id) const;
    i32v2 getCellCoordsFromID(i32 id) const;
    i32 getIDfromCellCoords(i32v2 cellCoord) const;

    inline i32 getCellWidth() const { return mCellWidth; }
    inline i32 getGridWidthCells() const { return mGridWidthCells; }
    inline i32 getGridSizeCells() const { return SQ(mGridWidthCells); }
private:
    i32 mCellWidth = 0;
    i32 mGridWidthCells = 0;
};

