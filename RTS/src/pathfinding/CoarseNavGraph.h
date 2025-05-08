#pragma once

#include "tile/TileHarvestable.h"
#include "util/BitArray.h"

typedef ui16 DisjointSetNode;
struct NavGraphTileDataToCopy {
    std::vector<DisjointSetNode> djNodes;
    std::vector<ui16> tileDjNodeIDs;
};

typedef ui16 CoarseNavNodeIndex;

constexpr int INVALID_NAV_NODE_INDEX = UINT16_MAX;

enum class TileCoarseNavEdgeType : ui8 {
    NONE = 0,
    DOWN = 1,
    UP = 2,
    EXTERIOR = 3,
};

struct CoarseNavNodeEdge {
    TileIndex startPos;
    ui16 adjacentNodeIndex;
    struct {
        Cartesian dir : 4;
        TileCoarseNavEdgeType edgeType : 4;
    };
    ui8 edgeLength = 0;

    bool isExternalEdge() const { return adjacentNodeIndex == INVALID_NAV_NODE_INDEX; }
};
static_assert(sizeof(CoarseNavNodeEdge) == 8, "Keep small");

struct CoarseNavNode {
    ui32 edgesStart = UINT32_MAX;
    //ui32 tileContainerID = 0;
    ui16 edgeCount = 0;
    mutable bool isClosed = false; // For use in single threaded pathfinding
    // can have extra byte for flags field
};
static_assert(sizeof(CoarseNavNode) == 8, "Keep small");

struct CoarseNavNodeIndexPair {
    TileContainerID tileContainerID;
    CoarseNavNodeIndex index;
};
static_assert(sizeof(CoarseNavNodeIndexPair) == 8, "Keep small");

// For checking if a nav node has a harvestable
class CoarseNavGraphHarvestablesLookup {
public:

    void init(ui32 numNodes) {
        mHarvestablesTest.resize(numNodes * (ui32)TileHarvestable::COUNT);
    }
    const std::set<TileIndex>* tryGetHarvestables(ui32 nodeIndex, TileHarvestable harvestable) const {
        // O(1) membership test
        if (mHarvestablesTest.getBit(nodeIndex * (ui32)TileHarvestable::COUNT + (ui32)harvestable)) {
            return &(mHarvestablePositions.find(std::make_pair(nodeIndex, harvestable))->second);
        }
        return nullptr;
    }
    void setNodeHarvestable(ui32 nodeIndex, TileHarvestable harvestable, TileIndex position) {
        mHarvestablesTest.setBit(nodeIndex * (ui32)TileHarvestable::COUNT + (ui32)harvestable);
        mHarvestablePositions[std::make_pair(nodeIndex, harvestable)].insert(position);
    }

private:
    BitArray mHarvestablesTest;
    // TODO: test flatSet?
    FlatMap<std::pair<ui32 /*nodeIndex*/, TileHarvestable>, std::set<TileIndex>> mHarvestablePositions;
};

struct CoarseNavGraph {
    //TileContainer* parentContainer; // TODO: Is this needed?
    std::unique_ptr<CoarseNavNodeIndex[]> tileCoarseNavIndices; // Size = tile container size
    std::unique_ptr<CoarseNavNode[]> nodes;
    std::unique_ptr<CoarseNavNodeEdge[]> edges;
    CoarseNavGraphHarvestablesLookup harvestablesLookup;
    //  std::unique_ptr<ui32 subchunkIndex> = UINT32_MAX; WE ONLY NEED SUBCHUNK INDEX FOR  RESOURCES
    ui32 numNodes = 0;
    ui32 numEdges = 0;

    const CoarseNavNode& getNode(ui32 nodeIndex) const {
        assert(nodeIndex < numNodes);
        return nodes[nodeIndex];
    }
};
