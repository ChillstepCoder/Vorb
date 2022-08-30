#pragma once

constexpr ui16 TILE_ID_NONE = UINT16_MAX;
constexpr int TILE_LAYER_GROUND = 0;
constexpr int TILE_LAYER_MID = 1;
constexpr int TILE_LAYER_TOP = 2;
constexpr int TILE_LAYER_COUNT = 3;

enum class TileLayer {
    Ground = 0,
    Mid = 1,
    Top = 2,
    COUNT = 3
};
static_assert(TILE_LAYER_COUNT == e_cast(TileLayer::COUNT));

enum class TileShape {
    THIN,  // Trees and flora
    BLOCK, // Most blocks
    FLOOR,
    WALL,
    DOOR,
    STAIRS,
    // Custom TODO
    COUNT
};
KEG_ENUM_DECL(TileShape);

enum class TileResource : ui8 {
    NONE,
    WOOD,
    STONE,
    COUNT
};
KEG_ENUM_DECL(TileResource);

struct ItemInputDef {
    nString itemName;
    ui32 count;
};
KEG_TYPE_DECL(ItemInputDef);

struct ItemDrop {
    ItemID id;
    ui32v2 countRange;
};

struct ItemDropDef {
    nString itemName;
    ui32v2 countRange;
};
KEG_TYPE_DECL(ItemDropDef);