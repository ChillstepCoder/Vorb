#pragma once

namespace ozz {
    namespace animation {
        class Animation;
    };
};

enum class AttackShape {
    CONE,
    SPHERE,
    COUNT
};
KEG_ENUM_DECL(AttackShape);

struct SkillAttackTrigger {
    f32 mTime = 0.0f;
    f32 mRadius = 1.0f;
    f32 mAngle = 90.0f; // Only used for sphere
    AttackShape mShape = AttackShape::CONE;

    // What happens on attack hit?
    // Need script!?
};
KEG_TYPE_DECL(SkillAttackTrigger);

struct SkillDefFileData {
    nString mAnimName;
    f32 mDuration = 1.0f;
    f32 mCost = 0.0f;
    Array<SkillAttackTrigger> mAttackTriggers;
};
KEG_TYPE_DECL(SkillDefFileData);

enum class SkillDefFlags : ui8 {
    INSTANT = 1 << 0,
};

constexpr ui32 MAX_SKILL_ATTACK_TRIGGERS = 4;

struct SkillDef {
    ui32 mSkillId;
    f32 mDuration;
    f32 mCost;
    SkillAttackTrigger mAttackTriggers[MAX_SKILL_ATTACK_TRIGGERS];
    ui32 mNumAttackTriggers;
    BitFlags<SkillDefFlags> mFlags;
    const ozz::animation::Animation* mAnim = nullptr;
};

