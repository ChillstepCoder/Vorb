#include "stdafx.h"
#include "SimChunk.h"

#include "world/Chunk.h"

// Check for mismatches in harvestables and tiles
#define ENABLE_DEBUG_VALIDATE 0

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
    debugValidateHarvestables();
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
    debugValidateHarvestables();
}

const SimTileData* SimChunkTileData::tryGetTileData(ChunkTileIndex pos) const {
    auto&& it = tileIndexToTileData.find(pos);
    if (it != tileIndexToTileData.end()) {
        return &it->second;
    }
    return nullptr;
}

void SimChunkTileData::debugValidateHarvestables() {
#if defined(DEBUG) && ENABLE_DEBUG_VALIDATE
    for (auto&& it : harvestables) {
        for (size_t i = 0; i < it.second.size(); ++i) {
            ChunkTileIndex index = it.second[i];
            auto&& tileData = tileIndexToTileData.find(index);
            assert(tileData != tileIndexToTileData.end());
            assert(TileRepository::get().getLoadedOrUnloadedAsset(tileData->second.tileId).harvestable == it.first);
            for (size_t j = i + 1; j < it.second.size(); ++j) {
                assert(it.second[i] != it.second[j]);
            }
        }
    }
#endif // DEBUG
}

void SimChunkTileData::onTileAdded(TileID id, ChunkTileIndex pos) {
    incrementTileQuantity(id, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(id).harvestable;
    if (harvestable != TileHarvestable::None) {
        harvestables[harvestable].emplace_back(pos);

        debugValidateHarvestables();
    }
}

void SimChunkTileData::onTileRemoved(TileID id, ChunkTileIndex pos) {
    // Not all removal paths go here, see tryClearHarvestable
    decrementTileQuantity(id, 1);
    TileHarvestable harvestable = TileRepository::get().getLoadedOrUnloadedAsset(id).harvestable;
    if (harvestable != TileHarvestable::None) {
        bool found = false;
        auto&& hit = harvestables.find(harvestable);
        assert(hit != harvestables.end());
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

    mTileData->debugValidateHarvestables();

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

    mTileData->debugValidateHarvestables();

    return id;
}

FlatMap<ItemID, std::vector<TileItemStack>> SimChunk::getItemDataCopy() const {
    std::lock_guard lock(mMutex);
    return mItemData.itemStacks;
}

TileItemUID SimChunk::tryAddItemStackToGroundSimThread(ItemStack itemStack, ChunkTileIndex tileIndex) {
    std::lock_guard lock(mMutex);
    return mItemData.addStackToTileSimThread(tileIndex, itemStack);
}

TileItemUID SimChunk::tryAddItemStackToGroundGameThread(ItemStack itemStack, ChunkTileIndex tileIndex) {
    std::lock_guard lock(mMutex);
    return mItemData.addStackToTileGameThread(tileIndex, itemStack);
}

SimChunkTileItemReservationPtr SimChunk::tryReserveItemStackOnTile(ChunkTileIndex tileIndex, ItemID itemId, ui16 quantity) {
    std::lock_guard lock(mMutex);
    return mItemData.tryReserveItemStackOnTile(tileIndex, itemId, quantity, *this);
}

SimChunkTileItemReservationPtr SimChunk::tryReserveItemStack(TileItemUID uid, ItemID itemId, ui16 quantity) {
    std::lock_guard lock(mMutex);
    return mItemData.tryReserveItemStack(uid, itemId, quantity, *this);
}

void SimChunk::untrackItem(TileItemUID uid, ItemID itemId) {
    std::lock_guard lock(mMutex);
    mItemData.untrackItem(uid, itemId);
}

i32v2 SimChunk::tryPickupItemsForReservation(SimChunkTileItemReservation& reservation, i32 maxCount) {
    { // Critical section
        std::lock_guard lock(mMutex);
        return mItemData.tryPickupItemsForReservation(reservation, maxCount);
    }
    return i32v2(0);
}

bool SimChunk::hasBlockingTileAtIndex(ChunkTileIndex tileIndex) const {
    std::shared_lock lock(mMutex); // Critical Section
    if (!mTileData) {
        return true; // TODO: OCEAN IS CURRENTLY ALWAYS BLOCKING
    }
    if (const SimTileData* tileData = mTileData->tryGetTileData(tileIndex)) {
        // TODO: Need to set blocking bit still
        return tileData->flags.isBitSet(SimTileDataFlags::Blocking);
    }
    return false;
}

void SimChunk::beginSimulating() {
    PROFILE_FUNCTION();
    mIsSimulating = true;
    { // Critical Section
        std::shared_lock lock(mMutex);
        mItemData.combineStacks();
    }
}

void SimChunk::stopSimulating() {
    mIsSimulating = true;
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
    return sUniqueIdGenerator.fetch_add(1, std::memory_order_relaxed) + 1;
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

i32v2 SimChunkItemData::tryPickupItemsForReservation(SimChunkTileItemReservation& reservation, i32 maxCount) {
    auto&& it = itemStacks.find(reservation.mItemID);
    if (it != itemStacks.end()) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            TileItemStack& stack = it->second[i];
            if (stack.uniqueId == reservation.mItemUID) {
                const i32 count = std::min(maxCount, (i32)reservation.mReservedCount);
                assert(stack.reservedCount >= count);
                stack.reservedCount -= count;
                reservation.mReservedCount -= count;
                // Just in case someone stole items they didnt reserve,
                // we must check if we are trying to pick up more than exists
                i32v2 rv;
                rv.x = std::min(count, (i32)stack.count);
                stack.count -= rv.x;
                rv.y = stack.count;
                if (stack.count == 0) {
                    it->second[i] = it->second.back();
                    it->second.pop_back();
                } else if (stack.count < reservation.mReservedCount) {
                    // If there are less items remaining than we reserved, need to reduce the reservation
                    reservation.mReservedCount = stack.count;
                }
                return rv;
            }
        }
    }
    // If we don't find it, we have no reservation remaining
    reservation.mReservedCount = 0;
    return i32v2(0);
}

TileItemUID SimChunkItemData::addStackToTileSimThread(ChunkTileIndex tileIndex, ItemStack stack) {
    ASSERT_SIM_THREAD();
    // Sim thread can combine stacks together

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

TileItemUID SimChunkItemData::addStackToTileGameThread(ChunkTileIndex tileIndex, ItemStack stack){
    ASSERT_GAME_THREAD();
    // Sim thread keeps stacks distinct as they are different item entities

    assert(stack.isValid());
    assert(stack.count <= MAX_TILE_ITEM_STACK_SIZE);

    std::vector<TileItemStack>& stacks = itemStacks[stack.id];
    TileItemUID uid = generateNextItemUID();
    stacks.emplace_back(TileItemStack{ .tileIndex = tileIndex, .count = (ui16)stack.count, .props = stack.props, .uniqueId = uid });
    return uid;
}

void SimChunkItemData::untrackItem(TileItemUID uid, ItemID itemId) {
    std::vector<TileItemStack>& stacks = itemStacks[itemId];
    for (TileItemStack& tileStack : stacks) {
        if (tileStack.uniqueId == uid) {
            tileStack = stacks.back();
            stacks.pop_back();
            return;
        }
    }
    panic("Tried to untrack item {} which was not tracked", uid);
}

void SimChunkItemData::combineStacks() {
    // Merge all stacks for efficiency
    for (auto& [itemId, stacks] : itemStacks) {
        for (size_t i = 0; i < stacks.size(); ++i) {
            TileItemStack& stack = stacks[i];
            for (size_t j = i + 1; j < stacks.size();) {
                TileItemStack& other = stacks[j];
                if (stack.tileIndex == other.tileIndex && other.reservedCount == 0 && stack.canCombine(other)) {
                    stack.combine(other);
                    stacks[j] = stacks.back();
                    stacks.pop_back();
                }
                else {
                    ++j;
                }
            }
        }
    }
}
