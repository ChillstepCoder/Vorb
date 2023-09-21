#pragma once

#include "ecs/business/BusinessComponentTypes.h"
#include "ecs/business/BusinessComponent.h"
#include "definitions/BusinessDef.h"

DECL_VIO(class IOManager);
class ItemRepository;


class BusinessRepository
{
public:
public:
    BusinessRepository(vio::IOManager& ioManager);
    ~BusinessRepository();

    void loadBusinessFile(const vio::Path& filePath);
    entt::entity createBusinessEntity(City* parentCity, entt::registry& registry, const nString& typeName);

private:
    vio::IOManager& mIoManager;

    std::vector<std::unique_ptr<BusinessDef>> mBusinesses;
    std::map<nString, BusinessTypeID> mBusinessesFromName;
};

