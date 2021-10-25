#pragma once

#include "world/ChunkID.h"

class World;
class Chunk;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr int INVALID_NAV_NODE_INDEX = UINT8_MAX;

typedef ui16 NavNodeIndex;
struct DisjointSetNode;

struct NavNodeEdge {
    NavNodeEdge(TileIndex start, ui8 length, Cartesian dir) : start(start), length(length), dir(dir) {};
    TileIndex start;
    ui8 length;
    Cartesian dir;
    // ui8 isDoor/flags/dir
};
static_assert(sizeof(NavNodeEdge) == 4, "Keep small");

struct NavNode {
    // TODO: Experiment with static array, max size is 152 edges in worst case? prob not cache efficient...
    // TODO: Memory pool?
    std::vector<NavNodeEdge> edges;
};

struct NavNodeIndexPair {
    ui32 chunkId;
    NavNodeIndex index;
};
static_assert(sizeof(NavNodeIndexPair) == 8, "Keep small");

class NavGraph
{
public:
    NavGraph(World& world);
    // TODO: async
    void buildNavNodesForChunkSynchronous(Chunk& chunk);

    const NavNode* tryGetNode(NavNodeIndexPair index) const {
        auto&& it = mNodes.find(index.chunkId);
        if (it == mNodes.end()) return nullptr;
        return &it->second[index.index];
    }

private:
    void buildEdges(Chunk& chunk, const int cornerX, const int cornerY, DisjointSetNode* djNodes, ui32* djNodeIDs, ui16* navNodeIdTable, std::vector<NavNode>& navNodes, Cartesian dir);
    void addNodeEdge(ui16* navNodeIdTable, const ui32 djIndex, std::vector<NavNode>& navNodes, TileIndex start, int length, Cartesian dir);

    std::map<ui32 /* chunkId */, std::vector<NavNode>> mNodes;
    //std::unordered_map<NavNodeEdge, std::vector<NavNodeIndex>> mEdgeLookup;
    World& mWorld;
};
