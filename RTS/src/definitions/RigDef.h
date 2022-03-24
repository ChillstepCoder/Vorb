#pragma once

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>

#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>

struct RigDefFileData {
    Array<nString> mAnimationNames;
    nString mSkeletonFileName;
    nString mUpperRootJointName;
};
KEG_TYPE_DECL(RigDefFileData);

typedef const ozz::animation::Animation* ConstOzzAnimationPtr;

struct RigDef {
    std::map<nString, ui32> mNameToAnimationIndex;
    std::unique_ptr<ConstOzzAnimationPtr[]> mAnimations;
    ozz::animation::Skeleton mSkeleton;
    ozz::vector<ozz::math::SimdFloat4> mUpperBodyJointWeights;
    ozz::vector<ozz::math::SimdFloat4> mLowerBodyJointWeights;
    ui32 mNumAnimations;
    ui32 mRigId;
};