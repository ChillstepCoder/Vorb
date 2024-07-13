#include "stdafx.h"
#include "BusinessRepository.h"

#include "city/CityBusinessManager.h"

#include "item/ItemRepository.h"

#include "ecs/component/OwnershipComponent.h"

#include <Vorb/io/IOManager.h>

BusinessRepository::BusinessRepository(vio::IOManager& ioManager) :
    mIoManager(ioManager) {

}

BusinessRepository::~BusinessRepository() {

}

void BusinessRepository::loadBusinessFile(const vio::Path& filePath) {

    BusinessDef def;
    // TODO: DELETE ME WHEN USING ASSET REPO BASE
    nString fileData;
    if (!mIoManager.readFileToString(filePath, fileData)) {
        panic("Asset repository failed to read file {}", filePath.getCString());
    }
    YmlSerializer::readFileData(fileData, def);

    def.mTypeId = (BusinessTypeID)(mBusinesses.size() - 1);

    // TODO: Check for mod conflicts
    mBusinessesFromName[filePath.getFileNameNoExtension()] = def.mTypeId;
}

entt::entity BusinessRepository::createBusinessEntity(entt::registry& registry, const nString& typeName) {

    auto&& it = mBusinessesFromName.find(typeName);
    assert(it != mBusinessesFromName.end());
    BusinessDef& def = *mBusinesses[it->second];

    const entt::entity newEntity = registry.create();

    auto&& businessCmp = registry.emplace<BusinessComponent>(newEntity);
    //businessCmp.mCity = parentCity;
    businessCmp.mMaxEmployeeCount = def.mMaxEmployeeCount;
    businessCmp.mDesiredEmployeeCount = def.mDesiredEmployeeCount;
    businessCmp.mBusinessDef = &def;

    // Init components
    // TODO: Do something with priority
    if (def.mGather.mPriority != PRIORITY_NO_COMPONENT) {
        auto&& cmp = registry.emplace<BusinessGatherComponent>(newEntity);
        cmp.mResourceToGather = def.mGather.mResourceToFind;
    }
    if (def.mBuild.mPriority != PRIORITY_NO_COMPONENT) {
        auto&& cmp = registry.emplace<BusinessBuildComponent>(newEntity);
        cmp.mCurrentBlueprint = nullptr;
    }
    if (def.mProduce.mPriority != PRIORITY_NO_COMPONENT) {
        registry.emplace<BusinessProduceComponent>(newEntity);
    }
    if (def.mRetail.mPriority != PRIORITY_NO_COMPONENT) {
        registry.emplace<BusinessRetailComponent>(newEntity);
    }

    registry.emplace<OwnershipComponent>(newEntity);

   // parentCity->getBusinessManager().registerBusiness(newEntity);

    return newEntity;
}
