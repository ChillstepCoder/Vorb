#include "stdafx.h"
#include "CityFunctionManager.h"

CityFunctionManager::CityFunctionManager(City& city) :
    mCity(city)
{

}

void CityFunctionManager::registerNewBuilding(Building& building)
{
    // Assign to lists based on function
    switch (building.mFunction) {
        case BuildingFunction::NONE:
            break;
        case BuildingFunction::RESIDENCE:
            break;
        case BuildingFunction::LUMBERMILL:
            mLumbermills.emplace_back(building.mId);
            break;
        default:
            assert(false); // Invalid type
            break;

    };
    static_assert(enum_cast(BuildingFunction::TYPES) == 3, "Update switch");
}

void CityFunctionManager::update()
{

}
