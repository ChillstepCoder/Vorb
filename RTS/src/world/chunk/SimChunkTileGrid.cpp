#include "stdafx.h"
#include "SimChunkTileGrid.h"

SimChunkTileGrid::SimChunkTileGrid(ui32 worldWidthTiles) {
    mWidthChunks = worldWidthTiles / CHUNK_WIDTH;
    initInternal();
}

SimChunkTileGrid::~SimChunkTileGrid() {

}

ui32 SimChunkTileGrid::getApproxMemoryUsageBytes() const {
   return mTotalChunks * sizeof(SimChunkTileContainer) + mTotalSimulatingChunks * (sizeof(SimChunkTileData));
}

SimTileDataWriteReservationPtr SimChunkTileGrid::tryReserveTileDataAtPosIfNotEmpty(ChunkID chunkId, TileIndex tileIndex) {
    SimChunkTileContainer& container = mChunkData[chunkId];
    assert(container.isAllocated()); // TODO: Allow allocation later?

    { // Critical section
        std::unique_lock lock(container.mutex);
        auto&& it = container.data->tileIndexToTileData.find(tileIndex);
        if (it != container.data->tileIndexToTileData.end()) {
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

SimTileDataWriteReservationPtr SimChunkTileGrid::tryReserveTileDataAtPosIfNotEmpty(TileCoord tileCoord) const {
    const auto[chunkId, tileIndex] = tileCoord.toChunkTileIndexAndChunkID(mWidthChunks);
    tryReserveTileDataAtPosIfNotEmpty(chunkId, tileIndex);
}

void SimChunkTileGrid::releaseTileDataReservationAndCopyData(SimTileDataWriteReservation& reservation) {
    reservation.mDidRelease = true;
    SimChunkTileContainer& container = mChunkData[reservation.mChunk];
    assert(container.isAllocated());
    SimTileData newData = reservation.reservedCopy;
    newData.flags.clearBit(SimTileDataFlags::Reserved); // No reserve anymore
    bool didModify = false;

    { // Critical section
        std::lock_guard lock(container.mutex);
        auto&& it = container.data->tileIndexToTileData.find(reservation.mTileIndex);
        if (it == container.data->tileIndexToTileData.end()) {
            // Tile was not tracked, just add it
            if (newData.tileId != TILE_ID_NONE) {
                container.data->tileIndexToTileData.emplace(reservation.mTileIndex, newData);
                container.data->incrementTileQuantity(newData.tileId, 1);
                didModify = true;
            }
            else if (!newData.isNull()) {
                // We can still store data with empty tile as long as there are flags
                container.data->tileIndexToTileData.emplace(reservation.mTileIndex, newData);
                didModify = true;
            }
        }
        else {
            // Tile was tracked, we need to modify it and change quantities
            SimTileData& existing = it->second;
            if (existing.tileId != newData.tileId) {
                didModify = true;
                container.data->decrementTileQuantity(existing.tileId, 1);
                if (newData.tileId != TILE_ID_NONE) {
                    container.data->incrementTileQuantity(newData.tileId, 1);
                }
                if (newData.isNull()) {
                    container.data->tileIndexToTileData.erase(it);
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

void SimChunkTileGrid::initInternal() {
    mSpatialGrid.init(CHUNK_WIDTH, mWidthChunks);
    mTotalChunks = SQ(mSpatialGrid.getGridWidthCells());
    mChunkData = std::make_unique<SimChunkTileContainer[]>(mTotalChunks);
    LOG_DEBUG("Sim chunk grid allocated {} mb data",
        (mTotalChunks * sizeof(SimChunkTileContainer)) / 1024.f / 1024.f);
    for (ChunkID id = 0; id < mTotalChunks; ++id) {
        mChunkData[id].chunkId = id;
    }
}

bool SimChunkTileContainer::allocate() {
    std::lock_guard lock(mutex);
    if (!data) {
        state = SimChunkTileContainerState::Allocated;
        data = std::make_unique<SimChunkTileData>();
        return true;
    }
    assert(state == SimChunkTileContainerState::Allocated);
    return false;
}

SimTileDataWriteReservation::SimTileDataWriteReservation(SimChunkTileGrid& grid, ChunkID chunk, ui16 tileIndex, SimTileData data) :
    mGrid(&grid),
    mChunk(chunk),
    mTileIndex(tileIndex),
    reservedCopy(data)
{

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

void SimChunkTileData::incrementTileQuantity(TileID id, ui32 quantity) {
    assert(id != TILE_ID_NONE);
    auto&& qit = tileQuantities.find(id);
    if (qit == tileQuantities.end()) {
        tileQuantities.emplace(id, quantity);
    }
    else {
        ++qit->second;
    }
}

void SimChunkTileData::decrementTileQuantity(TileID id, ui32 quantity) {
    auto&& qit = tileQuantities.find(id);
    assert(qit != tileQuantities.end());
    assert(qit->second >= quantity);
    qit->second -= quantity;
    if (qit->second == 0) {
        tileQuantities.erase(qit);
    }
}
