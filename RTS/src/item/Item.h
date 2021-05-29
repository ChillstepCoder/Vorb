#pragma once

// TODO: instead have the ItemRenderer manage this mapping
#include "rendering/SpriteData.h"

typedef ui32 ItemID;
constexpr ui32 INVALID_ITEM_ID = UINT32_MAX;

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
    ItemID id = INVALID_ITEM_ID;
    ui32 quantity = 0;
};

class Item
{
    friend class ItemRepository;
public:
    const nString& getName() const { return mName; }
    ItemID getID() const { return mId; }
    f32 getValue() const { return mValue; }
    f32 getWeight() const { return mWeight; }

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