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

    std::unique_ptr<BuildingBlueprint> generateBlueprintSync(const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui16v2 plotSize, const ui32v2& bottomLeftPos, entt::entity ownerEntity, BuildingBlueprintFlags flags);

private:
    void generateBlueprintInternal(BuildingBlueprint* bPtr);
    // Graph Generation
    void addPublicRoomsToGraph(BuildingBlueprint& bp) const;
    void assignPublicRooms(BuildingBlueprint& bp) const;
    void addPrivateRoomsToGraph(BuildingBlueprint& bp) const;
    void addStickOnRoomsToGraph() const;
    void initRooms(BuildingBlueprint& bp) const;
    void placeRooms(BuildingBlueprint& bp) const;
    void expandRooms(BuildingBlueprint& bp) const;
    void roomCleanup(BuildingBlueprint& bp) const;
    void initRoomWalls(BuildingBlueprint& bp, RoomNode& room) const;
    void placeFacadeWalls(BuildingBlueprint& bp) const;
    void placeInteriorWalls(BuildingBlueprint& bp) const;
    void placeDoors(BuildingBlueprint& bp) const;

    void postProcessBlueprint(BuildingBlueprint& bp) const;

    static ui32 getNextBuildingID();

    BuildingDescriptionRepository& mBuildingRepo;
    CityBuilder& mCityBuilder;
    std::set<BuildingBlueprint*> mGeneratingBuildings;
    static BuildingBlueprintId sCurrentId;
};

