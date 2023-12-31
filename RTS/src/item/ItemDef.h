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

enum class ItemStockpileShape {
    POINT,
    QUAD_SHAPES_START,
    PLANK = QUAD_SHAPES_START,
    LOG,
    INGOT,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ItemStockpileShape,
    pair{ ItemStockpileShape::POINT, "point"sv },
    pair{ ItemStockpileShape::PLANK, "plank"sv },
    pair{ ItemStockpileShape::LOG, "log"sv },
    pair{ ItemStockpileShape::INGOT, "ingot"sv }
);
static_assert(e_count(ItemStockpileShape) == 4, "Update def");

class ItemDef : public IAsset {
    friend class ItemRepository;
    friend class ItemRenderer;
    friend class ItemStockpile;
public:
    DEFAULT_ASSET_CONSTRUCTOR(ItemDef, AssetType::Item);

    // TODO: Remove accessors
    f32 getValue() const { return mValue; }
    f32 getWeight() const { return mWeight; }
    ui32 getMaxStockpileStackSize() const { return mStockpileStackSize; }
    TileHarvestable getSourceHarvestable() const { return mHarvestableSource; }

    SoftAssetReference mIconTextureRef = AssetType::Texture;
    SoftAssetReference mModelRef = AssetType::Model;
    ItemType mType = ItemType::UNKNOWN;
    TileHarvestable mHarvestableSource = TileHarvestable::NONE; // TODO: Resource Tags instead?
    InventoryBagType mInventoryBagType = InventoryBagType::Misc;
    // TODO: ModelDef
    f32 mValue = 1.0f;
    f32 mWeight = 0.01f;

    // Stockpile specific
    ui32 mStockpileStackSize = 10;
    ItemStockpileShape mStockpileShape = ItemStockpileShape::POINT;
    ui32v3 mStockpileStackDims = ui32v3(5, 5, 5);
};
SERIALIZABLE_IMGUI_CONTROLLED(ItemDef,
    make_field(o.mIconTextureRef, "texture"sv),
    make_field(o.mModelRef, "model"sv),
    make_field(o.mType, "type"sv),
    make_field(o.mStockpileShape, "shape"sv),
    make_field(o.mHarvestableSource, "harvest"sv),
    make_field(o.mValue, "value"sv),
    make_field(o.mWeight, "weight"sv),
    make_field(o.mStockpileStackSize, "stack_size"sv),
    make_field(o.mStockpileStackDims, "stack_dims"sv),
    make_field(o.mInventoryBagType, "inv_bag"sv)
);

struct StoredItemStack {
    ItemStack stack;
    i32v2 worldPos = {};
    bool isInContainer = false;
};
