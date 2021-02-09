#pragma once

// Generates floor plans for buildings using a context free grammar.
// Algorithm inspired by - Jess Martin. Procedural house generation: A method for dynamically generating floor plans. In Symposium on Interactive 3D Graphics and Games. Citeseer, 2006
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.97.4544&rep=rep1&type=pdf

#include "Building.h"

enum class BlueprintTileType : ui8 {
    NONE,
    FLOOR,
    DOOR,
    WALL
};

struct BlueprintTile {
    ui8 parentRoom;
    BlueprintTileType type;
};
static_assert(sizeof(BlueprintTile) == 2, "Keep it small"); 

struct BuildingBlueprint {
    BuildingBlueprint(const BuildingDescription& desc, float sizeAlpha) : desc(desc), sizeAlpha(sizeAlpha) {}

    std::vector<RoomNode> nodes;
    std::vector<BlueprintTile> tiles;
    ui32v2 rootWorldPos;
    ui16v2 dims;
    float sizeAlpha;
    const BuildingDescription& desc;
};

class BuildingBlueprintGenerator
{
public:
    std::unique_ptr<BuildingBlueprint> generateBuilding(const BuildingDescription& desc, float sizeAlpha);

private:
    // Graph Generation
    void addPublicRoomsToGraph(BuildingBlueprint& bp);
    void assignPublicRooms(BuildingBlueprint& bp);
    void addPrivateRoomsToGraph(BuildingBlueprint& bp);
    void addStickOnRoomsToGraph();
    // Room Placement
    void placeRooms(BuildingBlueprint& bp);
    // Room Expansion
    void expandRooms(BuildingBlueprint& bp);
};

