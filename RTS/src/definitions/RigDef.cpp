#include "stdafx.h"
#include "RigDef.h"

KEG_TYPE_DEF_SAME_NAME(RigDef, kt) {
    kt.addValue("skeleton", keg::Value::basic(offsetof(RigDef, mSkeletonFileName), keg::BasicType::STRING));
    kt.addValue("anims", keg::Value::array(offsetof(RigDef, mAnimationFileNames), keg::BasicType::STRING));
}
