#pragma once

#include "world/ChunkID.h"

class World;
class Chunk;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr int INVALID_NAV_NODE_INDEX = UINT16_MAX;

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
static_assert(sizeof(NavNode) == 40, "Keep small");

struct LiteNavNodeEdge {
    ui8 length : 4;
    ui8 start : 4;
};
static_assert(sizeof(LiteNavNodeEdge) == sizeof(ui8), "Must be single byte");

struct NavNode2 {
    ui32 chunkId;
    ui8 numBottom;
    LiteNavNodeEdge bottomEdges[8];
    ui8 numLeft;
    LiteNavNodeEdge leftEdges[8];
    ui8 numRight;
    LiteNavNodeEdge rightEdges[8];
    ui8 numTop;
    LiteNavNodeEdge topEdges[8];
};
static_assert(sizeof(NavNode2) == 40, "Keep small");

struct NavNodeIndexPair {
    ui32 chunkId;
    NavNodeIndex index;
};
static_assert(sizeof(NavNodeIndexPair) == 8, "Keep small");

struct NavPatch {
    ui32 size;
    NavNode* nodes;
};

class NavGraph
{
public:
    NavGraph(World& world);
    // TODO: async
    void buildNavNodesForChunkSynchronous(Chunk& chunk);
    void buildNavNodesForChunkAsync(Chunk& chunk);
    void debugDrawNavGraphForChunk(const Chunk& chunk, ui32 lifetime, int debugId = 0) const;

    const NavNode* getNode(NavNodeIndexPair index) const {
        return &mPatches[index.chunkId].nodes[index.index];
    }

private:
    void buildEdges(Chunk& chunk, const int cornerX, const int cornerY, DisjointSetNode* djNodes, ui32* djNodeIDs, NavNodeIndex* navNodeIdTable, std::vector<NavNode>& navNodes, Cartesian dir);
    void addNodeEdge(Chunk& chunk, NavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<NavNode>& navNodes, TileIndex start, int length, Cartesian dir);

    NavPatch mPatches[WorldData::WORLD_SIZE_CHUNKS];
    World& mWorld;
};