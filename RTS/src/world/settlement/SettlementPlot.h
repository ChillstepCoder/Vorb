#pragma once

#include "world/settlement/SettlementZone.h"
#include "util/BitArray.h"

#include "building/BuildingBlueprint.h"

enum class PlotFlags : ui8 {
    Owned,
    Reserved,
    HasBlueprint,
    HasFinishedStructure
};

struct SettlementPlot {
    BitArray ownedDTiles;
    i32AABB2 aabbDTile;
    i32 dTileCount;
    RoadSegmentID connectedRoad;
    BitFlags<PlotFlags> flags;
    SettlementZone zone;
    entt::entity owner = entt::null;
    BuildingID structure = INVALID_BUILDING_ID;
    BuildingBlueprintPtr activeBlueprint = nullptr;
    //ui8 padding[2];
};