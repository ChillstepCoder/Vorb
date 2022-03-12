#include "stdafx.h"
#include "LocomotionComponent.h"

KEG_TYPE_DEF_SAME_NAME(LocomotionComponentDef, kt) {
    kt.addValue("speed", keg::Value::basic(offsetof(LocomotionComponentDef, mSpeed), keg::BasicType::F32));
}