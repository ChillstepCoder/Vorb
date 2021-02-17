#pragma once

// Generates floor plans for buildings using a context free grammar.
// Algorithm inspired by - Jess Martin. Procedural house generation: A method for dynamically generating floor plans. In Symposium on Interactive 3D Graphics and Games. Citeseer, 2006
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.97.4544&rep=rep1&type=pdf

#include "Building.h"

enum class BlueprintTileType : ui8 {
    NONE,
    FLOOR,
    DOOR,
    WALL,
    TYPES
};

struct BlueprintTile {
    BlueprintTileType type;
};
static_assert(sizeof(BlueprintTile) == 1, "Keep it small"); 

struct BuildingBlueprint {
    BuildingBlueprint(const BuildingDescription& desc, float sizeAlpha) : desc(desc), sizeAlpha(sizeAlpha) {}

    std::vector<RoomNode> nodes;
    std::vector<BlueprintTile> tiles;
    ui32v2 bottomLeftWorldPos;
    ui16v2 dims;
    float sizeAlpha;
    const BuildingDescription& desc;
    // TODO: This is for debug only
    std::vector<RoomNodeID> ownerArray;
};

class BuildingBlueprintGenerator
{
public:
    BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo);
    std::unique_ptr<BuildingBlueprint> generateBuilding(const BuildingDescription& desc, float sizeAlpha);

private:
    // Graph Generation
    void addPublicRoomsToGraph(BuildingBlueprint& bp);
    void assignPublicRooms(BuildingBlueprint& bp);
    void addPrivateRoomsToGraph(BuildingBlueprint& bp);
    void addStickOnRoomsToGraph();
    void initRooms(BuildingBlueprint& bp);
    void placeRooms(BuildingBlueprint& bp);
    void expandRooms(BuildingBlueprint& bp);
    void initRoomWalls(BuildingBlueprint& bp, RoomNode& room, RoomNodeID roomId, std::vector<RoomNodeID>& metaData);
    void placeDoors(BuildingBlueprint& bp);

    BuildingDescriptionRepository& mBuildingRepo;
};

