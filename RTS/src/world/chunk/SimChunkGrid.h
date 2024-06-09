#pragma once
#include "util/SpatialGrid2D.h"

#include "world/chunk/SimChunk.h"

// Lock a sim tile with intent to modify, so it can't be modified or used by anyone else
class SimTileDataWriteReservation {
    friend class SimChunkGrid;
public:
    SimTileDataWriteReservation() = delete;
    SimTileDataWriteReservation(SimChunkGrid& grid, ChunkID chunk, ChunkTileIndex tileIndex, SimTileData data);
    ~SimTileDataWriteReservation();

    POOLED_ALLOC_DECL();

    ChunkTileIndex getTileIndex() const { return mTileIndex; }
    ChunkID getChunkID() const { return mChunk; }
    SimChunkGrid* getSimChunkGrid() const { return mGrid; }
    bool didRelease() const { return mDidRelease; }

    void copyBackAndRelease();
    void cancelReservation() { mDidRelease = true; }

    // Freely modify this and then release or destroy the reservation
    SimTileData reservedCopy;
private:
    SimChunkGrid* mGrid = nullptr;
    ChunkTileIndex mTileIndex;
    ChunkID mChunk;
    bool mDidRelease = false;
};
typedef std::unique_ptr<SimTileDataWriteReservation> SimTileDataWriteReservationPtr;


class SimChunkGrid {
    friend class WorldSaveContext;
public:
    SimChunkGrid(ui32 worldWidthTiles);
    ~SimChunkGrid();

    ui32 getApproxMemoryUsageBytes() const;

    const SimChunk& getChunk(ChunkID chunkId) const {
        return mChunkData[chunkId];
    }
    SimChunk& getChunk(ChunkID chunkId) {
        return mChunkData[chunkId];
    }

    SimChunk& getChunkForGeneration(ChunkID chunkId) {
        return mChunkData[chunkId];
    }

    // TODO: Delete this? It doesn't account chunks that are fully simulated
    SimTileDataWriteReservationPtr tryReserveTileDataAtPosIfNotEmpty(ChunkID chunkId, TileIndex tileIndex);
    SimTileDataWriteReservationPtr tryReserveTileDataAtPosIfNotEmpty(TileCoord tileCoord);
    void releaseTileDataReservationAndCopyData(SimTileDataWriteReservation& reservation);

    // Returns sorted list of harvestables
    // These are not guarenteed to still exist
    SortedIntCoordDistanceSqMap getClosestUnreservedHarvestablesToPoint(TileCoord worldPos, TileHarvestable harvestable, i32 maxDistance, i32 maxCount);

    SimChunkTileReservationHandle tryReserveHarvestableAtTilePos(TileCoord worldPos, TileHarvestable harvestable);

    // Items
    // On fail returns INVALID_TILE_ITEM_UID
    TileItemUID tryDropItemStackOnGround(ItemStack stack, TileCoord worldPos);
    SimChunkTileItemReservationPtr tryReserveItemStack(TileCoord worldPos, TileItemUID uid, ItemID itemId, ui16 quantity);

    bool hasBlockingTileAtWorldPos(TileCoord worldPos) const;

    // For memory tracking only
    void onNewChunkAllocated() { ++mTotalSimulatingChunks; }
private:
    void initInternal();

    std::atomic<ui32> mTotalSimulatingChunks = 0;
    ui32 mWorldWidthTiles;
    ui32 mWidthChunks = 0;
    ui32 mTotalChunks;
    SpatialGrid2D mSpatialGrid;
    std::unique_ptr<SimChunk[]> mChunkData;

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

