#pragma once
#include "util/SpatialGrid2D.h"

#include <shared_mutex>
#include <boost/container/flat_map.hpp>

#include "tile/TileWallContainer.h"
#include "util/BitArray.h"

enum class SimChunkTileContainerState : ui8 {
    Wilderness, // Unloaded, no tiles
    Ocean, // Never loaded on sim layer
    Allocated // Tiles are loaded into memory
};

// Flags instead?
enum class SimTileState : ui8 {
    Empty,
    Tile,
    Structure,
    Blocked, // Blocked by other tile
    COUNT
};

struct SimTileData {
    ui16 tileIndex;
    ui8 variant : 4 = {};
    //ui8 mainLayerVariant : 4 = {};
    ui8 PADDING; // Use
};
static_assert(sizeof(SimTileData) == 4);

struct SimChunkTileData {
    boost::container::flat_map<ui16, TileID> tileIndexToTileID;
    boost::container::flat_map<TileID, std::vector<SimTileData>> tileIdToTileData;
    TileWallContainer tileWalls; // Most chunks don't have walls
    std::vector<SimTileState> tileStates; // Flags instead?
};

class SimChunkTileContainer {
    friend class SimChunkTileGrid;
    friend class ChunkGenerator;
public:
    // Return true if it wasnt already allocated
    bool allocate();
    ChunkID getChunkID() const { return chunkId; }
private:
    std::shared_mutex mutex;
    std::unique_ptr<SimChunkTileData> data;
    SimChunkTileContainerState state = SimChunkTileContainerState::Ocean;
    ChunkID chunkId;
};

class SimChunkTileGrid {
public:
    SimChunkTileGrid(ui32 worldWidthTiles);
    ~SimChunkTileGrid();

    ui32 getApproxMemoryUsageBytes() const;
    SimChunkTileContainer& getChunkForGeneration(ChunkID chunkId) {
        return mChunkData[chunkId];
    }

    // For memory tracking only
    void onNewChunkAllocated() { ++mTotalSimulatingChunks; }
private:
    std::atomic<ui32> mTotalSimulatingChunks = 0;
    ui32 mWorldWidthTiles;
    ui32 mTotalChunks;
    SpatialGrid2D mSpatialGrid;
    std::unique_ptr<SimChunkTileContainer[]> mChunkData;
};

