#pragma once

#include "Item.h"

#include "rendering/QuadMesh.h"

class ItemStockpile;
class World;

struct ItemStockpileRenderData {
    std::unique_ptr<BillboardMesh> mBillboardMesh;
    std::unique_ptr<QuadMesh> mQuadMesh;
    bool mBillboardMeshDirty = false;
    bool mQuadMeshDirty = false;
};

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

struct ItemStockpileRecord {
    ui32 totalQuantity;
    ui32 reservedQuantity;
    std::vector<ui32> stackLocations;
};

// Tracks the location, dimensions, and contents of a stockpile
// of items. Can be owned.
class ItemStockpile
{
    friend class ItemReservation;
    friend class ItemRenderer;
    friend class RenderContext;
public:
    ItemStockpile(World& world, const ui32AABB2& aabb, entt::entity ownerEntity = INVALID_ENTITY);
    ~ItemStockpile();

    bool isValid() { return mAABB.width != 0; } // If we have 0 width we are null

    void renderDebug() const;
    // Returns the leftover stack, if quantity is 0, itemStack was consumed
    ItemStack tryAddItemStackAt(ItemStack stack, ui32v2 pos);
    // Returns true if item stack can be partially placed, stores world position
    // in outPos
    bool tryGetBestPositionToInsertItemStack(ItemStack stack, OUT ui32v2* outPos);
    bool tryGetClosestPositionOfItem(const f32v2& pos, ItemID itemId, OUT ui32v2* outPos) const;
    CALLER_DELETE std::unique_ptr<ItemReservation> tryReserveItemStack(ItemStack itemStack, ui32 minimumQuantity);

    const ui32AABB2& getAABB() const { return mAABB; }

private:
    void releaseReservation(ItemReservation* reservation);
    void dirtyMeshForItem(const Item& item);

    // TODO: MultiAABB
    World& mWorld;
    ui32AABB2 mAABB = ui32AABB2(0);
    ui32 mZPos = 0; // TODO: Use this
    entt::entity mOwnerEntity = INVALID_ENTITY; // Business entity that owns this stockpile

    std::vector<ItemStack> mStorage;
    std::map<ItemID, ItemStockpileRecord> mItemContents;
    std::set<ItemReservation*> mReservations;
    ui32 mTotalItems = 0;

    mutable ItemStockpileRenderData mRenderData;

    // TODO: Allowed item tags
    // TODO: Priorities? May not need...
};

