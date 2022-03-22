#pragma once

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>

struct RigDefFileData {
    Array<nString> mAnimationNames;
    nString mSkeletonFileName;
};
KEG_TYPE_DECL(RigDefFileData);

typedef const ozz::animation::Animation* ConstOzzAnimationPtr;

struct RigDef {
    std::map<nString, ui32> mNameToAnimationIndex;
    std::unique_ptr<ConstOzzAnimationPtr[]> mAnimations;
    ozz::animation::Skeleton mSkeleton;
    ui32 mNumAnimations;
    ui32 mRigId;
};