#pragma once

// TODO: instead have the ItemRenderer manage this mapping

#include "item/ItemStack.h"
#include "tile/TileHarvestable.h"

enum class ItemType {
    UNKNOWN,
    MATERIAL,
    WEAPON,
    ARMOR,
    TRINKET,
    FOOD,
    BEVERAGE,
    POTION,
    QUEST,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ItemType,
    pair{ ItemType::UNKNOWN, "unknown"sv},
    pair{ ItemType::MATERIAL, "material"sv },
    pair{ ItemType::WEAPON, "weapon"sv },
    pair{ ItemType::ARMOR, "armor"sv },
    pair{ ItemType::TRINKET, "trinket"sv },
    pair{ ItemType::FOOD, "food"sv },
    pair{ ItemType::BEVERAGE, "beverage"sv },
    pair{ ItemType::POTION, "potion"sv },
    pair{ ItemType::QUEST, "quest"sv }
);
static_assert(e_count(ItemType) == 9, "Update def");

enum class ItemStorageShape {
    POINT,
    QUAD_SHAPES_START,
    PLANK = QUAD_SHAPES_START,
    LOG,
    INGOT,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ItemStorageShape,
    pair{ ItemStorageShape::POINT, "point"sv },
    pair{ ItemStorageShape::PLANK, "plank"sv },
    pair{ ItemStorageShape::LOG, "log"sv },
    pair{ ItemStorageShape::INGOT, "ingot"sv }
);
static_assert(e_count(ItemStorageShape) == 4, "Update def");

class ItemDef : public IAsset {
    friend class ItemRepository;
    friend class ItemRenderer;
    friend class ItemStockpile;
public:
    DEFAULT_ASSET_CONSTRUCTOR(ItemDef);

    // TODO: Remove accessors
    f32 getValue() const { return mValue; }
    f32 getWeight() const { return mWeight; }
    ui32 getMaxStackSize() const { return mStackSize; }
    TileHarvestable getSourceHarvestable() const { return mHarvestableSource; }

    StrToken mTextureName;
    ItemType mType = ItemType::UNKNOWN;
    ItemStorageShape mShape = ItemStorageShape::POINT;
    TileHarvestable mHarvestableSource = TileHarvestable::NONE;
    // TODO: Model or something?
    f32 mValue = 1.0f;
    f32 mWeight = 0.01f;
    ui32 mStackSize = 10;
    ui32v3 mStackDims = ui32v3(5, 5, 5);
};
SERIALIZABLE_IMGUI_CONTROLLED(ItemDef,
    make_field(o.mTextureName, "texture"sv),
    make_field(o.mType, "type"sv),
    make_field(o.mShape, "shape"sv),
    make_field(o.mHarvestableSource, "harvest"sv),
    make_field(o.mValue, "value"sv),
    make_field(o.mWeight, "weight"sv),
    make_field(o.mStackSize, "stack_size"sv),
    make_field(o.mStackDims, "stack_dims"sv)
);

struct StoredItemStack {
    ItemStack stack;
    i32v2 worldPos = {};
    bool isInContainer = false;
};