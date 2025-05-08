#pragma once

// TODO: THIS FILE IS DEPRECATED


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
SERIALIZABLE_SIMPLE(BusinessGatherComponentDef,
    make_field(o.mPriority, "priority"sv),
    make_field(o.mResourceToFind, "resource"sv)
);

struct BusinessBuildComponentDef : public BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
SERIALIZABLE_SIMPLE(BusinessBuildComponentDef,
    make_field(o.mPriority, "priority"sv)
);

struct BusinessProduceComponentDef : public BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
SERIALIZABLE_SIMPLE(BusinessProduceComponentDef,
    make_field(o.mPriority, "priority"sv)
);

struct BusinessRetailComponentDef : public BusinessComponentDefinition {
    ui32 mPriority = PRIORITY_NO_COMPONENT;
};
SERIALIZABLE_SIMPLE(BusinessRetailComponentDef,
    make_field(o.mPriority, "priority"sv)
);

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
SERIALIZABLE_SIMPLE(BusinessDef,
    make_field(o.mBuildingName, "building"sv),
    make_field(o.mRequiresBuilding, "requires_building"sv),
    make_field(o.mGather, "gather"sv),
    make_field(o.mBuild, "build"sv),
    make_field(o.mProduce, "produce"sv),
    make_field(o.mRetail, "retail"sv),
    make_field(o.mMaxEmployeeCount, "max_employees"sv),
    make_field(o.mDesiredEmployeeCount, "desired_employees"sv)
);
