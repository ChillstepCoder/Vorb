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

    void update();
    void addRoadToBuild(RoadID roadId) { mRoadsToBuild.emplace_back(roadId); }
    BuildingBlueprint* aquireBlueprintToBuild(entt::entity businessId);

    void onBlueprintComplete(BuildingBlueprint* bp);

private:

    void debugBuildInstant(BuildingBlueprint& bp);
    void debugBuildInstant(RoadID roadId);

    City& mCity;
    World& mWorld;
    std::list<std::unique_ptr<BuildingBlueprint>> mWaitingBlueprints;
    std::vector<std::pair<std::unique_ptr<BuildingBlueprint>, entt::entity /*business owner*/>> mInProgressBlueprints;
    std::vector<RoadID> mRoadsToBuild;


};

