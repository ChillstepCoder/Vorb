#include "stdafx.h"
#include "ItemStockpile.h"

#include "debugging/DebugRenderer.h"
#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"

#include "ecs/IEntityComponentSystem.h"
#include "ItemReservation.h"
#include "ecs/component/OwnershipComponent.h"

#include "camera/Camera3D.h"

ItemStockpile::ItemStockpile(World& world, ItemStockpileID id, const i32AABB2& aabb, OPT bool* ownershipMask, entt::entity ownerEntity /*= INVALID_ENTITY*/)
    : mWorld(world)
    , mId(id)
    , mAABB(aabb)
    , mOwnerEntity(ownerEntity) {

    assert(mAABB.width <= MAX_STOCKPILE_WIDTH && mAABB.depth <= MAX_STOCKPILE_WIDTH);

    mStorage.resize(mAABB.width * mAABB.depth);

    mFirstFreeSlot = UINT32_MAX;

    f32 maxZPos = -FLT_MAX;
    // Set stockpile flags

    const IHeightmapGrid& heightmapGrid = mWorld.getHeightmapGrid();
    ui32 index = 0;
    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.depth; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.width; ++x) {
            const i32v2 worldPos(x, y);
            TileRef ref(mWorld.getTerrainTileHandleAtWorldPos(worldPos));
            const bool c = ownershipMask[index];
            if ((ownershipMask && ownershipMask[index] == false)/* || ref.tile->hasFlag(TILE_FLAG_IS_STOCKPILE)*/) {
                // If there is already a stockpile here, we are invalid
                mStorage[index].stack.id = INVALID_STOCKPILE_INDEX;
            }
            else {
                // Valid slot
                if (mFirstFreeSlot == UINT32_MAX) mFirstFreeSlot = index;
                ++mTotalSlots;
                ref.container->setTileFlag(ref.index, TileFlags::IS_STOCKPILE);
                f32 height = heightmapGrid.computeMaxHeightAtTile(worldPos);
                if (height > maxZPos) maxZPos = height;
            }
            ++index;
        }
    }
    mZPos = maxZPos;
    // We must have at least one slot
    assert(mTotalSlots);
    mFreeSlots = mTotalSlots;

    // Ownership
    if (mOwnerEntity != INVALID_ENTITY) {
        OwnershipComponent& ownershipCmp = mWorld.getECS().mRegistry.get<OwnershipComponent>(mOwnerEntity);
        ownershipCmp.mOwnedStockpiles.push_back(this);
    }

    dispatchCreate(ItemStockpileEvent{ this, INVALID_ITEM_ID });
}

ItemStockpile::~ItemStockpile() {
    // TODO: can we make this more elegant
    if (mWorld.isShuttingDown()) return;
    assert(mRefCount == 0);

    dispatchDestroy(ItemStockpileEvent{ this, INVALID_ITEM_ID });

    // TODO: Run a function on the reservation?
    assert(!mReservations.size()); // TODO: UNSUPPORTED
    for (auto&& it : mReservations) {
        it->mStockpile = nullptr;
    }

    // Clean up ownership
    if (mOwnerEntity != INVALID_ENTITY) {
        OwnershipComponent& ownershipCmp = mWorld.getECS().mRegistry.get<OwnershipComponent>(mOwnerEntity);
        for (size_t i = 0; i < ownershipCmp.mOwnedStockpiles.size(); ++i) {
            if (ownershipCmp.mOwnedStockpiles[i] == this) {
                ownershipCmp.mOwnedStockpiles[i] = ownershipCmp.mOwnedStockpiles.back();
                ownershipCmp.mOwnedStockpiles.pop_back();
                break;
            }
        }
    }
}

void ItemStockpile::renderDebug() const {
    ui32 index = 0;
    f32v2 cornerPos = f32v2(mAABB.pos);
    DebugRenderer::drawAABB(mAABB, mZPos, color4(1.0f, 0.0f, 0.0f));
    DebugRenderer::reserveFilledQuads(mAABB.dims.x * mAABB.dims.y);
    for (ui32 y = 0; y < mAABB.dims.y; ++y) {
        for (ui32 x = 0; x < mAABB.dims.x; ++x) {
            if (mStorage[index].stack.id != INVALID_STOCKPILE_INDEX) {
                if (mStorage[index].stack.isNull()) {
                    DebugRenderer::drawFilledQuad(f32v3(cornerPos.x + x, cornerPos.y + y, mZPos), f32v2(1.0f), color4(0.5f, 0.5f, 0.0f, 0.4f));
                }
                else {
                    DebugRenderer::drawFilledQuad(f32v3(cornerPos.x + x, cornerPos.y + y, mZPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 0.4f));
                }
            }
            ++index;
        }
    }
}

