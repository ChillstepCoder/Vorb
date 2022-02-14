#pragma once

#include "Item.h"

#include "rendering/QuadMesh.h"
#include "world/ChunkID.h"
#include "util/BitArray.h"
#include "ItemReservation.h"

constexpr ui32 MAX_STOCKPILE_WIDTH = CHUNK_WIDTH / 2;

class ItemStockpile;
class World;

struct ItemStockpileRenderData {
    std::unique_ptr<BillboardMesh> mBillboardMesh;
    std::unique_ptr<QuadMesh> mQuadMesh;
    bool mBillboardMeshDirty = false;
    bool mQuadMeshDirty = false;
};

// TODO: ui16?
struct ItemStockpileRecord {
    ui32 totalQuantity;
    ui32 reservedQuantity;
    ui32 promisedQuantity;
    ui32 freeStackSpace;
    std::vector<ui32> stackLocations;
};

enum class ItemStockpileTileStorageFlags : ui32 {
    IS_RESERVATION = 1 << 0,
    TEST_1 = 1 << 1,
    TEST_2 = 1 << 2
};

struct ItemStockpileTileStorage {
    ItemStack stack;
    BitFlags<ItemStockpileTileStorageFlags> flags;
};
static_assert(sizeof(ItemStockpileTileStorage) == 12);

// Tracks the location, dimensions, and contents of a stockpile
// of items. Can be owned.
class ItemStockpile
{
    friend class ItemReservation;
    friend class ItemRenderer;
    friend class ItemStockpileRegistry;
    friend class RenderContext;
public:
    ItemStockpile(World& world, const ui32AABB2& aabb, entt::entity ownerEntity = INVALID_ENTITY);
    ItemStockpile(World& world, const ui32AABB2& aabb, bool* ownershipMask, entt::entity ownerEntity = INVALID_ENTITY);
    ~ItemStockpile();

    bool isValid() const { return mAABB.width != 0; } // If we have 0 width we are null
    bool isVisible() const;

    void renderDebug() const;
    // Returns the leftover stack, if quantity is 0, itemStack was consumed
    ItemStack tryAddItemStackAt(ItemStack stack, ui32v2 pos, ui32 maxQuantityToAdd);
    // Returns true if item stack can be partially placed, stores world position
    // in outPos
    bool tryGetBestPositionToInsertItemStack(ItemStack stack, OUT ui32v2* outPos);
    bool tryGetClosestPositionOfItem(const f32v2& pos, ItemID itemId, OUT ui32v2* outPos) const;
    CALLER_DELETE std::unique_ptr<ItemReservation> tryReserveItemStack(ItemStack itemStack, ui32 minimumQuantity);

    const ui32AABB2& getAABB() const { return mAABB; }

    // Events
    Event<ItemStockpile*> onDestroy;

private:
    void releaseReservation(ItemReservation* reservation);
    void dirtyMeshForItem(const Item& item);
    // Return true if fully fulfilled
    bool itemReservationFulfullQuantity(ItemReservation* reservation, ui32 quantity);
    std::unique_ptr<ItemReservation> splitReservation(ItemReservation* reservation, ui32 splitQuantity);

    // TODO: MultiAABB
    World& mWorld;
    ui32AABB2 mAABB = ui32AABB2(0);

    f32 mZPos = 0; // TODO: Use this
    entt::entity mOwnerEntity = INVALID_ENTITY; // Business entity that owns this stockpile

    std::vector<ChunkID> mResidingChunks;
    std::vector<ItemStockpileTileStorage> mStorage;
    std::map<ItemID, ItemStockpileRecord> mItemContents;
    std::set<ItemReservation*> mReservations;
    ui32 mTotalItems = 0;
    ui32 mTotalSlots = 0;
    ui32 mFreeSlots = 0;

    mutable ItemStockpileRenderData mRenderData;

    // TODO: Allowed item tags
    // TODO: Priorities? May not need...
};

