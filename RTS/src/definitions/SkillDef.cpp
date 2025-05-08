#include "stdafx.h"
#include "SkillDef.h"

SERIALIZABLE_SIMPLE(SkillSimpleTriggerFileData,
    make_field(o.mId, "id"sv),
    make_field(o.mTime, "time"sv)
);

SERIALIZABLE_SIMPLE(SkillAttackTriggerFileData,
    make_field(o.mTime, "time"sv),
    make_field(o.mData.mSwingDir, "swing_dir"sv),
    make_field(o.mData.mSwingHeight, "swing_height"sv),
    make_field(o.mData.mRadius, "radius"sv),
    make_field(o.mData.mAngle, "angle"sv),
    make_field(o.mData.mHeight, "height"sv),
    make_field(o.mData.mShape, "shape"sv),
    make_field(o.mData.mDamageRange, "damage_range"sv)
);

SERIALIZABLE_SIMPLE(SkillDefFileData,
    make_field(o.mAnimName, "anim"sv),
    make_field(o.mHitEffectName, "hit_effect"sv),
    make_field(o.mDuration, "duration"sv),
    make_field(o.mCost, "cost"sv),
    make_field(o.mSimpleTriggers, "simple_triggers"sv),
    make_field(o.mAttackTriggers, "attack_triggers"sv)
);