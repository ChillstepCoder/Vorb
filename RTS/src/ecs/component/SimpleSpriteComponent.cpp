#include "stdafx.h"
#include "ecs/component/SimpleSpriteComponent.h"

KEG_TYPE_DEF_SAME_NAME(SimpleSpriteComponentDef , kt) {
    kt.addValue("texture", keg::Value::basic(offsetof(SimpleSpriteComponentDef, texture), keg::BasicType::C_STRING));
    kt.addValue("color", keg::Value::basic(offsetof(SimpleSpriteComponentDef, color), keg::BasicType::UI8_V4));
    kt.addValue("dims", keg::Value::basic(offsetof(SimpleSpriteComponentDef, dims), keg::BasicType::F32_V2));
}