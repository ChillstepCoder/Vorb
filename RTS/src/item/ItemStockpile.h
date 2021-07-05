#pragma once

#include "Item.h"

class ItemStockpile;
class World;

// TODO: notify destruction
// Record of reservation of items at a particular stockpile
class ItemReservation {
public:
    friend class ItemStockpile;

    ItemReservation(ItemStockpile* stockpile, ItemStack stack);
    ~ItemReservation();

    // Accessors
    ItemID getItemID() { return mReservedItemStack.id; }
    ui32 getRemainingQuantity() { return mReservedItemStack.quantity; }

    // Mutators
    void release();
    bool isValid() { return mStockpile != nullptr; }
    // Return true when fully fullfilled
    bool fulfillQuantity(ui32 quantity);

private:
    ItemStockpile* mStockpile = nullptr;
    ItemStack mReservedItemStack;
};

// Tracks the location, dimensions, and contents of a stockpile
// of items. Can be owned.
class ItemStockpile
{
    friend class ItemReservation;

    ItemStockpile(World& world, const ui32v4& aabb, entt::entity ownerEntity = INVALID_ENTITY);
    ~ItemStockpile();

    bool isValid() { return mAABB.z != 0; } // If we have 0 width we are null

    void renderDebug() const;
    // Returns the leftover stack, if quantity is 0, itemStack was consumed
    ItemStack tryAddItemStackAt(ItemStack stack, ui32v2 pos);
    // Returns true if item stack can be partially placed, stores world position
    // in outPos
    bool tryGetBestPositionToInsertItemStack(ItemStack stack, OUT ui32v2* outPos);
    bool tryGetClosestPositionOfItem(const f32v2& pos, ItemID itemId, OUT ui32v2* outPos) const;
    CALLER_DELETE std::unique_ptr<ItemReservation> tryReserveItemStack(ItemStack itemStack, ui32 minimumQuantity);

private:
    void releaseReservation(ItemReservation* reservation);

    struct ItemRecord {
        ItemID item;
        ui32 totalQuantity;
        ui32 reservedQuantity;
    };

    // TODO: MultiAABB
    World& mWorld;
    ui32v4 mAABB = ui32v4(0);
    entt::entity mOwnerEntity = INVALID_ENTITY;
    std::map<ItemID, ItemRecord> mItemContents;
    std::set<ItemReservation*> mReservations;

    // TODO: Allowed item tags
    // TODO: Priorities? May not need...
};

