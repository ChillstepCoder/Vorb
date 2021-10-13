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

ItemStockpile* ItemStockpileRegistry::tryCreateStockpileAt(const ui32AABB2& aabb) {
    // TODO: Better spatial partition test?
    if (checkStockpileOverlap(aabb)) {
        return nullptr;
    }

    // Create new stockpile and leave unassigned (city ownership)
    mAllStockpiles.emplace_back(std::make_unique<ItemStockpile>(mWorld, aabb));
    return mAllStockpiles.back().get();
}

bool ItemStockpileRegistry::checkStockpileOverlap(const ui32AABB2& aabb) const {
    for (auto& stockpile : mAllStockpiles) {
        if (testAABBAABB_SIMD(stockpile->getAABB(), aabb)) {
            return true;
        }
    }
    return false;
}