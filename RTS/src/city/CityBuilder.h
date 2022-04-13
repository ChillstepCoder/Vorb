#pragma once

class City;
class CityPlanner;
class World;
struct BuildingBlueprint;

#include "Building.h"

// ***********************************************************************************************************
// The city builder recieves blueprints, and contracts them out to builder businesses to be constructed

class CityBuilder
{
    friend class CityDebugRenderer;
public:
    CityBuilder(City& city, World& world);

    void update();
    void addRoadToBuild(RoadID roadId) { mRoadsToBuild.emplace_back(roadId); }
    void addBlueprintToBuildAndPreprocess(BuildingBlueprint* blueprint);

    static Building debugBuildInstant(BuildingBlueprint& bp, World& world);
    void debugBuildInstant(RoadID roadId);

private:
    void preprocessBlueprint(BuildingBlueprint* blueprint);
    bool trySendBuildingJob(BuildingBlueprint* blueprint);

    City& mCity;
    World& mWorld;
    std::list<BuildingBlueprint*> mBlueprintsToBuild;
    std::vector<RoadID> mRoadsToBuild;

};

