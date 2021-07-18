#include "stdafx.h"
#include "CityQuartermaster.h"
#include "city/City.h"

#include "item/ItemStockpile.h"

CityQuartermaster::CityQuartermaster(City& city) : mCity(city) {

}

CityQuartermaster::~CityQuartermaster() {

}

bool CityQuartermaster::tryCreateCityStockpileAt(const ui32AABB& aabb) {
    if (checkStockpileOverlap(aabb)) {
        return false;
    }

    // Create new stockpile and leave unassigned (city ownership)
    mAllStockpiles.emplace_back(std::make_unique<ItemStockpile>(mCity.mWorld, aabb));
}

ItemStockpile* CityQuartermaster::tryGetClosestStockpileToPoint(const ui32v2 position) {
    // TODO: Make sure that we can filter out full stockpiles
    ItemStockpile* best = nullptr;
    f32 bestDistance2 = FLT_MAX;
    // TODO: Heuristic
    for (auto& stockpile : mAllStockpiles) {
        ui32v2 offset = stockpile->getAABB().pos - position;
        f32 distance2 = glm::length2(offset);
        if (distance2 < bestDistance2) {
            best = stockpile.get();
            bestDistance2 = distance2;
        }
    }

    return best;
}

bool CityQuartermaster::checkStockpileOverlap(const ui32AABB& aabb) const{
    for (auto& stockpile : mAllStockpiles) {
        if (testAABBAABB_SIMD(stockpile->getAABB(), aabb)) {
            return true;
        }
    }
    return false;
}
