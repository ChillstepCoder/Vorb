#pragma once

#include "world/ChunkID.h"

class World;
class Chunk;
class TileContainer;
struct TileFineNavData;
struct CoarseNavNode;
struct TileWalls;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr int INVALID_NAV_NODE_INDEX = UINT16_MAX;

typedef ui32 DisjointSetNode;
typedef ui16 CoarseNavNodeIndex;

//const ui16v2 NAV_NODE_EDGE_OFFSETS[4] = {
//    {0, 0}, // DOWN
//    {0, 0}, // LEFT
//    {15, 0}, // RIGHT
//    {0, 15}  // UP
//};
//
//struct VerticalCoarseNavNodeEdge {
//    TileContainer* adjacentContainer;
//    TileIndex adjacentIndex;
//    bool isUp;
//};

// An edge determines where we can move OUT or IN to this chunk
// We can only traverse an EXTERNAL_EDGE if there is an adjacent edge on the other side
struct CoarseNavNodeEdge {
    TileIndex startPos;
    ui16 adjacentNodeIndex;
    Cartesian dir;
    ui8 edgeLength = 0;

    bool isExternalEdge() { return adjacentNodeIndex == INVALID_NAV_NODE_INDEX; }
};
static_assert(sizeof(CoarseNavNodeEdge) == 8, "Keep small");

struct CoarseNavNode {
    CoarseNavNodeEdge* edges = nullptr;
    ui32 tileContainerID = 0;
    ui16 edgeCount = 0;
    mutable bool isClosed = false; // For use in single threaded pathfinding
    // can have extra byte for flags field
};
static_assert(sizeof(CoarseNavNode) == 16, "Keep small");

struct CoarseNavNodeIndexPair {
    TileContainerID tileContainerID;
    CoarseNavNodeIndex index;
};
static_assert(sizeof(CoarseNavNodeIndexPair) == 8, "Keep small");

struct CoarseNavGraph {
    // Could fit another ui32 here
    TileContainer* parentContainer; // TODO: Is this needed?
    std::unique_ptr<CoarseNavNode[]> nodes;
    std::unique_ptr<CoarseNavNodeEdge[]> edges;
    ui32 numNodes = 0;
    ui32 numEdges = 0;

    const CoarseNavNode& getNode(ui32 nodeIndex) const {
        assert(nodeIndex < numNodes);
        return nodes[nodeIndex];
    }
};

struct TileEdgePointer {
    union {
        struct {
            ui32 bottom;
            ui32 left;
            ui32 right;
            ui32 up;
        };
        ui32 edges[4];
    };
};

class NavWorld
{
public:
    NavWorld(World& world);
    // TODO: async
    void buildNavGraphForContainer(TileContainer& tileContainer);

    void setFineNavEdgeCartesian(const TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, TileContainer& tileContainer, const f32 groundZPosition, TileFineNavData& fineNavData);

    void debugDrawNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId = 0) const;

    const CoarseNavGraph& getCoarseNavGraph(TileContainerID containerId) const {
        auto&& it = mNavGraphs.find(containerId);
        assert(it != mNavGraphs.end());
        return it->second;
    }
    const CoarseNavNode* getCoarseNavNode(CoarseNavNodeIndexPair index) const {
        return getCoarseNavNode(index.tileContainerID, index.index);
    }
    const CoarseNavNode* getCoarseNavNode(TileContainerID containerId, ui16 navNodeIndex) const {
        // TODO: what if invalid
        auto&& it = mNavGraphs.find(containerId);
        assert(it != mNavGraphs.end());
        const CoarseNavGraph& patch = it->second;
        assert(navNodeIndex < patch.numNodes);
        return &patch.nodes[navNodeIndex];
    }

private:

    //void buildEdges(TileContainer& tileContainer, const int cornerX, const int cornerY, const int zPos, const i32v2& subchunkDims, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, CoarseNavNodeIndex* navNodeIdTable, std::vector<CoarseNavNode>& navNodes, Cartesian dir);
    //void addNodeEdge(TileContainer& tileContainer, CoarseNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<CoarseNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir);

    // Return true if a new edge was made
    bool tryBuildEdge(const TileWalls& walls, const TileIndex index, const TileIndex prevIndex, const TileIndex outerIndex, TileContainer& tileContainer, const ui16 navNodeIndex, std::vector<TileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge);

    std::unordered_map<TileContainerID, CoarseNavGraph> mNavGraphs;
    World& mWorld;
};