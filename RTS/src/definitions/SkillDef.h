#pragma once

#include "rendering/model/AnimationConst.h"
#include "combat/Attack.h"

enum class SkillTriggerType {
    Simple,
    Attack,
    COUNT
};

struct SkillAttackTrigger {
    f32 mRadius;
    f32 mAngle;
    AttackShape mShape;
};

struct SkillTrigger {
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
KEG_TYPE_DECL(SkillSimpleTriggerFileData);

struct SkillAttackTriggerFileData {
    SkillAttackTrigger mData;
    f32 mTime;
};
KEG_TYPE_DECL(SkillAttackTriggerFileData);

struct SkillDefFileData {
    nString mAnimName;
    f32 mDuration = 1.0f;
    f32 mCost = 0.0f;
    Array<SkillSimpleTriggerFileData> mSimpleTriggers;
    Array<SkillAttackTriggerFileData> mAttackTriggers;
};
KEG_TYPE_DECL(SkillDefFileData);

enum class SkillDefFlags : ui8 {
    INSTANT = 1 << 0,
};

constexpr ui32 MAX_SKILL_TRIGGERS = 4;

struct SkillDef {
    ui32 mSkillId;
    f32 mDuration;
    f32 mCost;
    std::unique_ptr<SkillTrigger[]> mTriggers;
    ui32 mNumTriggers;
    BitFlags<SkillDefFlags> mFlags;
    AnimationID mAnimID;
};

