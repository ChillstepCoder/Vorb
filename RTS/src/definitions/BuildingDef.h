#pragma once

#include "city/CityConst.h"
#include "city/BuildingGrammar.h"

// TODO: Move to data?
enum class BuildingFunction : ui16 {
    NONE,
    RESIDENCE,
    LUMBERMILL,
    TYPES
};
KEG_ENUM_DECL(BuildingFunction);

enum class RoomType : ui16 {
    NONE,
    STOCKPILE,
    SHOP,
    WORKSHOP,
    LAVATORY,
    KITCHEN,
    DINING_ROOM,
    BEDROOM,
    TYPES
};
KEG_ENUM_DECL(RoomType);

struct RoomDef {
    RoomDefID id;
    RoomType roomType = RoomType::NONE;
    ui8 minWidth = 2;
    ui8 maxWidth = 12;
    f32 stairsChance = 0.0f;
    bool canStairsConnect = true;
};
KEG_TYPE_DECL(RoomDef);

struct PossibleSubRoom {
    RoomDefID id;
    ui32v2 countRange = ui32v2(0, 1);
    std::vector<RoomDefID> parentRoomIDs;
};
struct PossibleRoom {
    RoomDefID id;
    ui32v2 countRange;
    f32 weight;
};
struct BuildingDef {
    BuildingTypeID id = UINT16_MAX;
    ui32v2 widthRange = f32v2(10, 30);
    ui32v2 publicRoomCountRange = ui32v2(1, 3);
    ui32v2 privateRoomCountRange = ui32v2(1, 3);
    ui32v2 employeeCountRange = ui32v2(0);
    f32 minAspectRatio = 0.5f;
    BuildingGrammar publicGrammar;
    BuildingFunction function = BuildingFunction::NONE;
    // TODO: More cache friendly combination?
    std::vector<PossibleRoom> publicRooms;
    std::vector<PossibleRoom> privateRooms;
    std::vector<PossibleSubRoom> subRooms;
    nString name;
};
