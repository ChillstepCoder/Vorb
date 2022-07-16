#pragma once

#include "world/ChunkID.h"

class World;
class Chunk;
class TileContainer;
struct CoarseNavNode;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr int INVALID_NAV_NODE_INDEX = UINT16_MAX;

typedef ui16 CoarseNavNodeIndex;
struct DisjointSetNode;

const ui16v2 NAV_NODE_EDGE_OFFSETS[4] = {
    {0, 0}, // DOWN
    {0, 0}, // LEFT
    {15, 0}, // RIGHT
    {0, 15}  // UP
};

struct LiteCoarseNavNodeEdge {
    ui8 lengthMinusOne : 4;
    ui8 start : 4;
};
static_assert(sizeof(LiteCoarseNavNodeEdge) == sizeof(ui8), "Must be single byte");

struct VerticalCoarseNavNodeEdge {
    TileContainer* adjacentContainer;
    TileIndex adjacentIndex;
    bool isUp;
};

struct CoarseNavNode {
    std::vector<VerticalCoarseNavNodeEdge> verticalEdges; // TODO: Compress, pool, (use boost?)
    ui32 tileContainerID; // TODO: ContainerID?
    TileIndex cornerPos;
    union {
        ui8 counts[4];
        struct {
            ui8 numSouth;
            ui8 numWest;
            ui8 numEast;
            ui8 numNorth;
        };
    };
    union {
        LiteCoarseNavNodeEdge edges[4][8]; // Cartesian, 8x8 so 8 possible edges per side
        struct {
            LiteCoarseNavNodeEdge southEdges[8];
            LiteCoarseNavNodeEdge westEdges[8];
            LiteCoarseNavNodeEdge eastEdgest[8];
            LiteCoarseNavNodeEdge northEdges[8];
        };
    };
    ui8 numNestedContainers;
    mutable bool isClosed; // For use in single threaded pathfinding
    ui8 width = 8;
    ui8 depth = 8;
};

static_assert(sizeof(CoarseNavNode) == 80, "Keep small");

struct CoarseNavNodeIndexPair {
    TileContainerID tileContainerID;
    CoarseNavNodeIndex index;
};
static_assert(sizeof(CoarseNavNodeIndexPair) == 8, "Keep small");

struct CoarseNavPatch {
    // Could fit another ui32 here
    TileContainer* parentContainer;
    CoarseNavNode* nodes = nullptr;
    ui32 size = 0;
};

class NavGraph
{
public:
    NavGraph(World& world);
    // TODO: async
    void buildNavPatchForContainer(TileContainer& tileContainer);
    void debugDrawNavPatchForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId = 0) const;

    const CoarseNavNode* getNode(CoarseNavNodeIndexPair index) const {
        // TODO: what if invalid
        const CoarseNavPatch& patch = mNavPatches[index.tileContainerID];
        assert(index.index < patch.size);
        return &patch.nodes[index.index];
    }

private:
    void buildEdges(TileContainer& tileContainer, const int cornerX, const int cornerY, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, CoarseNavNodeIndex* navNodeIdTable, std::vector<CoarseNavNode>& navNodes, Cartesian dir);
    void addNodeEdge(TileContainer& tileContainer, CoarseNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<CoarseNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir);

    std::unordered_map<TileContainerID, CoarseNavPatch> mNavPatches;
    World& mWorld;
};