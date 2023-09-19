#pragma once

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>

#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>

struct RigDefFileData {
    std::vector<StrToken> mAnimationNames;
    StrToken mSkeletonFileName;
    StrToken mUpperRootJointName;
};
SERIALIZABLE_SIMPLE(RigDefFileData,
    make_field(o.mAnimationNames, "anims"sv),
    make_field(o.mSkeletonFileName, "skeleton"sv),
    make_field(o.mUpperRootJointName, "upper_root"sv)
);

typedef const ozz::animation::Animation* ConstOzzAnimationPtr;

class RigDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(RigDef);

    std::map<StrToken, ui32> mNameToAnimationIndex;
    std::unique_ptr<ConstOzzAnimationPtr[]> mAnimations;
    ozz::animation::Skeleton mSkeleton;
    ozz::vector<ozz::math::SimdFloat4> mUpperBodyJointWeights;
    ozz::vector<ozz::math::SimdFloat4> mLowerBodyJointWeights;
    ui32 mNumAnimations;
    ui32 mRigId;
};