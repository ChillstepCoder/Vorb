#include "stdafx.h"
#include "ItemRepository.h"

#include "resources/TextureRepository.h"

#include <Vorb/io/IOManager.h>

struct ItemDef {
    nString name;
    nString textureName;
    ItemType type = ItemType::UNKNOWN;
    ItemStorageShape shape = ItemStorageShape::POINT;
    f32 value = 1.0f;
    f32 weight = 0.01f;
    ui32 stackSize = 10;
    ui32v3 stackDims = ui32v3(5, 5, 5);
};
KEG_TYPE_DEF_SAME_NAME(ItemDef, kt) {
    kt.addValue("type", keg::Value::custom(offsetof(ItemDef, type), "ItemType", true));
    kt.addValue("shape", keg::Value::custom(offsetof(ItemDef, shape), "ItemStorageShape", true));
    kt.addValue("texture", keg::Value::basic(offsetof(ItemDef, textureName), keg::BasicType::STRING));
    kt.addValue("value", keg::Value::basic(offsetof(ItemDef, value), keg::BasicType::F32));
    kt.addValue("weight", keg::Value::basic(offsetof(ItemDef, weight), keg::BasicType::F32));
    kt.addValue("stack_size", keg::Value::basic(offsetof(ItemDef, stackSize), keg::BasicType::UI32));
    kt.addValue("stack_dims", keg::Value::basic(offsetof(ItemDef, stackDims), keg::BasicType::UI32_V3));
}

ItemRepository::ItemRepository(vio::IOManager& ioManager) :
    mIoManager(ioManager) {

    // Add the null item, no lookup
    mItems.emplace_back();
}

void ItemRepository::loadItemFile(const vio::Path& filePath, TextureRepository& textureRepo)
{
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        ItemDef def;
        keg::parse((ui8*)&def, value, readContext, &KEG_GLOBAL_TYPE(ItemDef));
        
        Item& newItem = mItems.emplace_back();
        newItem.mShape = def.shape;
        newItem.mId = mItems.size() - 1;
        newItem.mName = key;
        newItem.mTexture = textureRepo.getTexture(def.textureName);
        newItem.mValue = def.value;
        newItem.mWeight = def.weight;
        newItem.mStackSize = def.stackSize;
        newItem.mStackDims = def.stackDims;
        
        // TODO: Check for mod conflicts
        mItemIdLookup[key] = newItem.mId;

    }))) {
        // Do nothing on success
    }
    else {
        // Failure case
        pError("Failed to parse item file " + filePath.getString());
    }
}

const Item& ItemRepository::getItem(const nString& itemName) const {
    auto&& it = mItemIdLookup.find(itemName);
    assert(it != mItemIdLookup.end());
    return mItems[it->second];
}
