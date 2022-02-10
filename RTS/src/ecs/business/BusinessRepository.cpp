#include "stdafx.h"
#include "BusinessRepository.h"

#include "city/City.h"
#include "city/CityBusinessManager.h"

#include "item/ItemRepository.h"

#include <Vorb/io/IOManager.h>

BusinessRepository::BusinessRepository(vio::IOManager& ioManager, ItemRepository& itemRepository) :
    mIoManager(ioManager),
    mItemRepository(itemRepository) {

}

BusinessRepository::~BusinessRepository() {

}

void BusinessRepository::loadBusinessFile(const vio::Path& filePath)
{
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        BusinessDef& def = *mBusinesses.emplace_back(std::make_unique<BusinessDef>());
        keg::parse((ui8*)&def, value, readContext, &KEG_GLOBAL_TYPE(BusinessDef));

        def.mTypeId = (BusinessTypeID)(mBusinesses.size() - 1);

        // TODO: Check for mod conflicts
        mBusinessesFromName[key] = def.mTypeId;

    }))) {
        // Do nothing on success
    }
    else {
        // Failure case
        pError("Failed to parse item file " + filePath.getString());
    }
}

entt::entity BusinessRepository::createBusinessEntity(City* parentCity, entt::registry& registry, const nString& typeName)
{
    assert(parentCity); // Currently required

    auto&& it = mBusinessesFromName.find(typeName);
    assert(it != mBusinessesFromName.end());
    BusinessDef& def = *mBusinesses[it->second];

    const entt::entity newEntity = registry.create();

    auto&& businessCmp = registry.emplace<BusinessComponent>(newEntity);
    businessCmp.mCity = parentCity;
    businessCmp.mMaxEmployeeCount = def.mMaxEmployeeCount;
    businessCmp.mDesiredEmployeeCount = def.mDesiredEmployeeCount;

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
        registry.emplace<BusinessRetailComponentDef>(newEntity);
    }

    parentCity->getBusinessManager().registerBusiness(newEntity);

    return newEntity;
}
