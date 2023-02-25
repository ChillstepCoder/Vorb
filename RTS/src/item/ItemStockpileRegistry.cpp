#include "stdafx.h"
#include "ItemStockpileRegistry.h"

#include "world/IWorld.h"

static ItemStockpileID sItemStockpileIdGen;

ItemStockpileRegistry::ItemStockpileRegistry()
{

}

ItemStockpileRegistry::~ItemStockpileRegistry()
{

}

ItemStockpile* ItemStockpileRegistry::tryCreateStockpileAt(const i32AABB2& aabb, OPT bool* ownershipMask, entt::entity ownerEntity) {
    // Create new stockpile and leave unassigned (city ownership)
    ItemStockpileID id = sItemStockpileIdGen++;
    std::unique_ptr<ItemStockpile> newUnique = std::make_unique<ItemStockpile>(id, aabb, ownershipMask, ownerEntity);
    ItemStockpile* newStockpile = newUnique.get();
    mAllStockpiles.insert(std::make_pair(id, std::move(newUnique)));

    if (sItemStockpileIdGen > INT32_MAX) {
        sItemStockpileIdGen = 0;
    }

    addStockpileToAreaLookup(*newStockpile);
    return newStockpile;
}

void ItemStockpileRegistry::destroyStockpile(ItemStockpile* stockpile)
{
    auto&& it = mAllStockpiles.find(stockpile->getId());
    assert(it != mAllStockpiles.end());
    removeStockpileFromAreaLookup(*stockpile);
    mAllStockpiles.erase(it);
}

const std::vector<ItemStockpile*>* ItemStockpileRegistry::tryGetStockpilesAtChunkPosition(ChunkID chunkID) const {
    const auto& it = mAreaLookup.find(chunkID);
    if (it == mAreaLookup.end()) {
        return nullptr;
    }
    return &it->second;
}

void ItemStockpileRegistry::addStockpileToAreaLookup(ItemStockpile& stockpile) {
    // Get all possible chunks
    const i32AABB2& aabb = stockpile.getAABB();
    std::set<ChunkID> chunkPositions;
    chunkPositions.insert(ChunkID(f32v2(aabb.pos)));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + i32v2(aabb.width, 0.0f))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + i32v2(0.0f, aabb.depth))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + i32v2(aabb.width, aabb.depth))));
    stockpile.mResidingChunks.reserve(chunkPositions.size());
    for (auto&& id : chunkPositions) {
        mAreaLookup[id].push_back(&stockpile);
        // Tell the stockpile what chunks it resides in
        stockpile.mResidingChunks.push_back(id);
    }
}

void ItemStockpileRegistry::removeStockpileFromAreaLookup(ItemStockpile& stockpile) {
    // Get all possible chunks
    const i32AABB2& aabb = stockpile.getAABB();
    std::set<ChunkID> chunkPositions;
    chunkPositions.insert(ChunkID(f32v2(aabb.pos)));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + i32v2(aabb.width, 0.0f))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + i32v2(0.0f, aabb.depth))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + i32v2(aabb.width, aabb.depth))));
    for (auto&& id : chunkPositions) {
        const auto& it = mAreaLookup.find(id);
        assert(it != mAreaLookup.end());
        auto& arry = it->second;
        for (size_t i = 0; i < arry.size(); ++i) {
            if (arry[i] == &stockpile) {
                arry[i] = arry.back();
                arry.pop_back();
                break;
            }
        }
        if (arry.empty()) {
            mAreaLookup.erase(it);
        }
    }
}
