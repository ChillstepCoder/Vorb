#pragma once

#include <span>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>

struct RigDef {
    Array<nString> mAnimationFileNames;
    nString mSkeletonFileName;
    std::unique_ptr<ozz::animation::Animation[]> mAnimations;
    ozz::animation::Skeleton mSkeleton;
    ui32 mNumAnimations;
    ui32 mRigId;
};
KEG_TYPE_DECL(RigDef);

