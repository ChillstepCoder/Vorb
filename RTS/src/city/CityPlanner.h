#pragma once

#include "Building.h"
#include "BuildingBlueprintGenerator.h"

class City;
class CityBuilder;
struct CityPlot;
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

    std::unique_ptr<BuildingBlueprint> recieveNextBlueprint();

private:
    
    void generatePlan(CityPlot& plot);
    void finishBlueprint(std::unique_ptr<BuildingBlueprint>&& bp);

    // CityBuilder will grab these as needed
    std::deque<std::unique_ptr<BuildingBlueprint>> mFinishedBluePrints;
    std::vector<std::unique_ptr<BuildingBlueprint>> mGeneratingBlueprints;

    std::unique_ptr<BuildingBlueprintGenerator> mBuildingGenerator;

    City& mCity;
    // Temporary af
    bool mHasFreePlots = true;
};

