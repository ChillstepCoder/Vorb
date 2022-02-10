#pragma once

#include "Building.h"

enum class BlueprintTileType : ui8 {
    NONE    = 0, // THIS SHOULD ALWAYS BE 0
    FLOOR_1 = 1, // THIS SHOULD ALWAYS BE 1
    DOOR    = 2,
    WALL    = 3,
    TYPES   = 4
};
static_assert(int(BlueprintTileType::TYPES) < UINT8_MAX);

struct BlueprintTile {
    BlueprintTileType type : 7;
    bool isBuilt : 1;
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
    CityPlotIndex plotIndex = INVALID_PLOT_INDEX;

    std::vector<RoomNode> nodes;
    std::vector<RoomNodeID> ownerArray;
    std::vector<BlueprintTile> tiles;
    std::vector<ItemStack> requiredItemsToBuild;

    TileID tileIDs[enum_cast(BlueprintTileType::TYPES)];

    BuildingBlueprintId id = INVALID_BLUEPRINT_ID;
    ui32 tilesBuilt = 0;
    ui32 totalTilesToBuild = 0;
    bool isGenerating = true;
    bool isBuilding = false;
    // TODO: This is for debug only
};