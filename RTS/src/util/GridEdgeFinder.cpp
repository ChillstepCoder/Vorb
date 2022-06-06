#include "stdafx.h"
#include "GridEdgeFinder.h"

#include "util/BitArray.h"

#include "debugging/VisualLogger.h"

constexpr ui32 CORNER_TABLE_SIZE = 16; // 4^2
Cartesian sCornerNextEdgeLookupTable[CORNER_TABLE_SIZE];
// We increment lengths of previous edges based on entry directions and shape
std::pair<bool /*IncPrev*/, bool /*IncNext*/> sIncrementEdgeLengthsLookupTable[CORNER_TABLE_SIZE];
//CornerWinding sCornerTypeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];

RUNTIME_INIT_FUNC(initEdgeLookup) {
    // Zero table
    for (ui32 i = 0; i < CORNER_TABLE_SIZE; ++i) {
        sCornerNextEdgeLookupTable[i] = Cartesian::INVALID;
        sIncrementEdgeLengthsLookupTable[i] = std::make_pair(false, false);
    }
    // Set up corners shapes, we move counter clockwise always
    // 0 1
    // 0 0
    sCornerNextEdgeLookupTable[0b0100] = Cartesian::SOUTH;
    // 1 0
    // 1 1
    sCornerNextEdgeLookupTable[0b1011] = Cartesian::EAST;
    sIncrementEdgeLengthsLookupTable[0b1011] = std::make_pair(true, true);
    // 1 0
    // 0 0
    sCornerNextEdgeLookupTable[0b1000] = Cartesian::EAST;
    // 0 1
    // 1 1
    sCornerNextEdgeLookupTable[0b0111] = Cartesian::NORTH;
    sIncrementEdgeLengthsLookupTable[0b0111] = std::make_pair(true, true);
    // 0 0
    // 1 0
    sCornerNextEdgeLookupTable[0b0010] = Cartesian::NORTH;
    sIncrementEdgeLengthsLookupTable[0b0111] = std::make_pair(false, true);
    // 1 1
    // 0 1
    sCornerNextEdgeLookupTable[0b1101] = Cartesian::WEST;
    sIncrementEdgeLengthsLookupTable[0b1101] = std::make_pair(true, true);
    // 0 0
    // 0 1
    sCornerNextEdgeLookupTable[0b0001] = Cartesian::WEST;
    // 1 1
    // 1 0
    sCornerNextEdgeLookupTable[0b1110] = Cartesian::SOUTH;
    sIncrementEdgeLengthsLookupTable[0b1110] = std::make_pair(true, true);
    // Diagonal edge cases
    // 1 0
    // 0 1
    sCornerNextEdgeLookupTable[0b1001] = Cartesian::NONE;
    sCornerNextEdgeLookupTable[0b0110] = Cartesian::NONE;
    sIncrementEdgeLengthsLookupTable[0b0110] = std::make_pair(true, false);
}

Cartesian GridEdgeFinder::getNextEdgeDirFromGrid4x4(GridCell4x4 cell4x4, Cartesian prevDirection, OPT std::pair<bool /*IncPrev*/, bool /*IncNext*/>* incEdgeLengths) {
    Cartesian dir = sCornerNextEdgeLookupTable[cell4x4.data];
    if (incEdgeLengths) {
        *incEdgeLengths = sIncrementEdgeLengthsLookupTable[cell4x4.data];
    }

    if (dir == Cartesian::NONE) {
        // Branch on these special corners based on where we were coming from (Counter clockwise)
        if (cell4x4.data == 0b1001) {
            // 1 0
            // 0 1
            if (prevDirection == Cartesian::EAST) {
                dir = Cartesian::SOUTH;
            }
            else if (prevDirection == Cartesian::WEST) {
                dir = Cartesian::NORTH;
            }
            else {
                assert(false);
            }
        }
        else {
            // 0 1
            // 1 0
            if (prevDirection == Cartesian::WEST) {
                dir = Cartesian::SOUTH;
            }
            else if (prevDirection == Cartesian::EAST) {
                dir = Cartesian::NORTH;
            }
            else {
                assert(false);
            }
        }
    }

    return dir;
}

TileIndex getTileIndex(ui32 x, ui32 y, const ui32v2& dims) {
    return y * dims.x + x;
}

TileIndex getTileIndex(i32v2 xy, const ui32v2& dims) {
    return xy.y * dims.x + xy.x;
}

i32v2 getPosFromTileIndex(TileIndex index, const ui32v2& dims) {
    return i32v2(index % dims.x, index / dims.x);
}

