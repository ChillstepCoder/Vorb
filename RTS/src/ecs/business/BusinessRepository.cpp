#include "stdafx.h"
#include "BusinessRepository.h"

#include "city/City.h"
#include "city/CityBusinessManager.h"

#include "item/ItemRepository.h"

#include <Vorb/io/IOManager.h>

#define PRIORITY_NO_COMPONENT UINT32_MAX


struct BusinessGatherComponentDef : BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
    TileResource mResourceToFind = TileResource::NONE;
};
KEG_TYPE_DEF_SAME_NAME(BusinessGatherComponentDef, kt) {
    kt.addValue("priority", keg::Value::basic(offsetof(BusinessGatherComponentDef, mPriority), keg::BasicType::UI32));
    kt.addValue("resource", keg::Value::custom(offsetof(BusinessGatherComponentDef, mResourceToFind), "TileResource", true));
}

struct BusinessProduceComponentDef : BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
KEG_TYPE_DEF_SAME_NAME(BusinessProduceComponentDef, kt) {
    kt.addValue("priority", keg::Value::basic(offsetof(BusinessProduceComponentDef, mPriority), keg::BasicType::UI32));
}

struct BusinessRetailComponentDef : BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
KEG_TYPE_DEF_SAME_NAME(BusinessRetailComponentDef, kt) {
    kt.addValue("priority", keg::Value::basic(offsetof(BusinessRetailComponentDef, mPriority), keg::BasicType::UI32));
}

// TODO: Should we shrink this? Right now it uses maximum memory for all components
struct BusinessDef {
    nString mBuildingName;
    bool mRequiresBuilding = true;
    BusinessGatherComponentDef mGather;
    BusinessProduceComponentDef mProduce;
    BusinessRetailComponentDef mRetail;
    BusinessTypeID mTypeId;
    ui32 mMaxEmployeeCount = 10;
    ui32 mDesiredEmployeeCount = 1;
};
KEG_TYPE_DEF_SAME_NAME(BusinessDef, kt) {
    kt.addValue("building", keg::Value::basic(offsetof(BusinessDef, mBuildingName), keg::BasicType::STRING));
    kt.addValue("requires_building", keg::Value::basic(offsetof(BusinessDef, mRequiresBuilding), keg::BasicType::BOOL));
    kt.addValue("gather", keg::Value::custom(offsetof(BusinessDef, mGather), "BusinessGatherComponentDef", false));
    kt.addValue("produce", keg::Value::custom(offsetof(BusinessDef, mProduce), "BusinessProduceComponentDef", false));
    kt.addValue("retail", keg::Value::custom(offsetof(BusinessDef, mRetail), "BusinessRetailComponentDef", false));
    kt.addValue("max_employees", keg::Value::basic(offsetof(BusinessDef, mMaxEmployeeCount), keg::BasicType::UI32));
    kt.addValue("desired_employees", keg::Value::basic(offsetof(BusinessDef, mDesiredEmployeeCount), keg::BasicType::UI32));
    // When adding new components, make sure to convert them to BusinessDescription below in loadBusinessFile
}

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

        def.mTypeId = mBusinesses.size() - 1;

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

    // Init components
    // TODO: Do something with priority
    if (def.mGather.mPriority != PRIORITY_NO_COMPONENT) {
        auto&& cmp = registry.emplace<BusinessGatherComponent>(newEntity);
        cmp.mResourceToGather = def.mGather.mResourceToFind;
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
