#pragma once

#include "Building.h"
#include "BuildingBlueprintGenerator.h"

class City;
class CityBuilder;
// ***********************************************************************************************************
// The city planner handles determining where current and future buildings should be placed.
// and what types of buildings should be built.
// The CityBuilder will query plans from the CityPlanner as needed.
// The CityPlanner can also send urgent plans to the CityBuilder to override existing plans

// It will need to be dynamic, for example a town in a peaceful area will not allocate much into military structures,
// while if the threat in an area is large, it may decide to add walls sooner

class CityPlanner
{
    friend class CityDebugRenderer;
public:
    CityPlanner(City& city);

    void update();

    // Returns true if successfully storing next plan in outPlan
    bool recieveNextPlan(OUT PlannedBuilding& outPlan);
    std::unique_ptr<BuildingBlueprint> recieveNextBlueprint();

private:
    
    void generatePlan();

    // CityBuilder will grab these as needed
    std::deque<PlannedBuilding> mStandardPlans;
    std::deque<std::unique_ptr<BuildingBlueprint>> mBluePrints;

    BuildingBlueprintGenerator mBuildingGenerator;

    City& mCity;
};

