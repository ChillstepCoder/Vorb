#include "stdafx.h"
#include "FishRepository.h"

#include "item/ItemRepository.h"
#include "item/ItemFileData.h"

#include <Vorb/io/IOManager.h>

struct FishFileData {
    ItemFileData itemData;
};
KEG_TYPE_DEF_SAME_NAME(FishFileData, kt) {
    kt.addValue("item", keg::Value::custom(offsetof(FishFileData, itemData), "ItemFileData", false));
}

FishRepository::FishRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {

}

FishRepository::~FishRepository()
{

}

void FishRepository::loadFishFile(const vio::Path& filePath, ItemRepository& itemRepo, TextureRepository& textureRepo)
{
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        FishFileData fileData;
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(FishFileData));

        FishDef& newFish = mFishDefinitions.emplace_back();
        newFish.mId = mFishDefinitions.size() - 1;
        newFish.mItem = itemRepo.addItem(key, fileData.itemData);

        // TODO: MinigameData

        // TODO: Check for mod conflicts
        mFishIdLookup[key] = newFish.mId;

    }))) {
        // Do nothing on success
    }
    else {
        // Failure case
        pError("Failed to parse item file " + filePath.getString());
    }
}

const FishDef& FishRepository::getFish(const nString& itemName) const
{
   
}
