#pragma once

class City;
class CityPlanner;
class World;
struct BuildingBlueprint;

#include "Building.h"

// ***********************************************************************************************************
// The city builder manages a cities builders, and gives them orders to construct buildings. It constructs them
// in the order received by the city planner. It will attempt to build many buildings in parallel using different work groups.
// It will also grab plans from the CityPlanner when it needs more

class CityBuilder
{
    friend class CityDebugRenderer;
public:
    CityBuilder(City& city, World& world);

    void addUrgentPlans (PlannedBuilding&& plannedBuilding);

    void update();

private:

    void debugBuildInstant(BuildingBlueprint& bp);

    City& mCity;
    World& mWorld;
    std::vector<std::unique_ptr<BuildingBlueprint>> mInProgressBlueprints;


};