CALLER_DELETE std::unique_ptr<ItemReservation> ItemStockpile::tryReserveItemStack(ItemStack itemStack, ui32 minimumQuantity) {
    assert(itemStack.quantity >= minimumQuantity);
    assert(itemStack.quantity < UINT16_MAX);
    auto&& it = mItemContents.find(itemStack.id);
    if (it == mItemContents.end()) {
        // If we dont have this item, no
        return nullptr;
    }
    ItemStockpileRecord& record = it->second;
    const ui32 availableQuantity = record.totalQuantity - record.reservedQuantity;
    if (availableQuantity >= minimumQuantity) {
        // Reserve the stacks
        std::vector<ItemReservationTarget> targets;
        targets.reserve(std::min(it->second.stackLocations.size(), (size_t)5)); // Arbitrary
        ui16 remainingQuantityToReserve = std::min(availableQuantity, (ui32)itemStack.quantity);
        for (const ui16& stackIndex : it->second.stackLocations) {
            if (remainingQuantityToReserve == 0) break;

            ItemStockpileTileStorage& tileStorage = mStorage[stackIndex];
            assert(tileStorage.stack.id == itemStack.id);
            const ui16 freeQuantity = tileStorage.stack.quantity - tileStorage.reserveCount;
            const ui16 quantityToReserveThisTile = std::min(freeQuantity, remainingQuantityToReserve);
            remainingQuantityToReserve -= quantityToReserveThisTile;
            record.reservedQuantity += quantityToReserveThisTile;
            tileStorage.reserveCount += quantityToReserveThisTile;
            targets.emplace_back(ItemReservationTarget{ stackIndex, quantityToReserveThisTile });
        }
        assert(remainingQuantityToReserve == 0);
        std::unique_ptr<ItemReservation> reservation = std::make_unique<ItemReservation>(this, itemStack.id, std::move(targets), false /*isPromise*/);
        mReservations.insert(reservation.get());
        return reservation;
    }
    return nullptr;
}

CALLER_DELETE std::unique_ptr<ItemReservation> ItemStockpile::tryPromiseItemStack(ItemStack itemStack, ui32 minimumQuantity) {
    assert(itemStack.quantity >= minimumQuantity);
    assert(itemStack.quantity < UINT16_MAX);

    ItemRepository& itemRepo = ItemRepository::get();
    const ui32 maxStackSize = itemRepo.getLoadedOrUnloadedAsset(itemStack.id).getMaxStockpileStackSize();
    ui32 totalFreeSpace = maxStackSize * mFreeSlots;
    ui32 remainingQuantity;
    std::vector<ItemReservationTarget> targets;

    ItemStockpileRecord* record;
    auto&& it = mItemContents.find(itemStack.id);
    if (it != mItemContents.end()) {
        // We have existing stacks, use them first
        record = &it->second;
        totalFreeSpace += record->freeStackSpace;

        if (totalFreeSpace < minimumQuantity) {
            return nullptr;
        }

        remainingQuantity = std::min(totalFreeSpace, (ui32)itemStack.quantity);
        targets.reserve(remainingQuantity / maxStackSize); // Arbitrary

        // Loop through existing stacks and promise them
        for (const ui16& stackIndex : record->stackLocations) {
            ItemStockpileTileStorage& tileStorage = mStorage[stackIndex];
            const ui32 freeSpaceThisStack = maxStackSize - (tileStorage.stack.quantity + tileStorage.promiseCount);
            if (freeSpaceThisStack > 0) {
                OVERFLOW_ASSERT_UI32(freeSpaceThisStack);
                const ui32 promiseQuantityThisTile = std::min(freeSpaceThisStack, remainingQuantity);
                remainingQuantity -= promiseQuantityThisTile;
                OVERFLOW_ASSERT_UI32(remainingQuantity);
                record->promisedQuantity += promiseQuantityThisTile;
                tileStorage.promiseCount += promiseQuantityThisTile;
                // Promises count as occupying the stack space
                record->freeStackSpace -= promiseQuantityThisTile;
                targets.emplace_back(ItemReservationTarget{ stackIndex, (ui16)promiseQuantityThisTile });
                if (remainingQuantity == 0) {
                    break;
                }
            }
        }
    }
    else {
        if (totalFreeSpace < minimumQuantity) {
            return nullptr;
        }

        remainingQuantity = std::min(totalFreeSpace, (ui32)itemStack.quantity);
        targets.reserve(remainingQuantity / maxStackSize); // Arbitrary

        // Add the record since we will be allocating new stacks for promises
        record = &mItemContents[itemStack.id];
    }

    if (remainingQuantity > 0) {
        // Loop through free slots in the stockpile and allocate them to this item as promise
        for (; mFirstFreeSlot < mStorage.size(); ++mFirstFreeSlot) {
            ItemStockpileTileStorage& tileStorage = mStorage[mFirstFreeSlot];
            if (!tileStorage.isInvalidStorage() && tileStorage.isNull()) {
                const ui32 promiseQuantityThisTile = std::min(maxStackSize, remainingQuantity);
                record->freeStackSpace += maxStackSize - promiseQuantityThisTile;
                record->stackLocations.emplace_back(mFirstFreeSlot);
                record->promisedQuantity += promiseQuantityThisTile;
                tileStorage.promiseCount += promiseQuantityThisTile;
                tileStorage.stack.id = itemStack.id;
                assert(tileStorage.stack.quantity == 0);
                remainingQuantity -= promiseQuantityThisTile;
                --mFreeSlots;
                targets.emplace_back(ItemReservationTarget{ (ui16)mFirstFreeSlot, (ui16)promiseQuantityThisTile });
                if (remainingQuantity == 0) {
                    ++mFirstFreeSlot;
                    break;
                }
            }
        }
    }

    // Create the item reservation
    assert(remainingQuantity == 0);
    std::unique_ptr<ItemReservation> reservation = std::make_unique<ItemReservation>(this, itemStack.id, std::move(targets), true /*isPromise*/);
    mReservations.insert(reservation.get());
    return reservation;
}

