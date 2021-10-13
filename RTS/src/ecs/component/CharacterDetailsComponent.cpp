#include "stdafx.h"
#include "CharacterDetailsComponent.h"

KEG_TYPE_DEF_SAME_NAME(CharacterDetailsComponentDef, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(CharacterDetailsComponentDef, name), keg::BasicType::C_STRING));
}