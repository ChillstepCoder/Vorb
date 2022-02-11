#include "stdafx.h"
#include "ItemStockpile.h"

#include "DebugRenderer.h"
#include "World.h"
#include "world/WorldGrid.h"
#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"

ItemStockpile::ItemStockpile(World& world, const ui32AABB2& aabb, entt::entity ownerEntity /*= INVALID_ENTITY*/)
    : mWorld(world)
    , mAABB(aabb)
    , mOwnerEntity(ownerEntity) {

    assert(mAABB.width <= MAX_STOCKPILE_WIDTH && mAABB.height <= MAX_STOCKPILE_WIDTH);

    mStorage.resize(mAABB.width * mAABB.height);

    f32 maxZPos = FLT_MIN;
    const WorldGrid& worldGrid = world.getWorldGrid();
    // Set stockpile flags
    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.height; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.width; ++x) {

            TileRef ref(world.getTileHandleAtWorldPos(ui32v2(x, y)));
            if (ref.tile->hasFlagMainThread(TILE_FLAG_IS_STOCKPILE)) {
                // If there is already a stockpile here, we are invalid
                mStorage[y * mAABB.dims.x + x].id = INVALID_STOCKPILE_INDEX;
            }
            else {
                // Valid slot
                ++mTotalSlots;
                ref.chunk->setTileFlag(ref.index, TILE_FLAG_IS_STOCKPILE);
                f32 height = worldGrid.computeMaxHeightAtTile(ref.chunk->getChunkID(), ref.index);
                if (height > maxZPos) maxZPos = height;
            }
        }
    }
    mZPos = maxZPos;
    // We must have at least one slot
    assert(mTotalSlots);
}

ItemStockpile::ItemStockpile(World& world, const ui32AABB2& aabb, bool* ownershipMask, entt::entity ownerEntity /*= INVALID_ENTITY*/)
    : mWorld(world)
    , mAABB(aabb)
    , mOwnerEntity(ownerEntity) {

    assert(mAABB.width <= MAX_STOCKPILE_WIDTH && mAABB.height <= MAX_STOCKPILE_WIDTH);

    mStorage.resize(mAABB.width * mAABB.height);

    f32 maxZPos = FLT_MIN;
    const WorldGrid& worldGrid = world.getWorldGrid();
    // Set stockpile flags
    ui32 index = 0;
    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.height; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.width; ++x) {
            TileRef ref(world.getTileHandleAtWorldPos(ui32v2(x, y)));
            if (ownershipMask[index] == false || ref.tile->hasFlagMainThread(TILE_FLAG_IS_STOCKPILE)) {
                // If there is already a stockpile here, we are invalid
                mStorage[index].id = INVALID_STOCKPILE_INDEX;
            }
            else {
                // Valid slot
                ++mTotalSlots;
                ref.chunk->setTileFlag(ref.index, TILE_FLAG_IS_STOCKPILE);
                f32 height = worldGrid.computeMaxHeightAtTile(ref.chunk->getChunkID(), ref.index);
                if (height > maxZPos) maxZPos = height;
            }
            ++index;
        }
    }
    mZPos = maxZPos;
    // We must have at least one slot
    assert(mTotalSlots);
}

ItemStockpile::~ItemStockpile() {

    onDestroy(this);

    // TODO: Run a function on the reservation?
    for (auto&& it : mReservations) {
        it->mStockpile = nullptr;
    }
}

bool ItemStockpile::isVisible() const {
    for (const ChunkID& chunkId : mResidingChunks) {
        if (mWorld.getWorldGrid().getChunk(chunkId).isVisible()) {
            return true;
        }
    }
    return false;
}

