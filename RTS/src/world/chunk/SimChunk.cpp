#include "stdafx.h"
#include "SimChunk.h"

#include "world/Chunk.h"

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

    tileIndexToTileData.emplace(pos, SimTileData{ .tileId = id, .variant = variant });

    incrementTileQuantity(id, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(id).harvestable;
    if (harvestable != TileHarvestable::None) {
        harvestables[harvestable].emplace_back(pos);
    }
}

void SimChunkTileData::removeTile(ChunkTileIndex pos) {
    auto&& it = tileIndexToTileData.find(pos);
    assert(it != tileIndexToTileData.end());
    SimTileData& data = it->second;
    decrementTileQuantity(data.tileId, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(data.tileId).harvestable;
    if (harvestable != TileHarvestable::None) {
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

bool SimChunk::allocate() {
    std::lock_guard lock(mMutex);
    if (!mData) {
        mState = SimChunkState::Allocated;
        mData = std::make_unique<SimChunkTileData>();
        return true;
    }
    assert(mState == SimChunkState::Allocated);
    return false;
}

void SimChunk::bindEditEventToChunkTileContainer(Chunk& chunk) {
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

void SimChunk::unBindEditEventToChunkTileContainer() {
    mEditTilesEventHandle.reset();
}

i32 SimChunk::tryReserveHarvestables(i32 maxCount, TileHarvestable harvestable, SimChunkTileReservationHandleVector& outReservationHandles) {
    i32 reservedCount = 0;
    { // Write lock since we set reserve flag
        std::shared_lock lock(mMutex);
        if (!mData) {
            return 0;
        }
        auto&& it = mData->harvestables.find(harvestable);
        if (it == mData->harvestables.end()) {
            return 0;
        }
        std::vector<ChunkTileIndex>& tileIndices = it->second;
        for (ChunkTileIndex index : tileIndices) {
            SimChunkTileReservationHandle newHandle = SimTileReservation::tryReserveSimTileForChunk(ChunkLiteTileHandle(mChunkID, index));
            if (newHandle) {
                if (outReservationHandles.try_push_back(std::move(newHandle))) {
                    if (++reservedCount == maxCount) {
                        return reservedCount;
                    }
                }
                else {
                    return reservedCount;
                }
            }
        }
    }
    return reservedCount;
}


SimChunkTileReservationHandle SimChunk::tryReserveHarvestableAtTile(ChunkTileIndex tileIndex, TileHarvestable harvestable) {
    std::lock_guard writeLock(mMutex);
    return SimTileReservation::tryReserveHarvestableSimTileForChunk(ChunkLiteTileHandle(mChunkID, tileIndex), harvestable);
}

bool SimChunk::tryReserveNonEmptyTile(ChunkTileIndex tileIndex) {
    // Does not lock as we can only create these reservations from within SimChunk lock
    if (mData) {
        auto&& it = mData->tileIndexToTileData.find(tileIndex);
        if (it == mData->tileIndexToTileData.end()) {
            return false;
        }
        if (it->second.flags.isBitSet(SimTileDataFlags::Reserved)) {
            return false;
        }
        it->second.flags.setBit(SimTileDataFlags::Reserved);
        // TODO: Need to notify full chunk of the reservation
        return true;
    }
    return false;
}

bool SimChunk::tryReserveHarvestableTile(ChunkTileIndex tileIndex, TileHarvestable harvestable) {
    // Does not lock as we can only create these reservations from within SimChunk lock
    if (mData) {
        auto&& it = mData->tileIndexToTileData.find(tileIndex);
        if (it == mData->tileIndexToTileData.end()) {
            return false;
        }
        if (it->second.flags.isBitSet(SimTileDataFlags::Reserved)) {
            return false;
        }
        if (TileRepository::get().getLoadedOrUnloadedAsset(it->second.tileId).harvestable == harvestable) {
            it->second.flags.setBit(SimTileDataFlags::Reserved);
            // TODO: Need to notify full chunk of the reservation
            return true;
        }
    }
    return false;
}

void SimChunk::freeTileReservation(ChunkTileIndex tileIndex) {
    // DOES lock, as is called from destructor of SimChunkTileReservation
    std::lock_guard lock(mMutex);
    if (mData) {
        auto&& it = mData->tileIndexToTileData.find(tileIndex);
        if (it == mData->tileIndexToTileData.end()) {
            return;
        }
        it->second.flags.clearBit(SimTileDataFlags::Reserved);
    }
}
