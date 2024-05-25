#include "stdafx.h"
#include "SimChunkTileGrid.h"

#include "world/Chunk.h"

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

SimTileDataWriteReservationPtr SimChunkTileGrid::tryReserveTileDataAtPosIfNotEmpty(TileCoord tileCoord) {
    const auto[chunkId, tileIndex] = tileCoord.toChunkTileIndexAndChunkID(mWidthChunks);
    return tryReserveTileDataAtPosIfNotEmpty(chunkId, tileIndex);
}

void SimChunkTileGrid::releaseTileDataReservationAndCopyData(SimTileDataWriteReservation& reservation) {
    reservation.mDidRelease = true;
    SimChunkTileContainer& container = mChunkData[reservation.mChunk];
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

void SimChunkTileGrid::initInternal() {
    mSpatialGrid.init(CHUNK_WIDTH, mWidthChunks);
    mTotalChunks = SQ(mSpatialGrid.getGridWidthCells());
    mChunkData = std::make_unique<SimChunkTileContainer[]>(mTotalChunks);
    LOG_DEBUG("Sim chunk grid allocated {} mb data",
        (mTotalChunks * sizeof(SimChunkTileContainer)) / 1024.f / 1024.f);
    for (ChunkID id = 0; id < mTotalChunks; ++id) {
        mChunkData[id].mChunkID = id;
    }
}

bool SimChunkTileContainer::allocate() {
    std::lock_guard lock(mMutex);
    if (!mData) {
        mState = SimChunkTileContainerState::Allocated;
        mData = std::make_unique<SimChunkTileData>();
        return true;
    }
    assert(mState == SimChunkTileContainerState::Allocated);
    return false;
}

void SimChunkTileContainer::bindEditEventToChunkTileContainer(Chunk& chunk) {
    TileContainer* chunkTileContainer = chunk.getTileContainer();

    assert(chunk.getTileContainer());
    // Thread safe updates of the sim tile grid
    mEditTilesEventHandle = chunkTileContainer->addEditTilesListener([this](const TileContainerEvent& containerEvent) {
        static_assert(e_cast(TileContainerEditEventType::TYPES) == 5, "Update handler");

        ASSERT_GAME_THREAD();

        const TileContainerEditEvent& editEvent = std::get<TileContainerEditEvent>(containerEvent.varEvent);
        if (editEvent.type == TileContainerEditEventType::ChangeLayer) {
            allocate();
            std::lock_guard lock(mMutex);
            for (i32 i = 0; i < editEvent.editCount; ++i) {
                TileContainerEditLayerEventData& data = editEvent.changeLayerArray[i];
                if (data.layer == TileLayer::Main) [[likely]] {
                    if (data.prevId != TILE_ID_NONE) {
                        mData->removeTile(data.tileIndex);
                    }
                    if (data.newId != TILE_ID_NONE) {
                        mData->addTile(data.tileIndex, data.newId, data.newVariant);
                    }
                }
            }
            mIsSaveUpToDate.clear();
        }
    });
}

void SimChunkTileContainer::unBindEditEventToChunkTileContainer() {
    mEditTilesEventHandle.reset();
}

SimTileDataWriteReservation::SimTileDataWriteReservation(SimChunkTileGrid& grid, ChunkID chunk, ChunkTileIndex tileIndex, SimTileData data) :
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

void SimChunkTileData::addTile(ChunkTileIndex pos, TileID id, ui8 variant) {

    tileIndexToTileData.emplace(pos, SimTileData{.tileId = id, .variant = variant});
    
    incrementTileQuantity(id, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(id).harvestable;
    if (harvestable != TileHarvestable::NONE) {
        harvestables[harvestable].emplace_back(pos);
    }
}

void SimChunkTileData::removeTile(ChunkTileIndex pos) {
    auto&& it = tileIndexToTileData.find(pos);
    assert(it != tileIndexToTileData.end());
    SimTileData& data = it->second;
    decrementTileQuantity(data.tileId, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(data.tileId).harvestable;
    if (harvestable != TileHarvestable::NONE) {
        auto&& hit = harvestables.find(harvestable);
        for (size_t i = 0; i < hit->second.size(); ++i) {
            if (hit->second[i] == pos) {
                hit->second[i] = hit->second.back();
                hit->second.pop_back();
                break;
            }
        }
    }
    tileIndexToTileData.erase(it);
}
