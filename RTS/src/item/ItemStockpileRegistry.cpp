#include "stdafx.h"
#include "ItemStockpileRegistry.h"

#include "world/LocalChunkGrid.h"
#include "world/World.h"

static ItemStockpileID sItemStockpileIdGen;

ItemStockpileRegistry::ItemStockpileRegistry(World& world) : mWorld(world)
{

}

ItemStockpileRegistry::~ItemStockpileRegistry()
{

}

ItemStockpile* ItemStockpileRegistry::tryCreateStockpileAt(const i32AABB2& aabb, OPT bool* ownershipMask, entt::entity ownerEntity) {
    // Create new stockpile and leave unassigned (city ownership)
    ItemStockpileID id = sItemStockpileIdGen++;
    std::unique_ptr<ItemStockpile> newUnique = std::make_unique<ItemStockpile>(mWorld, id, aabb, ownershipMask, ownerEntity);
    ItemStockpile* newStockpile = newUnique.get();
    mAllStockpiles.insert(std::make_pair(id, std::move(newUnique)));

    if (sItemStockpileIdGen > INT32_MAX) {
        sItemStockpileIdGen = 0;
    }

    addTerrainStockpileToAreaLookup(*newStockpile);
    return newStockpile;
}

void ItemStockpileRegistry::destroyStockpile(ItemStockpile* stockpile)
{
    auto&& it = mAllStockpiles.find(stockpile->getId());
    assert(it != mAllStockpiles.end());
    removeStockpileFromAreaLookup(*stockpile);
    mAllStockpiles.erase(it);
}

const std::vector<ItemStockpile*>* ItemStockpileRegistry::tryGetStockpilesAtTileContainer(TileContainerID containerId) const {
    const auto& it = mAreaLookup.find(containerId);
    if (it == mAreaLookup.end()) {
        return nullptr;
    }
    return &it->second;
}

void ItemStockpileRegistry::addTerrainStockpileToAreaLookup(ItemStockpile& stockpile) {
    // TODO: I think this is bad
    // Get all possible chunks
    LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
    const i32AABB2& aabb = stockpile.getAABB();
    std::set<ChunkID> chunkPositions;
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos));
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos + i32v2(aabb.width, 0.0f)));
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos + i32v2(0.0f, aabb.depth)));
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos + i32v2(aabb.width, aabb.depth)));
    int i = 0;
    for (auto&& id : chunkPositions) {
        LocalChunk& chunk = chunkGrid.getChunk(id);
        if (chunk.getTileContainer()) {
            const TileContainerID chunkContainerId = chunk.getTileContainer()->getId();
            mAreaLookup[chunkContainerId].push_back(&stockpile);
            stockpile.mContainerDependencies[i++] = chunkContainerId;
        }
        else {
            assert(false);
        }
    }
}

void ItemStockpileRegistry::removeStockpileFromAreaLookup(ItemStockpile& stockpile) {
    // TODO: I think this is bad
    // Get all possible chunks
    LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
    const i32AABB2& aabb = stockpile.getAABB();
    std::set<ChunkID> chunkPositions;
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos));
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos + i32v2(aabb.width, 0.0f)));
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos + i32v2(0.0f, aabb.depth)));
    chunkPositions.insert(chunkGrid.getChunkIDFromWorldPos(aabb.pos + i32v2(aabb.width, aabb.depth)));
    int i = 0;
    for (auto&& id : chunkPositions) {
        LocalChunk& chunk = chunkGrid.getChunk(id);
        assert(chunk.getTileContainer());
        const auto& it = mAreaLookup.find(chunk.getTileContainer()->getId());
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
