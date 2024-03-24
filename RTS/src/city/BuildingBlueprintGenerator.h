#pragma once

// Generates floor plans for buildings using a context free grammar.
// Algorithm inspired by - Jess Martin. Procedural house generation: A method for dynamically generating floor plans. In Symposium on Interactive 3D Graphics and Games. Citeseer, 2006
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.97.4544&rep=rep1&type=pdf

#include "world/settlement/building/BuildingBlueprint.h"
#include "BuildingBlueprintGenerationContext.h"

class VisualLog;
class World;
class CityBuilder;
struct RoomNode;

constexpr ui32 MAX_EXTERIOR_WALL_RUN_LENGTH = 8; // TODO: Enforce this

class BuildingBlueprintGenerator
{
public:
    BuildingBlueprintGenerator() = delete;
  
    //static void tryGenerateBlueprintASync(World& world, BuildingDescriptionRepository& buildingRepo, const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, i32v2 plotSize, const i32v3& worldPosRoot, entt::entity ownerEntity, BuildingBlueprintFlags flags, ui32 seed);
    static BuildingBlueprintPtr tryGenerateBlueprintSynchronous(
        const BuildingDef& desc,
        float sizeAlpha,
        Cartesian entrySide,
        i32v2 plotSize,
        const i32v3& worldPosRoot,
        entt::entity ownerEntity,
        BuildingBlueprintFlags flags,
        ui32 seed
    );

    static void generatePossibleWindowPermutations();
private:

    static bool tryGenerateBlueprintInternal(BuildingBlueprintGenerationContext& context);
    // Graph Generation
    static void addPublicRoomsToGraph(BuildingBlueprintGenerationContext& context);
    static void assignPublicRooms(BuildingBlueprintGenerationContext& context);
    static void addPrivateRoomsToGraph(BuildingBlueprintGenerationContext& context);
    static void initRooms(BuildingBlueprintGenerationContext& context);
    static void placeRooms(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void allocateTileData(BuildingBlueprintGenerationContext& context);
    static void expandRooms(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void roomCleanup(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void computeRoomAABBs(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static bool validateRoomsArentEmpty(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void initRoomWalls(BuildingBlueprintGenerationContext& context, RoomNode& room);
    static void placeWalls(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void placeDoors(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void buildRoomInteriorEdges(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void placeStairs(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void buildExteriorWallRuns(BuildingBlueprintGenerationContext& context, VisualLog* visLog);
    static void placeWindows(BuildingBlueprintGenerationContext& context, VisualLog* visLog);

    static void postProcessBlueprint(BuildingBlueprintGenerationContext& context);
    static BuildingBlueprintPtr finalizeBlueprint(BuildingBlueprintGenerationContext& context);

    inline static std::vector<std::vector<bool>> sPossibleWindowPermutations[MAX_EXTERIOR_WALL_RUN_LENGTH + 1];
};