ui32v2 ItemStockpile::getWorldPositionAtIndex(ui32 index) const {
    ui32v2 xy = mAABB.getBottomLeft();
    xy.x += index & mAABB.dims.x;
    xy.y += index / mAABB.dims.x;
    return xy;
}

void ItemStockpile::releaseReservation(ItemReservation* reservation) {
    auto&& rit = mReservations.find(reservation);
    assert(rit != mReservations.end());
    mReservations.erase(rit);
    const ui32 remaining = reservation->getRemainingQuantity();
    if (remaining != 0) {
        auto&& mit = mItemContents.find(reservation->getItemID());
        assert(mit != mItemContents.end());
        ItemStockpileRecord& record = mit->second;
        if (reservation->isPromise()) {
            assert(record.promisedQuantity >= remaining);
            record.promisedQuantity -= remaining;
            record.freeStackSpace += remaining;
            for (auto&& target : reservation->mTargets) {
                ItemStockpileTileStorage& tileStorage = mStorage[target.index];
                assert(tileStorage.promiseCount >= target.quantity);
                tileStorage.promiseCount -= target.quantity;
                if (tileStorage.isNull()) {
                    if (freeSlot(tileStorage, record, mit, reservation->getItemID(), target.index)) {
                        break;
                    }
                }
            }
        }
        else {
            assert(mit->second.reservedQuantity >= remaining);
            mit->second.reservedQuantity -= remaining;
            for (auto&& target : reservation->mTargets) {
                ItemStockpileTileStorage& storage = mStorage[target.index];
                assert(storage.reserveCount >= target.quantity);
                storage.reserveCount -= target.quantity;
            }
        }
    }
}

