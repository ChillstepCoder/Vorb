#include "stdafx.h"
#include "SkillDef.h"

KEG_TYPE_DEF_SAME_NAME(SkillAttackTriggerFileData, kt) {
    kt.addValue("time", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mTime), keg::BasicType::F32));
    kt.addValue("radius", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mRadius), keg::BasicType::F32));
    kt.addValue("angle", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mAngle), keg::BasicType::F32));
    kt.addValue("height", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mHeight), keg::BasicType::F32));
    kt.addValue("shape", keg::Value::custom(offsetof(SkillAttackTriggerFileData, mData.mShape), "AttackShape", true));
    kt.addValue("damage_range", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mDamageRange), keg::BasicType::UI16_V2));
}

KEG_TYPE_DEF_SAME_NAME(SkillSimpleTriggerFileData, kt) {
    kt.addValue("time", keg::Value::basic(offsetof(SkillSimpleTriggerFileData, mTime), keg::BasicType::F32));
    kt.addValue("id", keg::Value::basic(offsetof(SkillSimpleTriggerFileData, mId), keg::BasicType::I32));
}

KEG_TYPE_DEF_SAME_NAME(SkillDefFileData, kt) {
    kt.addValue("anim", keg::Value::basic(offsetof(SkillDefFileData, mAnimName), keg::BasicType::STRING));
    kt.addValue("duration", keg::Value::basic(offsetof(SkillDefFileData, mDuration), keg::BasicType::F32));
    kt.addValue("cost", keg::Value::basic(offsetof(SkillDefFileData, mCost), keg::BasicType::F32));
    kt.addValue("attack_triggers", keg::Value::array(offsetof(SkillDefFileData, mAttackTriggers), keg::Value::custom(0, "SkillAttackTriggerFileData", false)));
    kt.addValue("simple_triggers", keg::Value::array(offsetof(SkillDefFileData, mSimpleTriggers), keg::Value::custom(0, "SkillSimpleTriggerFileData", false)));
}