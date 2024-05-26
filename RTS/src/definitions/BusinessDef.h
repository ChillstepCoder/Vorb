#pragma once

#include "tile/TileConst.h"
#include "tile/TileHarvestable.h"

typedef ui32 BusinessTypeID;
#define INVALID_BUSINESS_TYPE_ID UINT32_MAX
#define PRIORITY_NO_COMPONENT UINT32_MAX

struct BusinessComponentDefinition {
    BusinessComponentDefinition() {};
    virtual ~BusinessComponentDefinition() {};
};

struct BusinessGatherComponentDef : public BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
    TileHarvestable mResourceToFind = TileHarvestable::None;
};
KEG_TYPE_DECL(BusinessGatherComponentDef);

struct BusinessBuildComponentDef : public BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
KEG_TYPE_DECL(BusinessBuildComponentDef);

struct BusinessProduceComponentDef : public BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
KEG_TYPE_DECL(BusinessProduceComponentDef);

struct BusinessRetailComponentDef : public BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
KEG_TYPE_DECL(BusinessRetailComponentDef);

struct BusinessDef {
    nString mBuildingName;
    bool mRequiresBuilding = true;
    BusinessGatherComponentDef mGather;
    BusinessBuildComponentDef mBuild;
    BusinessProduceComponentDef mProduce;
    BusinessRetailComponentDef mRetail;
    BusinessTypeID mTypeId;
    ui32 mMaxEmployeeCount = 10;
    ui32 mDesiredEmployeeCount = 1;
};
KEG_TYPE_DECL(BusinessDef);