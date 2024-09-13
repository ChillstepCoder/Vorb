#include "stdafx.h"
#include "TileDef.h"

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
static_assert(e_count(TileShape) == 8);

SERIALIZABLE_SIMPLE(ItemInputDef,
    make_field(o.itemName, "item"sv),
    make_field(o.count, "count"sv)
);

SERIALIZABLE_SIMPLE(ItemDrop,
    make_field(o.itemName, "item"sv),
    make_field(o.countRange, "count"sv)
);

SERIALIZABLE_ENUM_SAME_NAME(TileTextureMethod,
    pair{ TileTextureMethod::SIMPLE, "simple"sv },
    pair{ TileTextureMethod::CONNECTED, "connected"sv },
    pair{ TileTextureMethod::CONNECTED_WALL, "connected_wall"sv },
    pair{ TileTextureMethod::VERTICAL, "vertical"sv },
    pair{ TileTextureMethod::FLORA, "flora"sv },
    pair{ TileTextureMethod::WORLD_TILING, "world_tiling"sv }
);
static_assert(e_count(TileTextureMethod) == 6);

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
