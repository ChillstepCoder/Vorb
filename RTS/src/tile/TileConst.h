#pragma once

constexpr ui16 TILE_ID_NONE = UINT16_MAX;
constexpr int TILE_LAYER_GROUND = 0;
constexpr int TILE_LAYER_MID = 1;
constexpr int TILE_LAYER_TOP = 2;
constexpr int TILE_LAYER_COUNT = 3;

constexpr i32 MIN_WORLD_HEIGHT = -300;
constexpr i32 MAX_WORLD_HEIGHT = 1000;
constexpr ui32 WORLD_HEIGHT_SPAN = (ui32)(MAX_WORLD_HEIGHT - MIN_WORLD_HEIGHT);
constexpr ui32 SCALED_Z_UNITS_PER_TILE = UINT16_MAX / WORLD_HEIGHT_SPAN;
constexpr f32 UNCOMPRESS_Z_UNITS_PER_TILE_MULT = 1.0f / SCALED_Z_UNITS_PER_TILE;

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