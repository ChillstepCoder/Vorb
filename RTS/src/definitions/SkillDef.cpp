#include "stdafx.h"
#include "SkillDef.h"

KEG_ENUM_DEF(AttackShape, AttackShape, kt) {
    kt.addValue("cone", AttackShape::CONE);
    kt.addValue("sphere", AttackShape::SPHERE);
}
static_assert(e_cast(AttackShape::COUNT) == 2, "Update def");

KEG_TYPE_DEF_SAME_NAME(SkillAttackTrigger, kt) {
    kt.addValue("time", keg::Value::basic(offsetof(SkillAttackTrigger, mTime), keg::BasicType::F32));
    kt.addValue("radius", keg::Value::basic(offsetof(SkillAttackTrigger, mRadius), keg::BasicType::F32));
    kt.addValue("angle", keg::Value::basic(offsetof(SkillAttackTrigger, mAngle), keg::BasicType::F32));
    kt.addValue("shape", keg::Value::custom(offsetof(SkillAttackTrigger, mShape), "AttackShape", true));
}

KEG_TYPE_DEF_SAME_NAME(SkillDefFileData, kt) {
    kt.addValue("anim", keg::Value::basic(offsetof(SkillDefFileData, mAnimName), keg::BasicType::STRING));
    kt.addValue("duration", keg::Value::basic(offsetof(SkillDefFileData, mDuration), keg::BasicType::F32));
    kt.addValue("cost", keg::Value::basic(offsetof(SkillDefFileData, mCost), keg::BasicType::F32));
    kt.addValue("attacks", keg::Value::array(offsetof(SkillDefFileData, mAttackTriggers), keg::Value::custom(0, "SkillAttackTriggerFileData", false)));
}