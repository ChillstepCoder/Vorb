#pragma once

#include "city/Building.h"

class City;

class CityFunctionManager
{
public:
    CityFunctionManager(City& city);

    void registerNewBuilding(Building& building);
    void update();

private:
    City& mCity;

    std::vector<BuildingID> mLumbermills;

};

