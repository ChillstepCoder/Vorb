#pragma once

#include "world/ChunkID.h"
#include "CoarseNavGraph.h"

class Chunk;
class TileContainer;
struct TileFineNavData;
struct CoarseNavNode;
struct TileWalls;
struct TileHandle;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr ui16 INVALID_DJ_NODE_ID = UINT16_MAX;


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

// An edge determines where we can move OUT or IN to this nav node
// We can only traverse an EXTERNAL_EDGE if there is an adjacent edge on the other side

struct TileEdgePointer {
    TileEdgePointer() : bottom(UINT32_MAX), left(UINT32_MAX), right(UINT32_MAX), up(UINT32_MAX) {}
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
    NavWorld();

    // TODO: async
    void buildNavGraphForContainer(TileContainer& tileContainer, OUT CoarseNavGraph& navGraph, OUT NavGraphTileDataToCopy& navTileData);

    void setFineNavEdgeCartesian(TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, TileContainer& tileContainer, const f32 groundZPosition, TileFineNavData& fineNavData, int prevZ);
    void setFineNavEdgeCartesianDiagonal(const TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, TileContainer& tileContainer, const f32 groundZPosition, TileFineNavData& fineNavData);

    // ========== Debug drawing ==========
    void debugDrawCoarseNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId = 0) const;
    void debugDrawFineNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId = 0) const;
    void debugDrawCoarseNavNode(const TileHandle& tileHandle, ui32 lifetime, int debugId = 0) const;

    const CoarseNavGraph* tryGetCoarseNavGraph(TileContainerID containerId) const;
    const CoarseNavGraph& getCoarseNavGraph(TileContainerID containerId) const;
    void assignCoarseNavGraph(TileContainerID containerId, CoarseNavGraph&& navGraph);

    const CoarseNavNode* getCoarseNavNode(CoarseNavNodeIndexPair index) const;
    const CoarseNavNode* getCoarseNavNode(TileContainerID containerId, ui16 navNodeIndex) const;

private:

    //void buildEdges(TileContainer& tileContainer, const int cornerX, const int cornerY, const int zPos, const i32v2& subchunkDims, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, CoarseNavNodeIndex* navNodeIdTable, std::vector<CoarseNavNode>& navNodes, Cartesian dir);
    //void addNodeEdge(TileContainer& tileContainer, CoarseNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<CoarseNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir);

    // Return true if a new edge was made
    bool tryBuildEdge(NavGraphTileDataToCopy& navTileData, const TileFineNavData& fineNavData, const TileIndex index, const TileIndex prevIndex, TileIndex outerIndex, TileContainer& tileContainer, const ui16 navNodeIndex, std::vector<TileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge);

    // TODO: We need to destroy these on chunk destruct
    std::unordered_map<TileContainerID, CoarseNavGraph> mNavGraphs;
};