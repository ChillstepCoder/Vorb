#pragma once

class BuildingBlueprint;
class Building;
class World;

#include "city/CityConst.h"

// ***********************************************************************************************************
// The city builder recieves blueprints, and contracts them out to builder businesses to be constructed

// TODO: Delete
class BuildingBuilder
{
public:
    // Synchronous
    static Building* debugCreateAndBuildNewBuilding(World& world, std::unique_ptr<BuildingBlueprint>& bpPtr);
};

