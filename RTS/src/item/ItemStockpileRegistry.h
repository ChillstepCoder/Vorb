#pragma once

#include "item/ItemStockpile.h"

class World;

class ItemStockpileRegistry {
public:
    ItemStockpileRegistry(World& world);
    ~ItemStockpileRegistry();

    ItemStockpile* tryCreateStockpileAt(const ui32AABB2& aabb);
    

private:
    bool checkStockpileOverlap(const ui32AABB2& aabb) const;

    std::vector<std::unique_ptr<ItemStockpile>> mAllStockpiles;
    World& mWorld;
};

