#pragma once

#include "tile/TileContainerEvents.h"
#include "tile/TileWallContainer.h"

#include <shared_mutex>
#include <boost/container/flat_map.hpp>

#include "util/BitArray.h"
#include "util/FixedSizeVector.h"

#include "serialization/BitseryExt.h"
#include "resources/TileRepository.h"

#include "tile/SimTileReservation.h"

class Chunk;

enum class SimTileDataFlags : ui8 {
    Reserved = BIT(0)
};

struct SimTileData {
    TileID tileId;
    ui8 variant;
    BitFlags<SimTileDataFlags> flags;

    auto operator<=>(const SimTileData&) const = default;
    bool isNull() const { return tileId == TILE_ID_NONE && flags.getBits() == 0; }
};
static_assert(sizeof(SimTileData) == 4);

enum class SimChunkState : ui8 {
    NONE,
    //Wilderness, // Unloaded, no tiles
    Ocean, // Never loaded on sim layer
    Allocated // Tiles are loaded into memory
};


constexpr i16 MAX_SIM_TILE_RESERVATIONS_PER_QUERY = 128;
typedef FixedSizeVector<SimChunkTileReservationHandle, MAX_SIM_TILE_RESERVATIONS_PER_QUERY> SimChunkTileReservationHandleVector;

class SimChunkTileData {
    friend class SimChunkGrid;
    friend class SimChunk;
    friend class ChunkGenerator;
private:
    void incrementTileQuantity(TileID id, ui32 quantity);
    void decrementTileQuantity(TileID id, ui32 quantity);

    void addTile(ChunkTileIndex pos, TileID id, ui8 variant);
    void removeTile(ChunkTileIndex pos);

    // SERIALIZED DATA
    boost::container::flat_map<ChunkTileIndex, SimTileData> tileIndexToTileData;
    TileWallContainer tileWalls; // Most chunks don't have walls
    // NOT SERIALIZED
    boost::container::flat_map<TileID, ui32> tileQuantities;
    boost::container::flat_map<TileHarvestable, std::vector<ChunkTileIndex>> harvestables;

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
        // Why are we checking here?
        if (tileQuantities.empty()) {
            TileRepository& tileRepo = TileRepository::get();
            for (auto& [tileIndex, tileData] : tileIndexToTileData) {
                const TileDef& tileDef = tileRepo.getLoadedOrUnloadedAsset(tileData.tileId);
                auto&& it = tileQuantities.find(tileData.tileId);
                if (it == tileQuantities.end()) [[unlikely]] {
                    tileQuantities.emplace(tileData.tileId, 1);
                }
                else {
                    ++it->second;
                }
                if (tileDef.harvestable != TileHarvestable::NONE) {
                    harvestables[tileDef.harvestable].emplace_back(tileIndex);
                }
            }
            tileQuantities.shrink_to_fit();
        }
        else {
            assert(false); // TODO: I don't understand this case, why would we input twice?
        }
        for (auto&& it : harvestables) {
            it.second.shrink_to_fit();
        }
    }
    BINARY_SERIALIZE_OUTPUT() {
        sharedSerialize(s);
    }
};

class SimChunk {
    friend class WorldSaveContext;
    friend class SimChunkGrid;
    friend class ChunkGenerator;
    friend class SimTileReservation;
    friend class SimChunkTileReservation;
public:
    // Return true if it wasnt already allocated
    bool allocate();
    ChunkID getChunkID() const { return mChunkID; }
    SimChunkState getState() const { return mState; }
    bool isAllocated() const { return mState == SimChunkState::Allocated; }

    void bindEditEventToChunkTileContainer(Chunk& chunk);
    void unBindEditEventToChunkTileContainer();

    // Returns num reserved, set maxCount to 0 for infinite
    i32 tryReserveHarvestables(i32 maxCount, TileHarvestable harvestable, SimChunkTileReservationHandleVector& outReservationHandles);
private:
    bool tryReserveNonEmptyTile(ChunkTileIndex tileIndex);
    void freeTileReservation(ChunkTileIndex tileIndex);

    mutable std::shared_mutex mMutex;
    std::unique_ptr<SimChunkTileData> mData;
    SimChunkState mState = SimChunkState::NONE;
    ChunkID mChunkID;
    mutable std::atomic_flag mIsSaveUpToDate = ATOMIC_FLAG_INIT;
    TileContainerEventDispatcher::Handle mEditTilesEventHandle;

    BINARY_SERIALIZE();
    BINARY_SERIALIZE_INPUT() {
        s.value1b(mState);
        if (mState == SimChunkState::Allocated) {
            allocate();
            s.object(*mData);
        }
    }
    BINARY_SERIALIZE_OUTPUT() {
        s.value1b(mState);
        if (mData) {
            s.object(*mData);
        }
    }
};