void ItemStockpile::renderDebug() const {
    ui32 index = 0;
    f32v2 cornerPos = f32v2(mAABB.pos);
    DebugRenderer::reserveFilledQuads(mAABB.dims.x * mAABB.dims.y);
    for (ui32 y = 0; y < mAABB.dims.y; ++y) {
        for (ui32 x = 0; x < mAABB.dims.x; ++x) {
            if (mStorage[index].id != INVALID_STOCKPILE_INDEX) {
                if (mStorage[index].isNull()) {
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

ItemStack ItemStockpile::tryAddItemStackAt (ItemStack itemStack, ui32v2 pos, ui32 maxQuantityToAdd) {

    const ui32 stackQuantity = glm::min(maxQuantityToAdd, itemStack.quantity);
    const ui32 index = (pos.y - mAABB.y) * mAABB.width + pos.x - mAABB.x;
    assert(index < mStorage.size());
    ItemStack& existingStack = mStorage[index];
    assert(existingStack.id != INVALID_STOCKPILE_INDEX);


    ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
    const Item& item = itemRepo.getItem(itemStack.id);
    const ui32 stackSize = item.getStackSize();

    if (existingStack.isNull()) {
        // We can put the full stack here
        // TODO: Make sure the current stack isn't overfull?
        const ui32 quantityToAdd = std::min(stackSize, stackQuantity);
        existingStack.id = itemStack.id;
        existingStack.quantity = quantityToAdd;
        itemStack.quantity -= quantityToAdd;
        dirtyMeshForItem(item);
        mRenderData.mBillboardMeshDirty = true;

        // Update the record
        ItemStockpileRecord& record = mItemContents[itemStack.id];
        record.totalQuantity += quantityToAdd;
        record.stackLocations.push_back(index);
    }
    else if (existingStack.id == itemStack.id) {

        if (stackSize == existingStack.quantity) {
            return itemStack; // Fail
        }

        // Check if we can fit our entire stack on existing stack
        const ui32 newTotal = existingStack.quantity + stackQuantity;
        ui32 quantityToAdd = stackQuantity;

        if (newTotal > stackSize) {
            // Can't fit full stack
            quantityToAdd = stackSize - existingStack.quantity;
        }
        // Increase existing stack size
        existingStack.quantity += quantityToAdd;
        itemStack.quantity -= quantityToAdd;
        mTotalItems += quantityToAdd;
        assert(mTotalItems < 100000);
        dirtyMeshForItem(item);

        assert(existingStack.quantity <= stackSize);

        // Update the record
        ItemStockpileRecord& record = mItemContents[itemStack.id];
        record.totalQuantity += existingStack.quantity;
    }
    return itemStack;
}

bool ItemStockpile::tryGetBestPositionToInsertItemStack(ItemStack stack, OUT ui32v2* outPos) {
    // TODO: Optimize iteration
    // Greedy?
    // BFS?
    f32 closestDistSq = FLT_MAX;
    assert(outPos);

    ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
    const ui32 stackSize = itemRepo.getItem(stack.id).getStackSize();

    ui32 index = 0;
    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.height; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.width; ++x) {
            ItemStack& existingStack = mStorage[index];
            if (existingStack.id != INVALID_STOCKPILE_INDEX) {
                if (existingStack.isNull() || (existingStack.id == stack.id && existingStack.quantity < stackSize)) {
                    outPos->x = x;
                    outPos->y = y;
                    return true;
                }
            }
            ++index;
        }
    }
    return false;
}

bool ItemStockpile::tryGetClosestPositionOfItem(const f32v2& pos, ItemID itemId, OUT ui32v2* outPos) const {
    // TODO: Optimize iteration
    // Greedy?
    // BFS?
    f32 closestDistSq = FLT_MAX;
    assert(outPos);
    bool found = false;

    ui32 index = 0;
    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.height; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.width; ++x) {
            f32v2 tilePos(x, y);
            const ItemStack& existingStack = mStorage[index];
            if (!existingStack.isNull() && existingStack.id == itemId) {
                const f32v2 offset = tilePos - pos;
                const f32 dist2 = glm::length2(offset);
                if (dist2 < closestDistSq) {
                    closestDistSq = dist2;
                    found = true;
                    outPos->x = x;
                    outPos->y = y;
                }
            }
            ++index;
        }
    }
    return found;
}

CALLER_DELETE std::unique_ptr<ItemReservation> ItemStockpile::tryReserveItemStack(ItemStack itemStack, ui32 minimumQuantity) {
    assert(itemStack.quantity > minimumQuantity);
    auto&& it = mItemContents.find(itemStack.id);
    if (it == mItemContents.end()) {
        return nullptr;
    }
    ItemStockpileRecord& record = it->second;
    const ui32 availableQuantity = record.totalQuantity - record.reservedQuantity;
    if (availableQuantity >= minimumQuantity) {
        // Reserve the stack
        ItemStack reserveStack;
        reserveStack.id = itemStack.id;
        reserveStack.quantity = std::min(availableQuantity, itemStack.quantity);
        record.reservedQuantity += reserveStack.quantity;
        std::unique_ptr<ItemReservation> reservation = std::make_unique<ItemReservation>(this, std::move(reserveStack));
        mReservations.insert(reservation.get());
        return reservation;
    }
    return nullptr;
}

void ItemStockpile::releaseReservation(ItemReservation* reservation) {
    auto&& rit = mReservations.find(reservation);
    assert(rit != mReservations.end());
    mReservations.erase(rit);
    const ui32 remaining = reservation->getRemainingQuantity();
    if (remaining != 0) {
        auto&& mit = mItemContents.find(reservation->getItemID());
        assert(mit != mItemContents.end());
        assert(mit->second.reservedQuantity >= remaining);
        mit->second.reservedQuantity -= remaining;
    }
}

void ItemStockpile::dirtyMeshForItem(const Item& item) {
    // Decide which mesh to dirty based on our material/shape
    if (item.mShape >= ItemStorageShape::QUAD_SHAPES_START) {
        mRenderData.mQuadMeshDirty = true;
    }
    else {
        mRenderData.mBillboardMeshDirty = true;
    }
}

bool ItemStockpile::itemReservationFulfullQuantity(ItemReservation* reservation, ui32 quantity) {
    reservation->mReservedItemStack.quantity -= quantity;
    auto&& mit = mItemContents.find(reservation->getItemID());
    assert(mit != mItemContents.end());
    assert(mit->second.reservedQuantity >= quantity);
    mit->second.reservedQuantity -= quantity;
    if (reservation->mReservedItemStack.quantity == 0) {
        mReservations.erase(reservation);
        return true;
    }
    return false;
}

std::unique_ptr<ItemReservation> ItemStockpile::splitReservation(ItemReservation* reservation, ui32 splitQuantity) {
    assert(splitQuantity < reservation->mReservedItemStack.quantity);
    ItemStack reserveStack;
    reserveStack.id = reservation->mReservedItemStack.id;
    reserveStack.quantity = splitQuantity;
    reservation->mReservedItemStack.quantity -= splitQuantity;
    std::unique_ptr<ItemReservation> newReservation = std::make_unique<ItemReservation>(this, std::move(reserveStack));
    mReservations.insert(newReservation.get());
    return newReservation;
}
