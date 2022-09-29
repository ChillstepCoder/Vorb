#pragma once

#include "item/ItemStockpile.h"
#include "world/ChunkID.h"    

class ItemStockpileRegistry {
public:
    ItemStockpileRegistry();
    ~ItemStockpileRegistry();

    ItemStockpile* tryCreateStockpileAt(const i32AABB2& aabb, OPT bool* ownershipMask, entt::entity ownerEntity);
    void destroyStockpile(ItemStockpile* stockpile);

    const std::vector<ItemStockpile*>* tryGetStockpilesAtChunkPosition(ChunkID chunkID) const;
    const std::vector<std::unique_ptr<ItemStockpile>>& getAllStockpiles() const { return mAllStockpiles; }

private:
    void addStockpileToAreaLookup(ItemStockpile& stockpile);
    void removeStockpileFromAreaLookup(ItemStockpile& stockpile);

    std::vector<std::unique_ptr<ItemStockpile>> mAllStockpiles;

    std::unordered_map<ChunkID, std::vector<ItemStockpile*>> mAreaLookup;
};

