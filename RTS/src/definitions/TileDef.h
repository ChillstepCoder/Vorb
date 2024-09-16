#pragma once

#include "tile/TileHarvestable.h"
#include "tile/MutationDef.h"

// TODO: Do we need rendering here? (MaterialDesc)
#include "rendering/material/MaterialDef.h"

#include "item/ItemRollTable.h"

enum class TileLayer : ui8 {
    Ground = 0,
    Main = 1,
    COUNT = 2
};

enum class TileShape : ui8 {
    THIN,  // Trees and flora
    BLOCK, // Most blocks
    FLOOR,
    WALL,
    WINDOW,
    DOOR,
    STAIRS,
    MODEL,
    // Custom TODO
    COUNT,
    NONE = COUNT
};
SERIALIZABLE_ENUM_DECL(TileShape);

struct ItemInputDef {
    StrToken itemName;
    ui32 count;
};
SERIALIZABLE_DECL(ItemInputDef);

struct ItemDrop {
    StrToken itemName;
    ui32v2 countRange;
    ItemID id;
};
SERIALIZABLE_DECL(ItemDrop);

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL,
    VERTICAL,
    FLORA,
    WORLD_TILING,
    COUNT
};
SERIALIZABLE_ENUM_DECL(TileTextureMethod);

enum class NavBlockerType : ui8 {
    NONE,
    MEDIUM,
    LARGE,
    COUNT
};
enum class VisibilityBlockerType : ui8 {
    NONE,
    MEDIUM,
    LARGE,
    COUNT
};

constexpr int MAX_TILE_MATERIAL_SLOTS = 8;

// TODO: separate certain data into multiple arrays because right now every TileData lookup is a cache miss
// For example we only look up path weight when constructing the nav  graph, why not  have it in a separate vector?
// Same with materialData. Does it really need to be here?
// This def is accessed quite commonly
class TileDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(TileDef, AssetType::Tile);

    // TileCollider collider;
     //ui8v2 tileDims = ui8v2(1); // 4x4 is max size
    TileHarvestable harvestable = TileHarvestable::None;
    // TODO: Could be a giant array of material slots and these defs only store pointers and lengths.
    std::vector<StrToken> materialNames;
    std::vector<MaterialDesc> materialData;
    TileTextureMethod textureMethod;
    EffectAssetRef destroyEffectRef;
    ModelAssetRef modelRef;
    ModelID modelId = INVALID_MODEL_ID;
    std::vector<ui8> modelVariants;
    ui16 maxHealth = 100;
    ui8 layer = e_cast(TileLayer::Main);
    TileShape shape = TileShape::BLOCK;
    ui8 pathWeight = 255;
    ui8 navMask = 0xff; // Access bits mapped to Cartesian8 based on default (SOUTH) orientation
    VisibilityBlockerType visBlockerType = VisibilityBlockerType::NONE;
    TileType tileType = TileType::Default;
    std::array<TileID, e_count(MutationType)> transformations;
    std::vector<MutationDef> transformationDefs;
    union {
        struct {
            f32 heightOffsetSouth;
            f32 heightOffsetEast;
            f32 heightOffsetWest;
            f32 heightOffsetNorth;
        };
        f32 heightOffsets[4];
    };
    std::string displayName;
    std::vector<ItemInputDef> recipeData;
    ItemRollTable itemDrops;
};
SERIALIZABLE_IMGUI_CONTROLLED_DECL(TileDef);