#pragma once

#include "item/ItemStockpile.h"
#include "world/GridID.h"
#include "world/World.h"

class ItemStockpileRegistry {
public:
    ItemStockpileRegistry(World& world);
    ~ItemStockpileRegistry();

    ItemStockpile* tryCreateStockpileAt(const i32AABB2& aabb, OPT bool* ownershipMask, entt::entity ownerEntity);
    void destroyStockpile(ItemStockpile* stockpile);

    const std::vector<ItemStockpile*>* tryGetStockpilesAtTileContainer(TileContainerID containerId) const;

private:
    void addTerrainStockpileToAreaLookup(ItemStockpile& stockpile);
    void removeStockpileFromAreaLookup(ItemStockpile& stockpile);

    World& mWorld;
    UnorderedFlatMap<ItemStockpileID, std::unique_ptr<ItemStockpile>> mAllStockpiles;

    UnorderedFlatMap<TileContainerID, std::vector<ItemStockpile*>> mAreaLookup;
};

