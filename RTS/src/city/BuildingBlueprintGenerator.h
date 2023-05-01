#pragma once

// Generates floor plans for buildings using a context free grammar.
// Algorithm inspired by - Jess Martin. Procedural house generation: A method for dynamically generating floor plans. In Symposium on Interactive 3D Graphics and Games. Citeseer, 2006
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.97.4544&rep=rep1&type=pdf

#include "BuildingBlueprint.h"

class VisualLog;
class IWorld;
class CityBuilder;
class BuildingDescriptionRepository;
struct RoomNode;

constexpr ui32 MAX_EXTERIOR_WALL_RUN_LENGTH = 8; // TODO: Enforce this

class BuildingBlueprintGenerator
{
public:
    BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo, CityBuilder& cityBuilder);
    std::unique_ptr<BuildingBlueprint> generateBlueprintAsyncThenSendToBuilder(IWorld& world, const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, i32v2 plotSize, const i32v3& worldPosRoot, entt::entity ownerEntity, BuildingBlueprintFlags flags);

    VORB_NON_COPYABLE(BuildingBlueprintGenerator);

    static std::unique_ptr<BuildingBlueprint> tryGenerateBlueprintSynchronous(IWorld& world, BuildingDescriptionRepository& buildingRepo, const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, i32v2 plotSize, const i32v3& worldPosRoot, entt::entity ownerEntity, BuildingBlueprintFlags flags);

    static void generatePossibleWindowPermutations();
private:

    static bool tryGenerateBlueprintInternal(BuildingBlueprint* bPtr, BuildingDescriptionRepository& buildingRepo);
    // Graph Generation
    static void addPublicRoomsToGraph(BuildingBlueprint& bp);
    static void assignPublicRooms(BuildingBlueprint& bp);
    static void addPrivateRoomsToGraph(BuildingBlueprint& bp);
    static void initRooms(BuildingBlueprint& bp, BuildingDescriptionRepository& buildingRepo);
    static void placeRooms(BuildingBlueprint& bp, VisualLog* visLog);
    static void allocateTileData(BuildingBlueprint& bp);
    static void expandRooms(BuildingBlueprint& bp, VisualLog* visLog);
    static void roomCleanup(BuildingBlueprint& bp, VisualLog* visLog);
    static void computeRoomAABBs(BuildingBlueprint& bp, VisualLog* visLog);
    static bool validateRoomsArentEmpty(BuildingBlueprint& bp, VisualLog* visLog);
    static void initRoomWalls(BuildingBlueprint& bp, RoomNode& room);
    static void placeWalls(BuildingBlueprint& bp, VisualLog* visLog);
    static void placeDoors(BuildingBlueprint& bp, VisualLog* visLog);
    static void buildRoomInteriorEdges(BuildingBlueprint& bp, VisualLog* visLog);
    static void placeStairs(BuildingBlueprint& bp, VisualLog* visLog);
    static void buildExteriorWallRuns(BuildingBlueprint& bp, VisualLog* visLog);
    static void placeWindows(BuildingBlueprint& bp, VisualLog* visLog);

    static void postProcessBlueprint(BuildingBlueprint& bp);

    static BuildingBlueprintId getNextBuildingID();

    BuildingDescriptionRepository& mBuildingRepo;
    CityBuilder& mCityBuilder;
    std::set<BuildingBlueprint*> mGeneratingBuildings;
    static BuildingBlueprintId sCurrentId;

    inline static std::vector<std::vector<bool>> sPossibleWindowPermutations[MAX_EXTERIOR_WALL_RUN_LENGTH + 1];
};

