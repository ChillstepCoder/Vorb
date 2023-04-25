#pragma once

class BitArray;
class VisualLog;
struct GridEdge;

// Bits are 1 if they are part of the active shape
struct GridCell4x4 {
    union {
        struct {
            bool bottomRight : 1;
            bool bottomLeft : 1;
            bool topRight : 1; // This should be the iteration root for incEdgeLengths to be correct
            bool topLeft : 1;
        };
        ui8 data = 0;
    };

    // 1 Bits will be set to true
    void constructFrom2DBitArray(const BitArray& bitArray, i32 xPos, i32 yPos, i32 xDims, i32 yDims);
};
static_assert(sizeof(GridCell4x4) == 1);

class GridEdgeFinder
{
public:
    static Cartesian getNextCCWEdgeWalkDirFromGrid4x4(GridCell4x4 cell4x4, Cartesian prevDirection, OPT std::pair<bool /*IncPrev*/, bool /*IncNext*/>* incEdgeLengths);
    static std::vector<GridEdge> getInteriorEdgesFromOwnershipArray(const BitArray& ownershipBits, const ui32v2& dims, VisualLog* visLog, f32 vislogZ = 0.0f);
    static std::vector<TileIndex> getInteriorCounterClockwiseWalkFromGridEdges(const std::vector<GridEdge> gridEdges, const ui32v2& dims);
    //static CornerWinding getCornerTypeFromGrid4x4(GridCell4x4 cell4x4);
};

