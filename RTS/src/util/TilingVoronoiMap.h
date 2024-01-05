#pragma once
class TilingVoronoiMap {
public:
    TilingVoronoiMap(ui32 width, ui32 numCells);
    i32v2 getVoronoiPointAtTile(i32v2 tilePosWorld, float voronoiScale);

private:
    std::vector<ui8> mVoronoiCellLookup; // SQ(mWidth)
    std::vector<i32v2> mVoronoiCellCenters;
    ui32 mWidth;
};

