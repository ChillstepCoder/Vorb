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

    onTileAdded(id, pos);
}

void SimChunkTileData::removeTile(ChunkTileIndex pos) {
    auto&& it = tileIndexToTileData.find(pos);
    assert(it != tileIndexToTileData.end());
    SimTileData& data = it->second;
    onTileRemoved(data.tileId, pos);
    tileIndexToTileData.erase(it);
}

SimTileDataMap::iterator SimChunkTileData::removeTileDuringIter(SimTileDataMap::iterator iter) {
    SimTileData& data = iter->second;
    onTileRemoved(data.tileId, iter->first);
    return tileIndexToTileData.erase(iter);
}

void SimChunkTileData::changeTile(ChunkTileIndex pos, TileID id, ui8 variant) {
    // TODO: Flags?
    auto&& it = tileIndexToTileData.find(pos);
    if (it == tileIndexToTileData.end()) {
        // Tile was not tracked, just add it
        if (id != TILE_ID_NONE) {
            addTile(pos, id, variant);
        }
    }
    else {
        // Tile was tracked, we need to modify it and change quantities
        SimTileData& existing = it->second;
        if (existing.tileId != id) {
            onTileRemoved(existing.tileId, pos);
            if (id != TILE_ID_NONE) {
                onTileAdded(id, pos);
            }
            else {
                tileIndexToTileData.erase(it);
            }
        }
        else {
            // Its the same tile, but this wipes out flags and updates variant
            it->second = SimTileData{ .tileId = id, .variant = variant };
        }
    }
}

