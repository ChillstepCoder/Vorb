#include "stdafx.h"
#include "ItemStockpile.h"

#include "DebugRenderer.h"
#include "World.h"

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

ItemStockpile::ItemStockpile(World& world, const ui32v4& aabb, entt::entity ownerEntity /*= INVALID_ENTITY*/)
    : mWorld(world)
    , mAABB(aabb)
    , mOwnerEntity(ownerEntity) {

}

ItemStockpile::~ItemStockpile() {
    // TODO: Run a function on the reservation?
    for (auto&& it : mReservations) {
        it->mStockpile = nullptr;
    }
}

void ItemStockpile::renderDebug() const {
    DebugRenderer::drawQuad(f32v2(mAABB.x, mAABB.y), f32v2(mAABB.z, mAABB.w), color4(1.0f, 1.0f, 0.0f, 0.3f));
}

ItemStack ItemStockpile::tryAddItemStackAt (ItemStack stack, ui32v2 pos) {
    return mWorld.tryAddPartialItemStackAt(pos, stack);
}

bool ItemStockpile::tryGetBestPositionToInsertItemStack(ItemStack stack, OUT ui32v2* outPos) {
    // TODO: Optimize iteration
    // Greedy?
    // BFS?
    f32 closestDistSq = FLT_MAX;
    assert(outPos);

    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.w; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.z; ++x) {
            f32v2 tilePos(x, y);
            const ItemStack* item = mWorld.tryGetItemStackAtWorldPos(tilePos);
            if (!item || item->id == stack.id) {
                outPos->x = x;
                outPos->y = y;
                return true;
            }

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

    for (ui32 y = mAABB.y; y < mAABB.y + mAABB.w; ++y) {
        for (ui32 x = mAABB.x; x < mAABB.x + mAABB.z; ++x) {
            f32v2 tilePos(x, y);
            const ItemStack* item = mWorld.tryGetItemStackAtWorldPos(tilePos);
            if (item && item->id == itemId) {
                const f32v2 offset = tilePos - pos;
                const f32 dist2 = glm::length2(offset);
                if (dist2 < closestDistSq) {
                    closestDistSq = dist2;
                    found = true;
                    outPos->x = x;
                    outPos->y = y;
                }
            }
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
    ItemRecord& record = it->second;
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
