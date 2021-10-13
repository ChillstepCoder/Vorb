#include "stdafx.h"
#include "CityQuartermaster.h"
#include "city/City.h"
#include "World.h"

#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"

CityQuartermaster::CityQuartermaster(City& city) : mCity(city) {

}

CityQuartermaster::~CityQuartermaster() {

}

bool CityQuartermaster::tryCreateCityStockpileAt(const ui32AABB2& aabb) {

    ItemStockpile* newStockpile = mCity.mWorld.getItemStockpileRegistry().tryCreateStockpileAt(aabb);
    
    // Create new stockpile and leave unassigned (city ownership)
    if (newStockpile) {
        mAllStockpiles.emplace_back(std::make_unique<ItemStockpile>(mCity.mWorld, aabb));
        return true;
    }
    return false;
}

ItemStockpile* CityQuartermaster::tryGetClosestStockpileToPoint(const ui32v2 position) {
    // TODO: Make sure that we can filter out full stockpiles
    ItemStockpile* best = nullptr;
    f32 bestDistance2 = FLT_MAX;
    // TODO: Heuristic
    // Morton order?
    for (auto& stockpile : mAllStockpiles) {
        ui32v2 offset = stockpile->getAABB().pos - position;
        const f32 distance2 = glm::length2(f32v2(offset));
        if (distance2 < bestDistance2) {
            best = stockpile;
            bestDistance2 = distance2;
        }
    }

    return best;
}

