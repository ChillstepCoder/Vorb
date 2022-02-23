#include "stdafx.h"
#include "ModelDef.h"

KEG_TYPE_DEF_SAME_NAME(ModelDef, kt) {
    kt.addValue("model", keg::Value::basic(offsetof(ModelDef, mModelName), keg::BasicType::STRING));
    kt.addValue("skeleton", keg::Value::basic(offsetof(ModelDef, mSkeletonName), keg::BasicType::STRING));
}
