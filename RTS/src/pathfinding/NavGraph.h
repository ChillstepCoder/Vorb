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
    NavNode(Chunk& chunk) : chunk(chunk) {}
    // TODO: Experiment with static array, max size is 152 edges in worst case? prob not cache efficient...
    // TODO: Memory pool?
    Chunk& chunk;
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
    void buildNavNodesForChunkAsync(Chunk& chunk);
    void debugDrawNavGraphForChunk(const Chunk& chunk, ui32 lifetime, int debugId = 0) const;

    const NavNode* getNode(NavNodeIndexPair index) const {
        return &mNodes[index.chunkId][index.index];
    }

private:
    void buildEdges(Chunk& chunk, const int cornerX, const int cornerY, DisjointSetNode* djNodes, ui32* djNodeIDs, NavNodeIndex* navNodeIdTable, std::vector<NavNode>& navNodes, Cartesian dir);
    void addNodeEdge(Chunk& chunk, NavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<NavNode>& navNodes, TileIndex start, int length, Cartesian dir);

    std::vector<NavNode> mNodes[WorldData::WORLD_SIZE_CHUNKS];
    World& mWorld;
};