void SimChunkTileData::onTileAdded(TileID id, ChunkTileIndex pos) {
    incrementTileQuantity(id, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(id).harvestable;
    if (harvestable != TileHarvestable::None) {
        harvestables[harvestable].emplace_back(pos);
    }
}

void SimChunkTileData::onTileRemoved(TileID id, ChunkTileIndex pos) {
    // Not all removal paths go here, see tryClearHarvestable
    decrementTileQuantity(id, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(id).harvestable;
    if (harvestable != TileHarvestable::None) {
        bool found = false;
        auto&& hit = harvestables.find(harvestable);
        for (size_t i = 0; i < hit->second.size(); ++i) {
            if (hit->second[i] == pos) {
                hit->second[i] = hit->second.back();
                hit->second.pop_back();
                found = true;
                break;
            }
        }
        assert(found);
    }
}

bool SimChunk::allocate() {
    std::lock_guard lock(mMutex);
    if (!mTileData) {
        mState = SimChunkState::Allocated;
        mTileData = std::make_unique<SimChunkTileData>();
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
                        mTileData->removeTile(data.tileIndex);
                    }
                    if (data.newId != TILE_ID_NONE) {
                        mTileData->addTile(data.tileIndex, data.newId, data.newVariant);
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
        if (!mTileData) {
            return 0;
        }
        auto&& it = mTileData->harvestables.find(harvestable);
        if (it == mTileData->harvestables.end()) {
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

SimTileData SimChunk::getTileDataCopy(ChunkTileIndex tileIndex) const {
    std::shared_lock readLock(mMutex);
    if (!mTileData) {
        return SimTileData();
    }
    auto&& it = mTileData->tileIndexToTileData.find(tileIndex);
    if (it == mTileData->tileIndexToTileData.end()) {
        return SimTileData();
    }
    return it->second;
}

TileID SimChunk::tryClearHarvestable(TileHarvestable expectedHarvestable, ChunkTileIndex tileIndex) {
    std::lock_guard writeLock(mMutex);
    if (!mTileData) {
        return TILE_ID_NONE;
    }
    auto&& hit = mTileData->harvestables.find(expectedHarvestable);
    if (hit == mTileData->harvestables.end()) {
        return TILE_ID_NONE;
    }
    
    bool found = false;
    for (size_t i = 0; i < hit->second.size(); ++i) {
        if (hit->second[i] == tileIndex) {
            hit->second[i] = hit->second.back();
            hit->second.pop_back();
            found = true;
            break;
        }
    }

    if (!found) {
        return TILE_ID_NONE;
    }

    auto&& it = mTileData->tileIndexToTileData.find(tileIndex);
    assert(it != mTileData->tileIndexToTileData.end());

    const TileID id = it->second.tileId;
    mTileData->decrementTileQuantity(id, 1);

    mTileData->tileIndexToTileData.erase(it);
    return id;
}

std::unordered_map<ItemID, std::vector<TileItemStack>> SimChunk::getItemDataCopy() const {
    ASSERT_SIM_THREAD();
    std::lock_guard lock(mMutex);
    return mItemData.itemStacks;
}

TileItemUID SimChunk::tryDropItemStackOnGround(ItemStack itemStack, ChunkTileIndex tileIndex) {
    ASSERT_SIM_THREAD();
    if (!mIsSimulating) {
        return INVALID_TILE_ITEM_UID;
    }
    std::lock_guard lock(mMutex);
    return mItemData.addStackToTile(tileIndex, itemStack);
}

SimChunkTileItemReservationPtr SimChunk::tryReserveItemStackOnTile(ChunkTileIndex tileIndex, ItemID itemId, ui16 quantity) {
    ASSERT_SIM_THREAD();
    std::lock_guard lock(mMutex);
    return mItemData.tryReserveItemStackOnTile(tileIndex, itemId, quantity, *this);
}

SimChunkTileItemReservationPtr SimChunk::tryReserveItemStack(TileItemUID uid, ItemID itemId, ui16 quantity) {
    std::lock_guard lock(mMutex);
    return mItemData.tryReserveItemStack(uid, itemId, quantity, *this);
}

bool SimChunk::tryReserveNonEmptyTile(ChunkTileIndex tileIndex) {
    // Does not lock as we can only create these reservations from within SimChunk lock
    if (mTileData) {
        auto&& it = mTileData->tileIndexToTileData.find(tileIndex);
        if (it == mTileData->tileIndexToTileData.end()) {
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
    if (mTileData) {
        auto&& it = mTileData->tileIndexToTileData.find(tileIndex);
        if (it == mTileData->tileIndexToTileData.end()) {
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
    if (mTileData) {
        auto&& it = mTileData->tileIndexToTileData.find(tileIndex);
        if (it == mTileData->tileIndexToTileData.end()) {
            return;
        }
        it->second.flags.clearBit(SimTileDataFlags::Reserved);
    }
}

TileItemUID SimChunkItemData::generateNextItemUID() {
    // Increment by one
    return uniqueIdGenerator.fetch_add(1, std::memory_order_relaxed) + 1;
}

SimChunkTileItemReservationPtr SimChunkItemData::tryReserveItemStackOnTile(ChunkTileIndex tileIndex, ItemID itemId, ui16 quantity, SimChunk& owner) {
    auto&& it = itemStacks.find(itemId);
    if (it == itemStacks.end()) {
        return nullptr;
    }
    std::vector<TileItemStack>& stacks = it->second;
    for (TileItemStack& stack : stacks) {
        if (stack.tileIndex == tileIndex) {
            const i32 available = stack.count - stack.reservedCount;
            if (available >= quantity) {
                stack.reservedCount += quantity;
                return std::unique_ptr<SimChunkTileItemReservation>(new SimChunkTileItemReservation(stack.tileIndex, stack.uniqueId, itemId, quantity, owner));
            }
        }
    }
    return nullptr;
}

SimChunkTileItemReservationPtr SimChunkItemData::tryReserveItemStack(TileItemUID uid, ItemID itemId, ui16 quantity, SimChunk& owner) {
    auto&& it = itemStacks.find(itemId);
    if (it == itemStacks.end()) {
        return nullptr;
    }
    std::vector<TileItemStack>& stacks = it->second;
    for (TileItemStack& stack : stacks) {
        if (stack.uniqueId == uid) {
            const i32 available = stack.count - stack.reservedCount;
            if (available >= quantity) {
                stack.reservedCount += quantity;
                return std::unique_ptr<SimChunkTileItemReservation>(new SimChunkTileItemReservation(stack.tileIndex, stack.uniqueId, itemId, quantity, owner));
            }
        }
    }
    return nullptr;
}

TileItemUID SimChunkItemData::addStackToTile(ChunkTileIndex tileIndex, ItemStack stack) {
    assert(stack.isValid());
    assert(stack.count <= MAX_TILE_ITEM_STACK_SIZE);

    std::vector<TileItemStack>& stacks = itemStacks[stack.id];
    for (TileItemStack& tileStack : stacks) {
        if (tileStack.tileIndex == tileIndex && tileStack.tryCombine(stack)) {
            return tileStack.uniqueId;
        }
    }
    TileItemUID uid = generateNextItemUID();
    stacks.emplace_back(TileItemStack{ .tileIndex = tileIndex, .count = (ui16)stack.count, .props = stack.props, .uniqueId = uid });
    return uid;
}
