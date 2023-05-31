#include "stdafx.h"
#include "FishRepository.h"

#include "item/ItemRepository.h"
#include "item/ItemFileData.h"

#include "resources/ModelRepository.h"

#include <Vorb/io/IOManager.h>

struct FishFileData {
    ItemFileData itemData;
    nString modelName;
};
KEG_TYPE_DEF_SAME_NAME(FishFileData, kt) {
    kt.addValue("item", keg::Value::custom(offsetof(FishFileData, itemData), "ItemFileData", false));
    kt.addValue("model", keg::Value::basic(offsetof(FishFileData, modelName), keg::BasicType::STRING));
}

FishRepository::FishRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {

}

FishRepository::~FishRepository() {

}

void FishRepository::loadFishFile(const vio::Path& filePath, ModelRepository& modelRepo, ItemRepository& itemRepo, TextureRepository& textureRepo)
{
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        FishFileData fileData;
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(FishFileData));

        FishDef& newFish = mFishDefinitions.emplace_back();
        newFish.mId = mFishDefinitions.size() - 1;
        newFish.mItem = itemRepo.addItem(key, fileData.itemData);
        if (fileData.modelName.empty()) {
            LOG_CRITICAL("Missing model name for {}", filePath.getString());
            assert(false);
        }
        newFish.mModel = modelRepo.getModelID(fileData.modelName);

        // TODO: MinigameData

        // TODO: Check for mod conflicts
        mFishIdLookup[key] = newFish.mId;

    }))) {
        // Do nothing on success
    }
    else {
        // Failure case
        pError("Failed to parse fish file " + filePath.getString());
    }
}

const FishDef& FishRepository::getFish(const nString& itemName) const {
    auto&& it = mFishIdLookup.find(itemName);
    assert(it != mFishIdLookup.end());
    return mFishDefinitions[it->second];
}