bool ItemStockpile::itemReservationFulfullCurrentTarget(ItemReservation* reservation, OUT ItemStack& sourceStack) {
    assert(sourceStack.id == reservation->mItemID);
    ItemReservationTarget& target = reservation->mTargets.back();
    ItemStockpileTileStorage& tileStorage = mStorage[target.index];

    auto&& mit = mItemContents.find(reservation->getItemID());
    assert(mit != mItemContents.end());
    ItemStockpileRecord& record = mit->second;


    if (reservation->mIsPromise) {
        const ui32 transferQuantity = std::min(target.quantity, (ui16)sourceStack.quantity);
        target.quantity -= transferQuantity;
        reservation->mRemainingQuantity -= transferQuantity;
        assert(sourceStack.quantity);
        assert(tileStorage.promiseCount >= transferQuantity);
        assert(record.promisedQuantity >= transferQuantity);
        sourceStack.quantity -= transferQuantity;
        tileStorage.stack.quantity += transferQuantity;
        tileStorage.promiseCount -= transferQuantity;
        record.promisedQuantity -= transferQuantity;
        record.totalQuantity += transferQuantity;
    }
    else {
        const ui32 transferQuantity = target.quantity;
        target.quantity -= transferQuantity;
        reservation->mRemainingQuantity -= transferQuantity;
        assert(tileStorage.stack.quantity >= transferQuantity);
        assert(tileStorage.reserveCount >= transferQuantity);
        assert(record.reservedQuantity >= transferQuantity);
        sourceStack.quantity += transferQuantity;
        tileStorage.stack.quantity -= transferQuantity;
        tileStorage.reserveCount -= transferQuantity;
        record.reservedQuantity -= transferQuantity;
        record.totalQuantity -= transferQuantity;
        // Taking items increases free stack space
        record.freeStackSpace += transferQuantity;
        // If we cleared out the tile, free it
        if (tileStorage.isNull()) {
            freeSlot(tileStorage, record, mit, sourceStack.id, target.index);
        }
    }

    dispatchEdit(ItemStockpileEvent{ this, sourceStack.id });

    if (target.quantity == 0) {
        assert(&target == &reservation->mTargets.back());
        reservation->mTargets.pop_back();

        if (reservation->isFinished()) {
            reservation->release();
            return true;
        }
    }
    return false;
}

bool ItemStockpile::freeSlot(ItemStockpileTileStorage& tileStorage, ItemStockpileRecord& record, std::unordered_map<ItemID, ItemStockpileRecord>::const_iterator& iterator, ItemID itemId, ui16 stackIndex) {
    tileStorage.stack.id = INVALID_ITEM_ID;


    // If this tile storage is now invalid, its counted
    // as a free slot, and we remove the stack size from the free stack space (or just delete the record if we have none at all)
    // since its no longer listed as a stack of this type
    ++mFreeSlots;
    if (stackIndex < mFirstFreeSlot) {
        mFirstFreeSlot = stackIndex;
    }

    if (record.isNull()) {
        assert(record.stackLocations.empty());
        mItemContents.erase(iterator);
        return true;
    }
    else {
        // Remove stack index
        bool didRemove = false;
        for (size_t i = 0; i < record.stackLocations.size(); ++i) {
            if (record.stackLocations[i] == stackIndex) {
                record.stackLocations[i] = record.stackLocations.back();
                record.stackLocations.pop_back();
                didRemove = true;
            }
        }
        assert(didRemove);

        ItemRepository& itemRepo = ItemRepository::get();
        const ui32 maxStackSize = itemRepo.getLoadedOrUnloadedAsset(itemId).getMaxStockpileStackSize();
        assert(record.freeStackSpace >= maxStackSize);
        record.freeStackSpace -= maxStackSize;
        return false;
    }
}

std::unique_ptr<ItemReservation> ItemStockpile::splitReservation(ItemReservation* reservation, ui16 splitQuantity) {
    assert(splitQuantity);
    assert(reservation->isValid());
    assert(reservation->mTargets.size());
    assert(splitQuantity < reservation->getRemainingQuantity());

    std::vector<ItemReservationTarget> targets;
    targets.reserve((ui32)(reservation->mTargets.size() / 2)); // Arbitrary

    ui16 remainingQuantity = splitQuantity;

    for (int i = (int)reservation->mTargets.size() - 1; i >= 0; --i) {
        if (remainingQuantity == 0) break;

        ItemReservationTarget& existingTarget = reservation->mTargets[i];
        if (existingTarget.quantity <= remainingQuantity) {
            remainingQuantity -= existingTarget.quantity;
            // Just swap it over
            targets.push_back(existingTarget);
            reservation->mTargets.pop_back();
        }
        else {
            // Last one, split it into two
            targets.emplace_back(ItemReservationTarget{ existingTarget.index, remainingQuantity });
            existingTarget.quantity -= remainingQuantity;
            remainingQuantity = 0;
            break;
        }
    }

    reservation->mRemainingQuantity -= splitQuantity;

    assert(remainingQuantity == 0);
    assert(targets.size());
    std::unique_ptr<ItemReservation> newReservation = std::make_unique<ItemReservation>(this, reservation->mItemID, std::move(targets), reservation->mIsPromise);
    mReservations.insert(newReservation.get());
    return newReservation;
}
