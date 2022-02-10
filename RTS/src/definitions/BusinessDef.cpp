#include "stdafx.h"
#include "BusinessDef.h"

KEG_TYPE_DEF_SAME_NAME(BusinessGatherComponentDef, kt) {
    kt.addValue("priority", keg::Value::basic(offsetof(BusinessGatherComponentDef, mPriority), keg::BasicType::UI32));
    kt.addValue("resource", keg::Value::custom(offsetof(BusinessGatherComponentDef, mResourceToFind), "TileResource", true));
}

KEG_TYPE_DEF_SAME_NAME(BusinessBuildComponentDef, kt) {
    kt.addValue("priority", keg::Value::basic(offsetof(BusinessBuildComponentDef, mPriority), keg::BasicType::UI32));
}

KEG_TYPE_DEF_SAME_NAME(BusinessProduceComponentDef, kt) {
    kt.addValue("priority", keg::Value::basic(offsetof(BusinessProduceComponentDef, mPriority), keg::BasicType::UI32));
}

KEG_TYPE_DEF_SAME_NAME(BusinessRetailComponentDef, kt) {
    kt.addValue("priority", keg::Value::basic(offsetof(BusinessRetailComponentDef, mPriority), keg::BasicType::UI32));
}

// TODO: Should we shrink this? Right now it uses maximum memory for all components, maybe vector of component defs?

KEG_TYPE_DEF_SAME_NAME(BusinessDef, kt) {
    kt.addValue("building", keg::Value::basic(offsetof(BusinessDef, mBuildingName), keg::BasicType::STRING));
    kt.addValue("requires_building", keg::Value::basic(offsetof(BusinessDef, mRequiresBuilding), keg::BasicType::BOOL));
    kt.addValue("gather", keg::Value::custom(offsetof(BusinessDef, mGather), "BusinessGatherComponentDef", false));
    kt.addValue("build", keg::Value::custom(offsetof(BusinessDef, mBuild), "BusinessBuildComponentDef", false));
    kt.addValue("produce", keg::Value::custom(offsetof(BusinessDef, mProduce), "BusinessProduceComponentDef", false));
    kt.addValue("retail", keg::Value::custom(offsetof(BusinessDef, mRetail), "BusinessRetailComponentDef", false));
    kt.addValue("max_employees", keg::Value::basic(offsetof(BusinessDef, mMaxEmployeeCount), keg::BasicType::UI32));
    kt.addValue("desired_employees", keg::Value::basic(offsetof(BusinessDef, mDesiredEmployeeCount), keg::BasicType::UI32));
    // When adding new components, make sure to convert them to BusinessDescription below in loadBusinessFile
}
