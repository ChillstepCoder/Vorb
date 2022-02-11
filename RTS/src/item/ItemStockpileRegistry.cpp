#include "stdafx.h"
#include "ItemStockpileRegistry.h"

#include "World.h"


ItemStockpileRegistry::ItemStockpileRegistry(World& world)
    : mWorld(world)
{

}

ItemStockpileRegistry::~ItemStockpileRegistry()
{

}

ItemStockpile* ItemStockpileRegistry::tryCreateStockpileAt(const ui32AABB2& aabb, entt::entity ownerEntity) {

    // Create new stockpile and leave unassigned (city ownership)
    ItemStockpile* newStockpile = mAllStockpiles.emplace_back(std::make_unique<ItemStockpile>(mWorld, aabb, ownerEntity)).get();
    addStockpileToAreaLookup(*newStockpile);
    return newStockpile;
}

ItemStockpile* ItemStockpileRegistry::tryCreateStockpileAt(const ui32AABB2& aabb, bool* ownershipMask, entt::entity ownerEntity) {
    // Create new stockpile and leave unassigned (city ownership)
    ItemStockpile* newStockpile = mAllStockpiles.emplace_back(std::make_unique<ItemStockpile>(mWorld, aabb, ownershipMask, ownerEntity)).get();
    addStockpileToAreaLookup(*newStockpile);
    return newStockpile;
}

void ItemStockpileRegistry::destroyStockpile(ItemStockpile* stockpile)
{
    for (size_t i = 0; i < mAllStockpiles.size(); ++i) {
        if (mAllStockpiles[i].get() == stockpile) {
            removeStockpileFromAreaLookup(*stockpile);
            mAllStockpiles[i] = std::move(mAllStockpiles.back());
            mAllStockpiles.pop_back();
        }
    }
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
    const ui32AABB2& aabb = stockpile.getAABB();
    std::set<ChunkID> chunkPositions;
    chunkPositions.insert(ChunkID(f32v2(aabb.pos)));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + ui32v2(aabb.width, 0.0f))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + ui32v2(0.0f, aabb.height))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + ui32v2(aabb.width, aabb.height))));
    stockpile.mResidingChunks.reserve(chunkPositions.size());
    for (auto&& id : chunkPositions) {
        mAreaLookup[id].push_back(&stockpile);
        // Tell the stockpile what chunks it resides in
        stockpile.mResidingChunks.push_back(id);
    }
}

void ItemStockpileRegistry::removeStockpileFromAreaLookup(ItemStockpile& stockpile) {
    // Get all possible chunks
    const ui32AABB2& aabb = stockpile.getAABB();
    std::set<ChunkID> chunkPositions;
    chunkPositions.insert(ChunkID(f32v2(aabb.pos)));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + ui32v2(aabb.width, 0.0f))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + ui32v2(0.0f, aabb.height))));
    chunkPositions.insert(ChunkID(f32v2(aabb.pos + ui32v2(aabb.width, aabb.height))));
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
