#include "stdafx.h"
#include "SkillsComponent.h"

KEG_TYPE_DEF_SAME_NAME(SkillsComponentFileData, kt) {
    kt.addValue("skill_names", keg::Value::array(offsetof(SkillsComponentFileData, mSkillNames), keg::BasicType::STRING));
}