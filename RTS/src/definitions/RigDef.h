#pragma once

#include <span>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>

struct RigDefFileData {
    Array<nString> mAnimationFileNames;
    nString mSkeletonFileName;
};
KEG_TYPE_DECL(RigDefFileData);

struct RigDef {
    std::map<nString, ui32> mNameToAnimationIndex;
    std::unique_ptr<ozz::animation::Animation[]> mAnimations;
    ozz::animation::Skeleton mSkeleton;
    ui32 mNumAnimations;
    ui32 mRigId;
};