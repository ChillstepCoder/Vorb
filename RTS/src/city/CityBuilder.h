#pragma once

class City;
class CityPlanner;
struct BuildingBlueprint;

#include "Building.h"

// ***********************************************************************************************************
// The city builder recieves blueprints, and contracts them out to builder businesses to be constructed

class CityBuilder
{
    friend class CityDebugRenderer;
public:
    CityBuilder(City& city);

    void update();
    void addRoadToBuild(RoadID roadId) { mRoadsToBuild.emplace_back(roadId); }
    void addBlueprintToBuildAndPreprocess(BuildingBlueprint* blueprint);

    static Building* debugBuildInstant(BuildingBlueprint& bp);
    void debugBuildInstant(RoadID roadId);

private:
    void preprocessBlueprint(BuildingBlueprint* blueprint);
    static void finishBuilding(Building& building, BuildingBlueprint& blueprint);
    bool trySendBuildingJob(BuildingBlueprint* blueprint);

    City& mCity;
    std::list<BuildingBlueprint*> mBlueprintsToBuild;
    std::vector<RoadID> mRoadsToBuild;

};

