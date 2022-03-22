#include "stdafx.h"
#include "RigDef.h"

KEG_TYPE_DEF_SAME_NAME(RigDefFileData, kt) {
    kt.addValue("skeleton", keg::Value::basic(offsetof(RigDefFileData, mSkeletonFileName), keg::BasicType::STRING));
    kt.addValue("anims", keg::Value::array(offsetof(RigDefFileData, mAnimationNames), keg::BasicType::STRING));
}
