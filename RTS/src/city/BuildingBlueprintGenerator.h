#pragma once

// Generates floor plans for buildings using a context free grammar.
// Algorithm inspired by - Jess Martin. Procedural house generation: A method for dynamically generating floor plans. In Symposium on Interactive 3D Graphics and Games. Citeseer, 2006
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.97.4544&rep=rep1&type=pdf

#include "BuildingBlueprint.h"

class CityBuilder;
class BuildingDescriptionRepository;

class BuildingBlueprintGenerator
{
public:
    BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo, CityBuilder& cityBuilder);
    std::unique_ptr<BuildingBlueprint> generateBlueprintAsyncThenSendToBuilder(const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui16v2 plotSize, const ui32v2& bottomLeftPos, entt::entity ownerEntity, BuildingBlueprintFlags flags);

    static std::unique_ptr<BuildingBlueprint> generateBlueprintSync(BuildingDescriptionRepository& buildingRepo, const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui16v2 plotSize, const ui32v2& bottomLeftPos, entt::entity ownerEntity, BuildingBlueprintFlags flags);

private:
    static void generateBlueprintInternal(BuildingBlueprint* bPtr, BuildingDescriptionRepository& buildingRepo);
    // Graph Generation
    static void addPublicRoomsToGraph(BuildingBlueprint& bp);
    static void assignPublicRooms(BuildingBlueprint& bp);
    static void addPrivateRoomsToGraph(BuildingBlueprint& bp);
    static void addStickOnRoomsToGraph();
    static void initRooms(BuildingBlueprint& bp, BuildingDescriptionRepository& buildingRepo);
    static void placeRooms(BuildingBlueprint& bp);
    static void expandRooms(BuildingBlueprint& bp);
    static void roomCleanup(BuildingBlueprint& bp);
    static void initRoomWalls(BuildingBlueprint& bp, RoomNode& room);
    static void placeFacadeWalls(BuildingBlueprint& bp);
    static void placeInteriorWalls(BuildingBlueprint& bp);
    static void placeDoors(BuildingBlueprint& bp);

    static void postProcessBlueprint(BuildingBlueprint& bp);

    static ui32 getNextBuildingID();

    BuildingDescriptionRepository& mBuildingRepo;
    CityBuilder& mCityBuilder;
    std::set<BuildingBlueprint*> mGeneratingBuildings;
    static BuildingBlueprintId sCurrentId;
};

