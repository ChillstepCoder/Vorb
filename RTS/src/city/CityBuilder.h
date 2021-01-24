#pragma once

class City;
class CityPlanner;
class World;

#include "Building.h"

// ***********************************************************************************************************
// The city builder manages a cities builders, and gives them orders to construct buildings. It constructs them
// in the order received by the city planner. It will attempt to build many buildings in parallel using different work groups.
// It will also grab plans from the CityPlanner when it needs more

class CityBuilder
{
public:
    CityBuilder(City& city, World& world);

    void addUrgentPlans (PlannedBuilding&& plannedBuilding);

    void update();

private:

    void debugBuildInstant(PlannedBuilding& plan);

    City& mCity;
    World& mWorld;
    std::vector<PlannedBuilding> mInProgressPlans;


};

