#pragma once

class City;
class CityPlanner;
class BuildingBlueprintGenerationContext;
class Building;
class World;

#include "city/CityConst.h"

// ***********************************************************************************************************
// The city builder recieves blueprints, and contracts them out to builder businesses to be constructed

class CityBuilder
{
    friend class CityDebugRenderer;
public:
    CityBuilder(City& city);

    void update();
    void addRoadToBuild(RoadID roadId) { mRoadsToBuild.emplace_back(roadId); }
    void addBlueprintToBuildAndPreprocess(BuildingBlueprintGenerationContext* blueprint);

    static Building* debugBuildInstant(World& world, BuildingBlueprintGenerationContext& bp);
    void debugBuildRoadInstant(RoadID roadId);

private:
    void preprocessBlueprint(BuildingBlueprintGenerationContext& bp);
    static void finishBuilding(World& world, Building& building, BuildingBlueprintGenerationContext& blueprint);
    bool trySendBuildingJob(BuildingBlueprintGenerationContext* blueprint);

    City& mCity;
    std::list<BuildingBlueprintGenerationContext*> mBlueprintsToBuild;
    std::vector<RoadID> mRoadsToBuild;

};