std::vector<GridEdge> GridEdgeFinder::getInteriorEdgesFromOwnershipArray(const BitArray& ownershipBits, const ui32v2& dims, VisualLog* visLog, f32 vislogZ /*= 0.0f*/) {
    assert(ownershipBits.getNumBits() == (size_t)dims.x * dims.y);
    std::vector<GridEdge> edges;
    // Find first bottom left owned bit to begin iteration
    TileIndex tileIndex;
    bool found = false;
    for (tileIndex = 0; tileIndex < ownershipBits.getNumBits(); ++tileIndex) {
        if (ownershipBits.getBit(tileIndex)) {
            found = true;
            break;
        }
    }
    if (!found) {
        return edges;
    }

    edges.reserve(4); // Every room has at least 4

    GridEdge* currentEdge = &edges.emplace_back();
    currentEdge->start = tileIndex;
    currentEdge->edgeDir = Cartesian::SOUTH; // We are always south first based on how we found this corner
    currentEdge->length = 0;
    // Sometimes certain corner shapes need us to increment prev or next edge lengths to get a proper interior edge
    std::pair<bool, bool> incEdgeLengths;
    i32v2 tilePos = getPosFromTileIndex(tileIndex, dims);
    const TileIndex startTileIndex = tileIndex;
    do {
        
        // Now walk the edges
        GridCell4x4 gridCell4x4;
        gridCell4x4.topLeft = (tilePos.x == 0 || tilePos.y == dims.y) ? 0 : ownershipBits.getBit(tileIndex - 1);
        gridCell4x4.topRight = (tilePos.x == dims.x || tilePos.y == dims.y) ? 0 : ownershipBits.getBit(tileIndex);
        gridCell4x4.bottomLeft = (tilePos.x == 0 || tilePos.y == 0) ? 0 : ownershipBits.getBit(tileIndex - 1 - dims.x);
        gridCell4x4.bottomRight = (tilePos.x == dims.x || tilePos.y == 0) ? 0 : ownershipBits.getBit(tileIndex - dims.x);

        if (!gridCell4x4.data) {
            //assert(false && "Must be nonzero or we walked off the edge");
            break;
        }

        Cartesian nextDir = GridEdgeFinder::getNextEdgeDirFromGrid4x4(gridCell4x4, currentEdge->edgeDir, &incEdgeLengths);
        if (nextDir != Cartesian::INVALID && nextDir != currentEdge->edgeDir) {
            if (incEdgeLengths.first) ++currentEdge->length;
            // End prev
            if (currentEdge->length == 0) {
                // Zero length edges are destroyed
                // TODO: Can this even happen?
                edges.pop_back();
            }
            else {
                currentEdge->end = tileIndex;
            }
            // New edge
            currentEdge = &edges.emplace_back();
            currentEdge->start = tileIndex;
            currentEdge->edgeDir = nextDir;

            if (incEdgeLengths.second) {
                // Increment edge length and step the start backwards
                ++currentEdge->length;
                i32v2 pos = getPosFromTileIndex(currentEdge->start, dims);
                currentEdge->start = getTileIndex(i32v2(pos - CARTESIAN_EDGE_DIRS_COUNTER_CLOCKWISE[e_cast(currentEdge->edgeDir)]), dims);
            }
        }
        else {
            // Keep going along the edge
            ++currentEdge->length;
        }
        tilePos += CARTESIAN_EDGE_DIRS_COUNTER_CLOCKWISE[e_cast(currentEdge->edgeDir)];
        tileIndex = getTileIndex(tilePos, dims);
        if (visLog) {
            visLog->addWireQuad(f32v3(tilePos.x, tilePos.y, vislogZ), f32v2(1.0f),
                color4((f32)(currentEdge->edgeDir == Cartesian::EAST || currentEdge->edgeDir == Cartesian::WEST),
                    (f32)(currentEdge->edgeDir == Cartesian::NORTH),
                    (f32)(currentEdge->edgeDir == Cartesian::NORTH || currentEdge->edgeDir == Cartesian::WEST || currentEdge->edgeDir == Cartesian::SOUTH),
                    0.9f
                )
            );
        }

    } while (!(tileIndex == startTileIndex/* && currentEdge->edgeDir != Cartesian::SOUTH*/)); // While we aren't equal to the start edge

    // End last edge
    currentEdge->end = tileIndex;
    // Fixup all edges since east and north edges are along walls
    for (auto&& edge : edges) {
        if (edge.edgeDir == Cartesian::EAST) {
            // -1x
            --edge.start;
            --edge.end;
        }
        else if (edge.edgeDir == Cartesian::NORTH) {
            // -1y
            edge.start -= dims.x;
            edge.end -= dims.x;
            --edge.start;
        }
        else if (edge.edgeDir == Cartesian::WEST) {
            edge.start -= dims.x;
        }
    }
    return edges;
}

std::vector<TileIndex> GridEdgeFinder::getInteriorCounterClockwiseWalkFromGridEdges(const std::vector<GridEdge> gridEdges, const ui32v2& dims) {
    BitArray addedBits;
    addedBits.resizeAndZero(dims.x * dims.y);
    std::vector<TileIndex> edgeWalk;
    edgeWalk.reserve(8); // Most edges have at least 8

    for (size_t i = 0; i < gridEdges.size(); ++i) {
        const GridEdge& edge = gridEdges[i];
        const i32v2 startPos = getPosFromTileIndex(edge.start, dims);
        for (i32 j = 0; j < (i32)edge.length; ++j) {
            TileIndex tileIndex = getTileIndex(startPos + CARTESIAN_EDGE_DIRS_COUNTER_CLOCKWISE[e_cast(edge.edgeDir)] * j, dims);
            if (!addedBits.getBit(tileIndex)) {
                addedBits.setBit(tileIndex);
                edgeWalk.push_back(tileIndex);
            }
        }
    }
    return edgeWalk;
}

//CornerWinding GridEdgeFinder::getCornerTypeFromGrid4x4(GridCell4x4 cell4x4) {
//    return sCornerTypeLookupTable[cell4x4.data];
//}
