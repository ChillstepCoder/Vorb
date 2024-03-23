#pragma once

#include "city/CityConst.h"
#include "city/BuildingGrammar.h"

// TODO: Move to data?
enum class BuildingFunction : ui16 {
    NONE,
    RESIDENCE,
    LUMBERMILL,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(BuildingFunction,
    pair{ BuildingFunction::NONE, "none"sv },
    pair{ BuildingFunction::RESIDENCE, "stockpile"sv },
    pair{ BuildingFunction::LUMBERMILL, "shop"sv },
);

enum class RoomType : ui16 {
    NONE,
    STOCKPILE,
    SHOP,
    WORKSHOP,
    LAVATORY,
    KITCHEN,
    DINING_ROOM,
    BEDROOM,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(RoomType,
    pair{ RoomType::NONE, "none"sv},
    pair{ RoomType::STOCKPILE, "stockpile"sv},
    pair{ RoomType::SHOP, "shop"sv},
    pair{ RoomType::WORKSHOP, "workshop"sv},
    pair{ RoomType::LAVATORY, "lavatory"sv},
    pair{ RoomType::KITCHEN, "kitchen"sv},
    pair{ RoomType::DINING_ROOM, "dining_room"sv },
    pair{ RoomType::BEDROOM, "bedroom"sv }
);

class RoomDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(RoomDef, AssetType::Room);

    RoomType roomType = RoomType::NONE;
    ui8v2 widthRange = ui8v2(2, 12);
    f32 stairsChance = 0.0f;
    bool canStairsConnect = true;
};
SERIALIZABLE_IMGUI_CONTROLLED(RoomDef,
    make_field(o.roomType, "type"sv),
    make_field(o.widthRange, "width_r"sv),
    make_field(o.stairsChance, "stairs_chance"sv),
    make_field(o.canStairsConnect, "stairs_connect"sv)
);

struct PossibleRoomFileData {
    StrToken name;
    ui32v2 countRange = ui32v2(1, 100);
    f32 weight = 1.0f;
};
SERIALIZABLE_IMGUI_CONTROLLED(PossibleRoomFileData,
    make_field(o.name, "name"sv),
    make_field(o.countRange, "count_range"sv),
    make_field(o.weight, "weight"sv)
);

// Bathrooms, closets, ect
struct PossibleSubRoomFileData {
    StrToken name;
    ui32v2 countRange = ui32v2(0, 1);
    std::vector<StrToken> parentRooms;
};
SERIALIZABLE_IMGUI_CONTROLLED(PossibleSubRoomFileData,
    make_field(o.name, "name"sv),
    make_field(o.countRange, "count_range"sv),
    make_field(o.parentRooms, "parent_rooms"sv)
);

struct PossibleSubRoom {
    AssetID id;
    ui32v2 countRange = ui32v2(0, 1);
    std::vector<AssetID> parentRoomIDs; // TODO: Static array instead of vector?
};
struct PossibleRoom {
    AssetID id;
    ui32v2 countRange;
    f32 weight;
};
class BuildingDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BuildingDef, AssetType::Building);

    ui32v2 widthRange = f32v2(10, 30);
    ui32v2 publicRoomCountRange = ui32v2(1, 3);
    ui32v2 privateRoomCountRange = ui32v2(1, 3);
    ui32v2 employeeCountRange = ui32v2(0);
    f32 minAspectRatio = 0.5f; // TODO: Remove or use?
    BuildingGrammar publicGrammar; // Not serialized
    BuildingFunction function = BuildingFunction::NONE;
    std::vector<PossibleRoom> publicRooms; // Not serialized
    std::vector<PossibleRoom> privateRooms; // Not serialized
    std::vector<PossibleSubRoom> subRooms; // Not serialized
    std::vector<PossibleRoomFileData> publicRoomFileData;
    std::vector<PossibleRoomFileData> privateRoomFileData;
    std::vector<PossibleSubRoomFileData> subRoomFileData;
    std::vector<nString> publicRoomGrammarStrings;
};
SERIALIZABLE_IMGUI_CONTROLLED(BuildingDef,
    make_field(o.widthRange, "width_range"sv),
    make_field(o.publicRoomCountRange, "public_room_count_range"sv),
    make_field(o.privateRoomCountRange, "private_room_count_range"sv),
    make_field(o.employeeCountRange, "employs"sv),
    make_field(o.minAspectRatio, "minAspectRatio"sv),
    make_field(o.function, "function"sv),
    make_field(o.publicRoomFileData, "public_rooms"sv),
    make_field(o.privateRoomFileData, "private_rooms"sv),
    make_field(o.subRoomFileData, "sub_rooms"sv),
    make_field(o.publicRoomGrammarStrings, "public_room_grammars"sv)
);
