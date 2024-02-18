#pragma once
#include "util/SpatialGrid2D.h"

#include <shared_mutex>
#include <boost/container/flat_map.hpp>

#include "tile/TileWallContainer.h"
#include "util/BitArray.h"

#include "serialization/BitseryExt.h"

enum class SimChunkTileContainerState : ui8 {
    NONE,
    //Wilderness, // Unloaded, no tiles
    Ocean, // Never loaded on sim layer
    Allocated // Tiles are loaded into memory
};

struct SimTileData {
    TileID tileId;
    ui8 variant;
    ui8 padding;
};
static_assert(sizeof(SimTileData) == 4);

struct SimChunkTileData {
    boost::container::flat_map<ui16, SimTileData> tileIndexToTileData;
    boost::container::flat_map<TileID, ui32> tileQuantities;
    TileWallContainer tileWalls; // Most chunks don't have walls

    BINARY_SERIALIZE();
    template <typename T>
    inline void sharedSerialize(T& s) {
        s.ext(tileIndexToTileData, bitsery::ext::BoostFlatMap{CHUNK_SIZE}, [](T& s, ui16& key, SimTileData& value) {
            s.value2b(key);
            s.value2b(value.tileId);
            s.value1b(value.variant);
        });
        s.object(tileWalls);
    }
    BINARY_SERIALIZE_INPUT() {
        sharedSerialize(s);
        if (tileQuantities.empty()) {
            for (auto& [tileIndex, tileData] : tileIndexToTileData) {
                auto&& it = tileQuantities.find(tileData.tileId);
                if (it == tileQuantities.end()) [[unlikely]] {
                    tileQuantities.emplace(tileData.tileId, 1);
                }
                else {
                    ++it->second;
                }
            }
            tileQuantities.shrink_to_fit();
        }
    }
    BINARY_SERIALIZE_OUTPUT() {
        sharedSerialize(s);
    }
};

class SimChunkTileContainer {
    friend class WorldSaveContext;
    friend class SimChunkTileGrid;
    friend class ChunkGenerator;
public:
    // Return true if it wasnt already allocated
    bool allocate();
    ChunkID getChunkID() const { return chunkId; }
    SimChunkTileContainerState getState() const { return state; }
private:
    mutable std::shared_mutex mutex;
    std::unique_ptr<SimChunkTileData> data;
    SimChunkTileContainerState state = SimChunkTileContainerState::NONE;
    ChunkID chunkId;
    mutable std::atomic_flag isSaveUpToDate = ATOMIC_FLAG_INIT;

    BINARY_SERIALIZE();
    BINARY_SERIALIZE_INPUT() {
        s.value1b(state);
        if (state == SimChunkTileContainerState::Allocated) {
            allocate();
            s.object(*data);
        }
    }
    BINARY_SERIALIZE_OUTPUT() {
        s.value1b(state);
        if (data) {
            s.object(*data);
        }
    }

private:
    
};

class SimChunkTileGrid {
    friend class WorldSaveContext;
public:
    SimChunkTileGrid(ui32 worldWidthTiles);
    ~SimChunkTileGrid();

    ui32 getApproxMemoryUsageBytes() const;

    const SimChunkTileContainer& getChunk(ChunkID chunkId) {
        return mChunkData[chunkId];
    }

    SimChunkTileContainer& getChunkForGeneration(ChunkID chunkId) {
        return mChunkData[chunkId];
    }

    // For memory tracking only
    void onNewChunkAllocated() { ++mTotalSimulatingChunks; }
private:
    void initInternal();

    std::atomic<ui32> mTotalSimulatingChunks = 0;
    ui32 mWorldWidthTiles;
    ui32 mWidthChunks = 0;
    ui32 mTotalChunks;
    SpatialGrid2D mSpatialGrid;
    std::unique_ptr<SimChunkTileContainer[]> mChunkData;

    BINARY_SERIALIZE() {
        s.value4b(mWidthChunks);
        if (!mChunkData) {
            initInternal();
        }
        for (ui32 i = 0; i < mTotalChunks; ++i) {
            s.object(mChunkData[i]);
        }
    }
};

