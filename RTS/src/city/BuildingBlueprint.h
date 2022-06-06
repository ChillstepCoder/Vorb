#pragma once

#include "Building.h"

enum class BlueprintTileType : ui8 {
    NONE    = 0, // THIS SHOULD ALWAYS BE 0
    FLOOR = 1, // THIS SHOULD ALWAYS BE 1
    DOOR    = 2,
    WALL    = 3,
    STAIRS  = 4,
    TYPES   = 4
};
static_assert(int(BlueprintTileType::TYPES) < 1 << 6);

struct BlueprintTile {
    BlueprintTileType type : 6;
    bool isBuilt : 1;
    bool isReserved : 1;
};
static_assert(sizeof(BlueprintTile) == 1, "Keep it small");

// TODO: Cellular automata rule iteration for room fixup
typedef ui32 BuildingBlueprintId;
#define INVALID_BLUEPRINT_ID UINT32_MAX

enum BuildingBlueprintFlags : ui8 {
    BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE = 1 << 0
};

struct BuildingBlueprint {
    BuildingBlueprint(const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui32v2 dims, ui32v2 bottomLeftWorldPos, entt::entity ownerEntity, BuildingBlueprintFlags flags);

    ui32v2 getWorldPositionOfTile(ui32 tileIndex) const {
        return aabb.pos + ui32v2(tileIndex % aabb.dims.x, tileIndex / aabb.dims.x);
    }

    const BuildingDef& desc;
    float sizeAlpha;
    Cartesian entrySide = Cartesian::WEST;
    ui32AABB2 aabb;
    ui32 floorCount = 1u;
    CityPlotIndex plotIndex = INVALID_PLOT_INDEX;

    std::vector<RoomNode> rooms;
    std::vector<RoomNodeID> ownerArray;
    std::vector<BlueprintTile> tiles;
    std::vector<ItemStackUnbounded> requiredItemsToBuild;
    std::vector<RoomGateInfo> exteriorDoors;
    const std::vector<ItemStack>* tileRecipes[e_cast(BlueprintTileType::TYPES)];
    TileID tileIDs[e_cast(BlueprintTileType::TYPES)];

    BuildingBlueprintId id = INVALID_BLUEPRINT_ID;
    ui32 tilesBuilt = 0;
    ui32 totalTilesToBuild = 0;
    entt::entity mOwnerEntity = INVALID_ENTITY;
    bool isGenerating = true;
    bool isBuilding = false;
    BuildingBlueprintFlags flags = {};
    f32 zPos = 0.0f;
    f32 floorHeight = 3.0f;
    // TODO: This is for debug only
};