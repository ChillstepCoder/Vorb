#pragma once

#include "item/ItemStockpile.h"
#include "world/ChunkID.h"    

class World;

class ItemStockpileRegistry {
public:
    ItemStockpileRegistry(World& world);
    ~ItemStockpileRegistry();

    ItemStockpile* tryCreateStockpileAt(const ui32AABB2& aabb);
    void destroyStockpile(ItemStockpile* stockpile);

    const std::vector<ItemStockpile*>* tryGetStockpilesAtChunkPosition(ChunkID chunkID) const;
    const std::vector<std::unique_ptr<ItemStockpile>>& getAllStockpiles() const { return mAllStockpiles; }

private:
    bool checkStockpileOverlap(const ui32AABB2& aabb) const;
    void addStockpileToAreaLookup(ItemStockpile& stockpile);
    void removeStockpileFromAreaLookup(ItemStockpile& stockpile);

    std::vector<std::unique_ptr<ItemStockpile>> mAllStockpiles;
    World& mWorld;

    std::unordered_map<ChunkID, std::vector<ItemStockpile*>> mAreaLookup;
};

