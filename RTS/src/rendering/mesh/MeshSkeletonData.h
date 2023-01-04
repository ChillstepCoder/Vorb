#pragma once

#include <ozz/base/maths/simd_math.h>

struct RawMeshSkeletonData {
    std::vector<ui8> mJointRemaps; // Excludes bones which are not used, maps back to the original bone index for model transform
    // TODO: These inverse bind poses are duplicated on every mesh, instead they could live on the skeleton itself?
    std::vector<ozz::math::Float4x4> mInverseBindPoses; // Excludes bones which are not used
    ui8 mNumJoints = 0; // Excludes bones which are not used
};

// This is not a skeleton, but rather a reference to a subset of bones in a skeleton using indices, as well as
// the inverse bind poses
struct MeshSkeletonData {
    std::unique_ptr<ui8[]> mJointRemaps; // Excludes bones which are not used, maps back to the original bone index for model transform
    // TODO: These inverse bind poses are duplicated on every mesh, instead they could live on the skeleton itself?
    std::unique_ptr<ozz::math::Float4x4[]> mInverseBindPoses; // Excludes bones which are not used
    ui8 mNumJoints = 0; // Excludes bones which are not used
};