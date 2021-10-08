#pragma once

// TODO: instead have the ItemRenderer manage this mapping
#include "rendering/SpriteData.h"

#include "item/ItemStack.h"

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
    TYPES
};
KEG_ENUM_DECL(ItemType);

enum class ItemStorageShape {
    POINT,
    QUAD_SHAPES_START,
    PLANK = QUAD_SHAPES_START,
    LOG,
    INGOT,
    COUNT
};
KEG_ENUM_DECL(ItemStorageShape);

class Item
{
    friend class ItemRepository;
    friend class ItemRenderer;
    friend class ItemStockpile;
public:
    const nString& getName() const { return mName; }
    ItemID getID() const { return mId; }
    f32 getValue() const { return mValue; }
    f32 getWeight() const { return mWeight; }
    ui32 getStackSize() const { return mStackSize; }

protected:
    nString mName;
    ItemType mType = ItemType::UNKNOWN;
    ItemStorageShape mShape = ItemStorageShape::POINT;
    ItemID mId;
    SpriteData mSpriteData; // TODO: instead have the ItemRenderer manage this mapping
    f32 mValue = 1.0f;
    f32 mWeight = 0.01f;
    ui32 mStackSize = 10;
};
KEG_TYPE_DECL(Item);

struct StoredItemStack {
    ItemStack stack;
    ui32v2 worldPos = {};
    bool isInContainer = false;
};