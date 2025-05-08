#pragma once

#include "rendering/model/AnimationConst.h"
#include "combat/Attack.h"

enum class SkillTriggerType {
    Simple,
    Attack,
    COUNT
};

struct SkillAttackTrigger {
    f32v3 mSwingDir = f32v3(0.0f);
    f32 mSwingHeight = 1.3f;
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
SERIALIZABLE_DECL(SkillSimpleTriggerFileData);

struct SkillAttackTriggerFileData {
    SkillAttackTrigger mData;
    f32 mTime;
};
SERIALIZABLE_DECL(SkillAttackTriggerFileData);

struct SkillDefFileData {
    AnimationAssetRef mAnimName;
    EffectAssetRef mHitEffectName;
    f32 mDuration = 1.0f;
    f32 mCost = 0.0f;
    std::vector<SkillSimpleTriggerFileData> mSimpleTriggers;
    std::vector<SkillAttackTriggerFileData> mAttackTriggers;
};
SERIALIZABLE_DECL(SkillDefFileData);

enum class SkillDefFlags : ui8 {
    INSTANT = 1 << 0,
};

constexpr ui32 MAX_SKILL_TRIGGERS = 4;

class SkillDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(SkillDef, AssetType::Skill);

    ui32 mSkillId;
    f32 mDuration;
    f32 mCost;
    std::unique_ptr<SkillTrigger[]> mTriggers;
    ui32 mNumTriggers;
    BitFlags<SkillDefFlags> mFlags;
    AnimationAssetRef mAnimation;
    EffectAssetRef mHitEffect;
};