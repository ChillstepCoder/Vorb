#pragma once

#include "Item.h"

#include "world/ChunkID.h"
#include "util/BitArray.h"
#include "ItemReservation.h"
#include "ItemStockpileEvent.h"

class Camera3D;

constexpr ui32 MAX_STOCKPILE_WIDTH = CHUNK_WIDTH / 2;

// TODO: ui16?
struct ItemStockpileRecord {
    ui32 totalQuantity;
    ui32 reservedQuantity;
    ui32 promisedQuantity;
    ui32 freeStackSpace;
    std::vector<ui16> stackLocations; // TODO: Custom allocator

    bool isNull() const { return totalQuantity == 0 && promisedQuantity == 0; }
};

struct ItemStockpileTileStorage {
    ItemStack stack;
    ui16 promiseCount = 0u;
    ui16 reserveCount = 0u;

    bool isNull() const { return promiseCount == 0 && stack.quantity == 0; }
    bool isInvalidStorage() const { return stack.id == INVALID_STOCKPILE_INDEX; }
};
static_assert(sizeof(ItemStockpileTileStorage) == 8, "Keep small");

// All data needed to mesh a stockpile
struct ItemStockpileMeshDataCopy {
    // TODO:
};

// Tracks the location, dimensions, and contents of a stockpile
// of items. Can be owned.
class ItemStockpile
{
    friend class ItemReservation;
    friend class ItemPromise;
    friend class ItemRenderer;
    friend class ItemStockpileRegistry;
    friend class RenderContext;
public:
    ItemStockpile(ItemStockpileID id, const i32AABB2& aabb, OPT bool* ownershipMask, entt::entity ownerEntity = INVALID_ENTITY);
    ~ItemStockpile();

    bool isValid() const { return mAABB.width != 0; } // If we have 0 width we are null

    void renderDebug() const;

    // Returns true if item stack can be partially placed, stores world position in outPos
    CALLER_DELETE std::unique_ptr<ItemReservation> tryReserveItemStack(ItemStack itemStack, ui32 minimumQuantity);
    CALLER_DELETE std::unique_ptr<ItemReservation> tryPromiseItemStack(ItemStack itemStack, ui32 minimumQuantity);

    ui32v2 getWorldPositionAtIndex(ui32 index) const;
    const i32AABB2& getAABB() const { return mAABB; }
    ItemStockpileID getId() const { return mId; }

    // =========== Refcount  ===========
    inline void incRef() const {
        assert(IS_GAME_THREAD()); // Only main thread is allowed to incref
        assert(mRefCount.load() < 2000u); // This is probably a sign of something really awful
        ++mRefCount;
        if (mRefCount > 400) {
            std::cout << "DETECTED " << mRefCount << " REF COUNTS ON ITEM STOCKPILE " << std::endl;
            assert(false && "Too many container refcounts");
        }
    }
    inline void decRef() const {
        assert(mRefCount.load());
        --mRefCount;
    }
    ui32 getRefCount() const { return mRefCount; }

    // Events
    STATIC_EVENT_LISTENER_FUNCS(ItemStockpile, Create, ItemStockpileEventType::Create, const ItemStockpileEvent&);
    STATIC_EVENT_LISTENER_FUNCS(ItemStockpile, Edit, ItemStockpileEventType::Edit, const ItemStockpileEvent&);
    STATIC_EVENT_LISTENER_FUNCS(ItemStockpile, Destroy, ItemStockpileEventType::Destroy, const ItemStockpileEvent&);
    STATIC_EVENT_DISPATCHER(ItemStockpile);
private:
    void releaseReservation(ItemReservation* reservation);

    // Return true if fully fulfilled
    bool itemReservationFulfullCurrentTarget(ItemReservation* reservation, OUT ItemStack& sourceStack);

    // Returns true if the record is invalidated due to all stacks now being free
    bool freeSlot(ItemStockpileTileStorage& tileStorage, ItemStockpileRecord& record, std::unordered_map<ItemID, ItemStockpileRecord>::const_iterator& iterator, ItemID itemId, ui16 stackIndex);

    std::unique_ptr<ItemReservation> splitReservation(ItemReservation* reservation, ui16 splitQuantity);
    
    std::vector<ChunkID> mResidingChunks; // TODO: Share dependency logic with tilecontainer? No instead we need tile container dependencies
    std::vector<ItemStockpileTileStorage> mStorage;
    std::unordered_map<ItemID, ItemStockpileRecord> mItemContents;
    std::unordered_set<ItemReservation*> mReservations;

    i32AABB2 mAABB = i32AABB2(0);
    f32 mZPos = 0; // TODO: Use this better
    entt::entity mOwnerEntity = INVALID_ENTITY; // Business entity that owns this stockpile

    ui32 mTotalItems = 0;
    ui32 mTotalSlots = 0;
    ui32 mFreeSlots = 0;
    ui32 mFirstFreeSlot = 0;
    ItemStockpileID mId;

    mutable std::atomic_uint32_t mRefCount = 0u;

};
