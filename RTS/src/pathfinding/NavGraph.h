#pragma once

#include "world/ChunkID.h"

class World;
class Chunk;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr int INVALID_NAV_NODE_INDEX = UINT16_MAX;

typedef ui16 NavNodeIndex;
struct DisjointSetNode;

constexpr cui32v2 NAV_NODE_EDGE_OFFSETS[4] = {
    {0, 0}, // DOWN
    {0, 0}, // LEFT
    {15, 0}, // RIGHT
    {0, 15}  // UP
};

struct LiteNavNodeEdge {
    ui8 lengthMinusOne : 4;
    ui8 start : 4;
};
static_assert(sizeof(LiteNavNodeEdge) == sizeof(ui8), "Must be single byte");

struct NavNode {
    ui32 chunkId;
    TileIndex cornerPos;
    union {
        ui8 counts[4];
        struct {
            ui8 numBottom;
            ui8 numLeft;
            ui8 numRight;
            ui8 numTop;
        };
    };
    union {
        LiteNavNodeEdge edges[4][8];
        struct {
            LiteNavNodeEdge bottomEdges[8];
            LiteNavNodeEdge leftEdges[8];
            LiteNavNodeEdge rightEdges[8];
            LiteNavNodeEdge topEdges[8];
        };
    };
    mutable bool isClosed; // For use in single threaded pathfinding
};
static_assert(sizeof(NavNode) == 44, "Keep small");

struct NavNodeIndexPair {
    ui32 chunkId;
    NavNodeIndex index;
};
static_assert(sizeof(NavNodeIndexPair) == 8, "Keep small");

struct NavPatch {
    // Could fit another ui32 here
    ui32 size = 0;
    NavNode* nodes = nullptr;
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
    void buildEdges(Chunk& chunk, const int cornerX, const int cornerY, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, NavNodeIndex* navNodeIdTable, std::vector<NavNode>& navNodes, Cartesian dir);
    void addNodeEdge(Chunk& chunk, NavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<NavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir);

    NavPatch mPatches[WorldData::WORLD_SIZE_CHUNKS];
    World& mWorld;
};