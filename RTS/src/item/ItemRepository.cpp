#include "stdafx.h"
#include "ItemRepository.h"

#include "item/ItemFileData.h"

#include "resources/TextureRepository.h"

#include <Vorb/io/IOManager.h>

ItemRepository* sItemRepository = nullptr;

KEG_TYPE_DEF_SAME_NAME(ItemFileData, kt) {
    kt.addValue("type", keg::Value::custom(offsetof(ItemFileData, type), "ItemType", true));
    kt.addValue("shape", keg::Value::custom(offsetof(ItemFileData, shape), "ItemStorageShape", true));
    kt.addValue("texture", keg::Value::basic(offsetof(ItemFileData, textureName), keg::BasicType::STRING));
    kt.addValue("value", keg::Value::basic(offsetof(ItemFileData, value), keg::BasicType::F32));
    kt.addValue("weight", keg::Value::basic(offsetof(ItemFileData, weight), keg::BasicType::F32));
    kt.addValue("stack_size", keg::Value::basic(offsetof(ItemFileData, stackSize), keg::BasicType::UI32));
    kt.addValue("stack_dims", keg::Value::basic(offsetof(ItemFileData, stackDims), keg::BasicType::UI32_V3));
    kt.addValue("harvest", keg::Value::custom(offsetof(ItemFileData, harvestableSource), "TileHarvestable", true));
}

ItemRepository::ItemRepository(vio::IOManager& ioManager) :
    mIoManager(ioManager) {
    assert(!sItemRepository);
    sItemRepository = this;
    // Add the null item, no lookup
    mItems.emplace_back();
}

ItemRepository::~ItemRepository() {
    sItemRepository = nullptr;
}

void ItemRepository::loadItemFile(const vio::Path& filePath, TextureRepository& textureRepo) {
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        ItemFileData fileData;
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(ItemFileData));
        
        addItem(key, fileData);

    }))) {
        // Do nothing on success
    }
    else {
        // Failure case
        pError("Failed to parse item file " + filePath.getString());
    }
}

ItemID ItemRepository::addItem(const nString& itemName, const ItemFileData& fileData) {
    ItemDef& newItem = mItems.emplace_back();
    newItem.mShape = fileData.shape;
    newItem.mId = mItems.size() - 1;
    newItem.mName = itemName;
    // TODO: Make this work
    //newItem.mTexture = textureRepo.getSubTextureOLD(def.textureName);
    newItem.mValue = fileData.value;
    newItem.mWeight = fileData.weight;
    newItem.mStackSize = fileData.stackSize;
    newItem.mStackDims = fileData.stackDims;
    newItem.mHarvestableSource = fileData.harvestableSource;

    // TODO: Check for mod conflicts
    mItemIdLookup[itemName] = newItem.mId;

    return newItem.mId;
}

const ItemDef& ItemRepository::getItem(const nString& itemName) const {
    auto&& it = mItemIdLookup.find(itemName);
    assert(it != mItemIdLookup.end());
    return mItems[it->second];
}
