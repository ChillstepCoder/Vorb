#pragma once

// Generates floor plans for buildings using a context free grammar.
// Algorithm inspired by - Jess Martin. Procedural house generation: A method for dynamically generating floor plans. In Symposium on Interactive 3D Graphics and Games. Citeseer, 2006
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.97.4544&rep=rep1&type=pdf

#include "Building.h"

enum class BlueprintTileType : ui8 {
    NONE  = 0, // THIS SHOULD ALWAYS BE 0
    FLOOR = 1, // THIS SHOULD ALWAYS BE 1
    DOOR,
    WALL,
    TYPES
};

struct BlueprintTile {
    BlueprintTileType type;
};
static_assert(sizeof(BlueprintTile) == 1, "Keep it small"); 

// TODO: Cellular automata rule iteration for room fixup
typedef ui32 BuildingBlueprintId;
#define INVALID_BLUEPRINT_ID UINT32_MAX

struct BuildingBlueprint {
    BuildingBlueprint(const BuildingDescription& desc, float sizeAlpha, Cartesian entrySide, ui16v2 dims, ui32v2 bottomLeftWorldPos);

    const BuildingDescription& desc;
    float sizeAlpha;
    Cartesian entrySide = Cartesian::LEFT;
    ui16v2 dims;
    ui32v2 bottomLeftWorldPos;

    std::vector<RoomNode> nodes;
    std::vector<RoomNodeID> ownerArray;
    std::vector<BlueprintTile> tiles;

    BuildingBlueprintId id = INVALID_BLUEPRINT_ID;
    bool isGenerating = true;
    // TODO: This is for debug only
};

class BuildingBlueprintGenerator
{
public:
    BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo);
    std::unique_ptr<BuildingBlueprint> generateBuildingAsync(const BuildingDescription& desc, float sizeAlpha, Cartesian entrySide, ui16v2 plotSize, const ui32v2& bottomLeftPos);

private:
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

    BuildingDescriptionRepository& mBuildingRepo;
    std::set<BuildingBlueprint*> mGeneratingBuildings;
    BuildingBlueprintId mCurrentId = 0;
};

