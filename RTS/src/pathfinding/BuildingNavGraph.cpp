#include "stdafx.h"
#include "BuildingNavGraph.h"
#include "city/Building.h"

BuildingNavGraph::BuildingNavGraph(Building& building) : mBuilding(building) {

}

BuildingNavGraph::~BuildingNavGraph() {
    delete[] mNavData;
}

typedef ui32 NavValue;
#define INVALID_NAV_VALUE UINT16_MAX;

void BuildingNavGraph::update() {
    delete[] mNavData;

    // We manually construct a blob of data that represents the full navgraph in flat format
    // HEADER: N ints (NavValue) where N is the size of mGraph, representing offsets for each node
    // Each node exists in order, first a short that represents parent index, will be INVALID_NAV_VALUE if root
    // next short represents how many children, followed by children indices in order

    // HEADER
    // NODES
    //   - ADJCOUNT, ADJACENTS [TileIndex, RoomID]
    mNumNodes = (ui32)mBuilding.mRooms.size();
    ui32 totalSize = mNumNodes * 2; // Header + size shorts
    for (auto&& room : mBuilding.mRooms) {
        // Accumulate
        totalSize += room.numAdjacentRooms * 2; // Two ints, one for tile index, one for room ID
    }

    mNavData = new ui32[totalSize];

    ui32 roomOffset = mNumNodes;
    for (ui32 i = 0; i < mNumNodes; ++i) {
        auto&& room = mBuilding.mRooms[i];
        // Set offset for start of this node
        mNavData[i] = roomOffset;
        // Number of children
        mNavData[roomOffset++] = room.numAdjacentRooms;
        // All children
        for (ui32 j = 0; j < room.numAdjacentRooms; ++j) {
            mNavData[roomOffset++] = room.adjacentRooms[j].tileIndex;
            mNavData[roomOffset++] = room.adjacentRooms[j].adjacentRoom;
        }
    }
}
