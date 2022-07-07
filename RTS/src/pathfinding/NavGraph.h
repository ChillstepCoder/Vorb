#pragma once

#include "world/ChunkID.h"

class World;
class Chunk;
struct TerrainNavNode;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr int INVALID_NAV_NODE_INDEX = UINT16_MAX;

typedef ui16 TerrainNavNodeIndex;
struct DisjointSetNode;

const ui16v2 NAV_NODE_EDGE_OFFSETS[4] = {
    {0, 0}, // DOWN
    {0, 0}, // LEFT
    {15, 0}, // RIGHT
    {0, 15}  // UP
};

struct LiteTerrainNavNodeEdge {
    ui8 lengthMinusOne : 4;
    ui8 start : 4;
};
static_assert(sizeof(LiteTerrainNavNodeEdge) == sizeof(ui8), "Must be single byte");

struct TerrainNavNode {
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
        LiteTerrainNavNodeEdge edges[4][8];
        struct {
            LiteTerrainNavNodeEdge bottomEdges[8];
            LiteTerrainNavNodeEdge leftEdges[8];
            LiteTerrainNavNodeEdge rightEdges[8];
            LiteTerrainNavNodeEdge topEdges[8];
        };
    };
    mutable bool isClosed; // For use in single threaded pathfinding
};
static_assert(sizeof(TerrainNavNode) == 48, "Keep small");

struct TerrainNavNodeIndexPair {
    ui32 chunkId;
    TerrainNavNodeIndex index;
};
static_assert(sizeof(TerrainNavNodeIndexPair) == 8, "Keep small");

struct TerrainNavPatch {
    // Could fit another ui32 here
    ui32 size = 0;
    TerrainNavNode* nodes = nullptr;
};

class NavGraph
{
public:
    NavGraph(World& world);
    // TODO: async
    void buildNavNodesForChunk(Chunk& chunk);
    void debugDrawNavGraphForChunk(const Chunk& chunk, ui32 lifetime, int debugId = 0) const;

    const TerrainNavNode* getNode(TerrainNavNodeIndexPair index) const {
        const TerrainNavPatch& patch = mTerrainPatches[index.chunkId];
        assert(index.index < patch.size);
        return &patch.nodes[index.index];
    }

private:
    void buildEdges(Chunk& chunk, const int cornerX, const int cornerY, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, TerrainNavNodeIndex* navNodeIdTable, std::vector<TerrainNavNode>& navNodes, Cartesian dir);
    void addNodeEdge(Chunk& chunk, TerrainNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<TerrainNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir);

    TerrainNavPatch mTerrainPatches[WorldData::WORLD_SIZE_CHUNKS];
    World& mWorld;
};