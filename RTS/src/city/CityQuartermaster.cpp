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
}

bool CityQuartermaster::checkStockpileOverlap(const ui32AABB& aabb) const{
    for (auto& stockpile : mAllStockpiles) {
        if (testAABBAABB_SIMD(stockpile->getAABB(), aabb)) {
            return true;
        }
    }
    return false;
}
