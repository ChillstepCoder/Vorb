#pragma once

#include "rendering/model/AnimationConst.h"
#include "combat/Attack.h"

enum class SkillTriggerType {
    Simple,
    Attack,
    COUNT
};

struct SkillAttackTrigger {
    f32 mRadius = 1.0f;
    f32 mAngle = 120.0f;
    f32 mHeight = 1.5f;
    AttackShape mShape;
    ui16v2 mDamageRange = ui16v2(35ui16, 50ui16);
};

struct SkillTrigger {
    SkillTrigger() : mAttackTrigger() {};

    union {
        int mSimpleTrigger;
        SkillAttackTrigger mAttackTrigger;
    };
    f32 mTime = 0.0f;
    SkillTriggerType mType = SkillTriggerType::Simple;
};

struct SkillSimpleTriggerFileData {
    int mId;
    f32 mTime;
};
SERIALIZABLE_SIMPLE(SkillSimpleTriggerFileData,
    make_field(o.mId, "id"sv),
    make_field(o.mTime, "time"sv)
);

struct SkillAttackTriggerFileData {
    SkillAttackTrigger mData;
    f32 mTime;
};

KEG_TYPE_DEF_SAME_NAME(SkillAttackTriggerFileData, kt) {
    kt.addValue("time", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mTime), keg::BasicType::F32));
    kt.addValue("radius", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mRadius), keg::BasicType::F32));
    kt.addValue("angle", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mAngle), keg::BasicType::F32));
    kt.addValue("height", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mHeight), keg::BasicType::F32));
    kt.addValue("shape", keg::Value::custom(offsetof(SkillAttackTriggerFileData, mData.mShape), "AttackShape", true));
    kt.addValue("damage_range", keg::Value::basic(offsetof(SkillAttackTriggerFileData, mData.mDamageRange), keg::BasicType::UI16_V2));
}

struct SkillDefFileData {
    StrToken mAnimName;
    f32 mDuration = 1.0f;
    f32 mCost = 0.0f;
    std::vector<SkillSimpleTriggerFileData> mSimpleTriggers;
    std::vector<SkillAttackTriggerFileData> mAttackTriggers;
};
KEG_TYPE_DEF_SAME_NAME(SkillDefFileData, kt) {
    kt.addValue("anim", keg::Value::basic(offsetof(SkillDefFileData, mAnimName), keg::BasicType::STRING));
    kt.addValue("duration", keg::Value::basic(offsetof(SkillDefFileData, mDuration), keg::BasicType::F32));
    kt.addValue("cost", keg::Value::basic(offsetof(SkillDefFileData, mCost), keg::BasicType::F32));
    kt.addValue("attack_triggers", keg::Value::array(offsetof(SkillDefFileData, mAttackTriggers), keg::Value::custom(0, "SkillAttackTriggerFileData", false)));
    kt.addValue("simple_triggers", keg::Value::array(offsetof(SkillDefFileData, mSimpleTriggers), keg::Value::custom(0, "SkillSimpleTriggerFileData", false)));
}

enum class SkillDefFlags : ui8 {
    INSTANT = 1 << 0,
};

constexpr ui32 MAX_SKILL_TRIGGERS = 4;

class SkillDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(SkillDef);

    ui32 mSkillId;
    f32 mDuration;
    f32 mCost;
    std::unique_ptr<SkillTrigger[]> mTriggers;
    ui32 mNumTriggers;
    BitFlags<SkillDefFlags> mFlags;
    AssetID mAnimID;
};

