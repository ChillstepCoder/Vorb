#pragma once

#include "ecs/business/BusinessComponentTypes.h"
#include "ecs/business/BusinessComponent.h"

DECL_VIO(class IOManager);
class ItemRepository;

typedef ui32 BusinessTypeID;
#define INVALID_BUSINESS_TYPE_ID UINT32_MAX

struct BusinessDef;

struct BusinessComponentDefinition {
    BusinessComponentDefinition() {};
    virtual ~BusinessComponentDefinition() {};
};

class BusinessRepository
{
public:
public:
    BusinessRepository(vio::IOManager& ioManager, ItemRepository& itemRepository);
    ~BusinessRepository();

    void loadBusinessFile(const vio::Path& filePath);
    entt::entity createBusinessEntity(City* parentCity, entt::registry& registry, const nString& typeName);

private:
    vio::IOManager& mIoManager;
    ItemRepository& mItemRepository;

    std::vector<std::unique_ptr<BusinessDef>> mBusinesses;
    std::map<nString, BusinessTypeID> mBusinessesFromName;
};

