#pragma once

#include "item/ItemStockpile.h"
#include "world/ChunkID.h"
#include "world/IWorld.h"

class ItemStockpileRegistry {
public:
    ItemStockpileRegistry(IWorld& world);
    ~ItemStockpileRegistry();

    ItemStockpile* tryCreateStockpileAt(const i32AABB2& aabb, OPT bool* ownershipMask, entt::entity ownerEntity);
    void destroyStockpile(ItemStockpile* stockpile);

    const std::vector<ItemStockpile*>* tryGetStockpilesAtTileContainer(TileContainerID containerId) const;

private:
    void addTerrainStockpileToAreaLookup(ItemStockpile& stockpile);
    void removeStockpileFromAreaLookup(ItemStockpile& stockpile);

    IWorld& mWorld;
    std::unordered_map<ItemStockpileID, std::unique_ptr<ItemStockpile>> mAllStockpiles;

    std::unordered_map<TileContainerID, std::vector<ItemStockpile*>> mAreaLookup;
};

