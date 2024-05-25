#include "stdafx.h"
#include "SimChunkGrid.h"


SimChunkGrid::SimChunkGrid(ui32 worldWidthTiles) {
    mWidthChunks = worldWidthTiles / CHUNK_WIDTH;
    initInternal();
}

SimChunkGrid::~SimChunkGrid() {

}

ui32 SimChunkGrid::getApproxMemoryUsageBytes() const {
   return mTotalChunks * sizeof(SimChunk) + mTotalSimulatingChunks * (sizeof(SimChunkTileData));
}

SimTileDataWriteReservationPtr SimChunkGrid::tryReserveTileDataAtPosIfNotEmpty(ChunkID chunkId, TileIndex tileIndex) {
    SimChunk& container = mChunkData[chunkId];
    assert(container.isAllocated()); // TODO: Allow allocation later?

    { // Critical section
        std::unique_lock lock(container.mMutex);
        auto&& it = container.mData->tileIndexToTileData.find(tileIndex);
        if (it != container.mData->tileIndexToTileData.end()) {
            // Tile was tracked, only reserve if not empty
            SimTileData& existing = it->second;
            if (existing.tileId != TILE_ID_NONE) {
                existing.flags.setBit(SimTileDataFlags::Reserved);
                lock.unlock(); // UNLOCK - We are done modifying
                return std::make_unique<SimTileDataWriteReservation>(*this, chunkId, (ui16)tileIndex, existing);
            }
        }
    }
    return nullptr;
}

SimTileDataWriteReservationPtr SimChunkGrid::tryReserveTileDataAtPosIfNotEmpty(TileCoord tileCoord) {
    const auto[chunkId, tileIndex] = tileCoord.toChunkTileIndexAndChunkID(mWidthChunks);
    return tryReserveTileDataAtPosIfNotEmpty(chunkId, tileIndex);
}

void SimChunkGrid::releaseTileDataReservationAndCopyData(SimTileDataWriteReservation& reservation) {
    reservation.mDidRelease = true;
    SimChunk& container = mChunkData[reservation.mChunk];
    assert(container.isAllocated());
    SimTileData newData = reservation.reservedCopy;
    newData.flags.clearBit(SimTileDataFlags::Reserved); // No reserve anymore
    bool didModify = false;

    { // Critical section
        std::lock_guard lock(container.mMutex);
        auto&& it = container.mData->tileIndexToTileData.find(reservation.mTileIndex);
        if (it == container.mData->tileIndexToTileData.end()) {
            // Tile was not tracked, just add it
            if (newData.tileId != TILE_ID_NONE) {
                container.mData->tileIndexToTileData.emplace(reservation.mTileIndex, newData);
                container.mData->incrementTileQuantity(newData.tileId, 1);
                didModify = true;
            }
            else if (!newData.isNull()) {
                // We can still store data with empty tile as long as there are flags
                container.mData->tileIndexToTileData.emplace(reservation.mTileIndex, newData);
                didModify = true;
            }
        }
        else {
            // Tile was tracked, we need to modify it and change quantities
            SimTileData& existing = it->second;
            if (existing.tileId != newData.tileId) {
                didModify = true;
                container.mData->decrementTileQuantity(existing.tileId, 1);
                if (newData.tileId != TILE_ID_NONE) {
                    container.mData->incrementTileQuantity(newData.tileId, 1);
                }
                if (newData.isNull()) {
                    container.mData->tileIndexToTileData.erase(it);
                }
                else {
                    it->second = newData;
                }
            }
            else if (existing != newData) {
                it->second = newData;
            }
        }
    }
}

void SimChunkGrid::initInternal() {
    mSpatialGrid.init(CHUNK_WIDTH, mWidthChunks);
    mTotalChunks = SQ(mSpatialGrid.getGridWidthCells());
    mChunkData = std::make_unique<SimChunk[]>(mTotalChunks);
    LOG_DEBUG("Sim chunk grid allocated {} mb data",
        (mTotalChunks * sizeof(SimChunk)) / 1024.f / 1024.f);
    for (ChunkID id = 0; id < mTotalChunks; ++id) {
        mChunkData[id].mChunkID = id;
    }
}

SimTileDataWriteReservation::SimTileDataWriteReservation(SimChunkGrid& grid, ChunkID chunk, ChunkTileIndex tileIndex, SimTileData data) :
    mGrid(&grid),
    mChunk(chunk),
    mTileIndex(tileIndex),
    reservedCopy(data) {

}

SimTileDataWriteReservation::~SimTileDataWriteReservation() {
    if (!mDidRelease) {
        copyBackAndRelease();
    }
}

void SimTileDataWriteReservation::copyBackAndRelease() {
    assert(!mDidRelease);
    mGrid->releaseTileDataReservationAndCopyData(*this);
}

POOLED_ALLOC_DEF_THREADSAFE(SimTileDataWriteReservation, 128);
