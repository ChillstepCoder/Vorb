#pragma once

// TODO: instead have the ItemRenderer manage this mapping
#include "rendering/SpriteData.h"

typedef ui32 ItemID;

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

struct ItemStack {
    ItemID id = 0;
    ui32 quantity = 0;
};

class Item
{
    friend class ItemRepository;
public:
    const nString& getName() { return mName; }
    ItemID getID() { return mId; }
    f32 getValue() { return mValue; }
    f32 getWeight() { return mWeight; }

protected:
    nString mName;
    nString mTextureName;
    ItemType mType = ItemType::UNKNOWN;
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