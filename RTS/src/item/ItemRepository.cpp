#include "stdafx.h"
#include "ItemRepository.h"

#include <Vorb/io/IOManager.h>

struct ItemDef {
    nString name;
    nString textureName;
    ItemType type;
    f32 value;
    f32 weight;
    ui32 stackSize;
};
KEG_TYPE_DEF_SAME_NAME(ItemDef, kt) {
    kt.addValue("type", keg::Value::custom(offsetof(ItemDef, type), "ItemType", true));
    kt.addValue("texture", keg::Value::basic(offsetof(ItemDef, textureName), keg::BasicType::STRING));
    kt.addValue("value", keg::Value::basic(offsetof(ItemDef, value), keg::BasicType::F32));
    kt.addValue("weight", keg::Value::basic(offsetof(ItemDef, weight), keg::BasicType::F32));
    kt.addValue("stack_size", keg::Value::basic(offsetof(ItemDef, stackSize), keg::BasicType::UI32));
}

ItemRepository::ItemRepository(vio::IOManager& ioManager) :
    mIoManager(ioManager) {

    // Add the null item, no lookup
    mItems.emplace_back();
}

void ItemRepository::loadItemFile(const vio::Path& filePath)
{
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        ItemDef def;
        keg::parse((ui8*)&def, value, readContext, &KEG_GLOBAL_TYPE(ItemDef));
        
        Item& newItem = mItems.emplace_back();
        newItem.mId = mItems.size() - 1;
        newItem.mName = key;
        newItem.mTextureName = def.textureName;
        newItem.mValue = def.value;
        newItem.mWeight = def.weight;
        newItem.mStackSize = def.stackSize;
        static_assert(sizeof(ItemDef) == 96, "Update new values"); //SIZER(ItemDef) tmp;
        
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
