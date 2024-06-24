#pragma once

#include "tile/TileContainerEvents.h"
#include "tile/TileWallContainer.h"

#include <shared_mutex>

#include "util/BitArray.h"
#include "util/FixedSizeVector.h"

#include "resources/TileRepository.h"

#include "tile/SimTileReservation.h"
#include "tile/SimTileData.h"
#include "item/SimChunkTileItemReservation.h"
#include "item/ItemStack.h"

class Chunk;

enum class SimChunkState : ui8 {
    NONE,
    //Wilderness, // Unloaded, no tiles
    Ocean, // Never loaded on sim layer
    Allocated // Tiles are loaded into memory
};

typedef UnorderedFlatMap<ChunkTileIndex, SimTileData> SimTileDataMap;

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
    // Used by ChunkGenerator
    SimTileDataMap::iterator removeTileDuringIter(SimTileDataMap::iterator iter);
    void changeTile(ChunkTileIndex pos, TileID id, ui8 variant);
    const SimTileData* tryGetTileData(ChunkTileIndex pos) const;

private:
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
                if (tileDef.harvestable != TileHarvestable::None) {
                    harvestables[tileDef.harvestable].emplace_back(tileIndex);
                }
            }
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
private:
    // Internal use only
    void onTileAdded(TileID id, ChunkTileIndex pos);
    void onTileRemoved(TileID id, ChunkTileIndex pos);

private:
    // SERIALIZED DATA
    SimTileDataMap tileIndexToTileData;
    TileWallContainer tileWalls; // Most chunks don't have walls
    // NOT SERIALIZED
    FlatMap<TileID, ui32> tileQuantities;
    FlatMap<TileHarvestable, std::vector<ChunkTileIndex>> harvestables;
};

class SimChunkItemData {
    friend class SimChunkGrid;
    friend class SimChunk;
    friend class ChunkGenerator;
    friend class SimChunkTileItemReservation;

public:
    TileItemUID generateNextItemUID();

private:
    SimChunkTileItemReservationPtr tryReserveItemStackOnTile(ChunkTileIndex tileIndex, ItemID itemId, ui16 quantity, SimChunk& owner);
    SimChunkTileItemReservationPtr tryReserveItemStack(TileItemUID uid, ItemID itemId, ui16 quantity, SimChunk& owner);
    // Returns <pickedCount, remainingCount>
    [[nodiscard]] i32v2 tryPickupItemsForReservation(SimChunkTileItemReservation& reservation, i32 maxCount);
    TileItemUID addStackToTile(ChunkTileIndex tileIndex, ItemStack stack);

private:
    BINARY_SERIALIZE() {
        //s.ext(patches, bitsery::ext::PodStructVector{})
        s.container(itemStacks);
    }

private:
    FlatMap<ItemID, std::vector<TileItemStack>> itemStacks;
    std::atomic<TileItemUID> uniqueIdGenerator = 0;
};

class SimChunk {
    friend class WorldSaveContext;
    friend class SimChunkGrid;
    friend class ChunkGenerator;
    friend class SimTileReservation;
    friend class SimChunkTileReservation;
    friend class SimChunkTileItemReservation;
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
    SimChunkTileReservationHandle tryReserveHarvestableAtTile(ChunkTileIndex tileIndex, TileHarvestable harvestable);

    // Lock chunk mutex and get a copy of the current tile data
    [[nodiscard]] SimTileData getTileDataCopy(ChunkTileIndex tileIndex) const;

    // Lock chunk and try to clear specified harvestable at this point.
    // If harvestable does not exist here, return TILE_ID_NONE
    // TODO: Variant?
    [[nodiscard]] TileID tryClearHarvestable(TileHarvestable expectedHarvestable, ChunkTileIndex tilePos);

    // Lock chunk mutex and get a copy of the current item data
    [[nodiscard]] FlatMap<ItemID, std::vector<TileItemStack>> getItemDataCopy() const;

    // Items
    // On fail returns INVALID_TILE_ITEM_UID
    TileItemUID tryDropItemStackOnGround(ItemStack itemStack, ChunkTileIndex tileIndex);
    SimChunkTileItemReservationPtr tryReserveItemStackOnTile(ChunkTileIndex tileIndex, ItemID itemId, ui16 quantity);
    SimChunkTileItemReservationPtr tryReserveItemStack(TileItemUID uid, ItemID itemId, ui16 quantity);
    // Returns <picked up count, remaining count>
    [[nodiscard]] i32v2 tryPickupItemsForReservation(SimChunkTileItemReservation& reservation, i32 maxCount);

    bool hasBlockingTileAtIndex(ChunkTileIndex tileIndex) const;

    bool isSimulating() const { return mIsSimulating; }
    void setSimulating(bool simulating) { mIsSimulating = simulating; }

private:
    // TODO: These functions assume a lock so need to be constrained to an interface friend class?
    bool tryReserveNonEmptyTile(ChunkTileIndex tileIndex);
    bool tryReserveHarvestableTile(ChunkTileIndex tileIndex, TileHarvestable harvestable);
    void freeTileReservation(ChunkTileIndex tileIndex);

    mutable std::shared_mutex mMutex;
    std::unique_ptr<SimChunkTileData> mTileData;
    SimChunkState mState = SimChunkState::NONE;
    ChunkID mChunkID;
    mutable std::atomic_flag mIsSaveUpToDate = ATOMIC_FLAG_INIT;
    std::atomic_bool mIsSimulating = true;
    TileContainerEventDispatcher::Handle mEditTilesEventHandle;
    SimChunkItemData mItemData;

    BINARY_SERIALIZE();
    BINARY_SERIALIZE_INPUT() {
        s.value1b(mState);
        if (mState == SimChunkState::Allocated) {
            allocate();
            s.object(*mTileData);
        }
    }
    BINARY_SERIALIZE_OUTPUT() {
        s.value1b(mState);
        if (mTileData) {
            s.object(*mTileData);
        }
    }
};
