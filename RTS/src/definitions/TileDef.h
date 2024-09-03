#pragma once

#include "tile/TileHarvestable.h"

// TODO: Do we need rendering here? (MaterialDesc)
#include "rendering/material/MaterialData.h"

#include "item/ItemRollTable.h"

enum class TileTransformationType {
    BCorrupt,
    BPurify,
    CCorrupt,
    CPurify,
    Grow,
    Decay,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TileTransformationType,
    ENUM_FIELD_SIMPLE(TileTransformationType, BCorrupt),
    ENUM_FIELD_SIMPLE(TileTransformationType, BPurify),
    ENUM_FIELD_SIMPLE(TileTransformationType, CCorrupt),
    ENUM_FIELD_SIMPLE(TileTransformationType, CPurify),
    ENUM_FIELD_SIMPLE(TileTransformationType, Grow),
    ENUM_FIELD_SIMPLE(TileTransformationType, Decay)
);

struct TileTransformationDef {
    TileTransformationType type = TileTransformationType::BCorrupt;
    TileAssetRef target;
};;
SERIALIZABLE_IMGUI_CONTROLLED(TileTransformationDef,
    make_field(o.type, "type"sv),
    make_field(o.target, "target"sv)
);

enum class TileLayer : ui8 {
    Ground = 0,
    Main = 1,
    COUNT = 2
};
static_assert(TILE_LAYER_COUNT == e_cast(TileLayer::COUNT));

enum class TileShape {
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
SERIALIZABLE_ENUM_SAME_NAME(TileShape,
    pair{ TileShape::THIN, "thin"sv },
    pair{ TileShape::BLOCK, "block"sv },
    pair{ TileShape::FLOOR, "floor"sv },
    pair{ TileShape::WALL, "wall"sv },
    pair{ TileShape::WINDOW, "window"sv },
    pair{ TileShape::DOOR, "door"sv },
    pair{ TileShape::STAIRS, "stairs"sv },
    pair{ TileShape::MODEL, "model"sv }
);

struct ItemInputDef {
    StrToken itemName;
    ui32 count;
};
SERIALIZABLE_SIMPLE(ItemInputDef,
    make_field(o.itemName, "item"sv),
    make_field(o.count, "count"sv)
);

struct ItemDrop {
    StrToken itemName;
    ui32v2 countRange;
    ItemID id;
};
SERIALIZABLE_SIMPLE(ItemDrop,
    make_field(o.itemName, "item"sv),
    make_field(o.countRange, "count"sv)
);

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL,
    VERTICAL,
    FLORA,
    WORLD_TILING,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TileTextureMethod,
    pair{ TileTextureMethod::SIMPLE, "simple"sv },
    pair{ TileTextureMethod::CONNECTED, "connected"sv },
    pair{ TileTextureMethod::CONNECTED_WALL, "connected_wall"sv },
    pair{ TileTextureMethod::VERTICAL, "vertical"sv },
    pair{ TileTextureMethod::FLORA, "flora"sv },
    pair{ TileTextureMethod::WORLD_TILING, "world_tiling"sv }
);

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
    std::array<TileID, e_count(TileTransformationType)> transformations;
    std::vector<TileTransformationDef> transformationDefs;
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
SERIALIZABLE_IMGUI_CONTROLLED(TileDef,
    make_field(o.tileType, "type"),
    make_field(o.harvestable, "harvestable"),
    make_field(o.materialNames, "materials"),
    make_field(o.textureMethod, "texture_method"),
    make_field(o.destroyEffectRef, "destroy_effect"),
    make_field(o.modelRef, "model"),
    make_field(o.modelVariants, "model_variants"),
    make_field(o.maxHealth, "max_health"),
    make_field(o.layer, "layer"),
    make_field(o.shape, "shape"),
    make_field(o.pathWeight, "path_weight"),
    make_field(o.navMask, "nav_mask"),
    make_field(o.displayName, "name"),
    make_field(o.itemDrops, "item_drops"),
    make_field(o.recipeData, "recipe"),
    make_field(o.transformationDefs, "transforms")
);
