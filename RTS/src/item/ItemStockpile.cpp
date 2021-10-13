#include "stdafx.h"
#include "ItemStockpile.h"

#include "DebugRenderer.h"
#include "World.h"
#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"

ItemReservation::ItemReservation(ItemStockpile* stockpile, ItemStack stack) :
    mStockpile(stockpile), mReservedItemStack(stack) {

}

ItemReservation::~ItemReservation() {
    if (mStockpile) {
        release();
    }
}

void ItemReservation::release() {
    assert(mStockpile);
    mStockpile->releaseReservation(this);
    mStockpile = nullptr;
}

bool ItemReservation::fulfillQuantity(ui32 quantity) {
    assert(quantity <= mReservedItemStack.quantity);
    mReservedItemStack.quantity -= quantity;
    if (mReservedItemStack.quantity == 0) {
        release();
        return true;
    }
    return false;
}

ItemStockpile::ItemStockpile(World& world, const ui32AABB2& aabb, entt::entity ownerEntity /*= INVALID_ENTITY*/)
    : mWorld(world)
    , mAABB(aabb)
    , mOwnerEntity(ownerEntity) {

    assert(mAABB.width <= MAX_STOCKPILE_WIDTH && mAABB.height <= MAX_STOCKPILE_WIDTH);

    mStorage.resize(mAABB.width * mAABB.height);

    // Set stockpile flags
    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.height; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.width; ++x) {
            TileRef(world.getTileHandleAtWorldPos(ui32v2(x, y))).tile->setTileFlag(TILE_FLAG_IS_STOCKPILE);
        }
    }

}

ItemStockpile::~ItemStockpile() {
    // TODO: Run a function on the reservation?
    for (auto&& it : mReservations) {
        it->mStockpile = nullptr;
    }
}

void ItemStockpile::renderDebug() const {
    DebugRenderer::drawQuad(f32v2(mAABB.pos), f32v2(mAABB.dims), color4(1.0f, 1.0f, 0.0f, 0.3f));
}

ItemStack ItemStockpile::tryAddItemStackAt (ItemStack itemStack, ui32v2 pos, ui32 maxQuantityToAdd) {

    const ui32 stackQuantity = glm::min(maxQuantityToAdd, itemStack.quantity);
    const ui32 index = (pos.y - mAABB.y) * mAABB.width + pos.x - mAABB.x;
    assert(index < mStorage.size());
    ItemStack& existingStack = mStorage[index];



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
            if (existingStack.isNull() || (existingStack.id == stack.id && existingStack.quantity < stackSize)) {
                outPos->x = x;
                outPos->y = y;
                return true;
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
    if (record.totalQuantity - record.reservedQuantity >= minimumQuantity) {
        ItemStack stack;
        std::unique_ptr<ItemReservation> reservation
            = std::make_unique<ItemReservation>(this, stack);
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
