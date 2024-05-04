#pragma once

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>

#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>

struct RigDefFileData {
    StrToken mSkeletonFileName;
    nString mUpperRootJointName;
};
SERIALIZABLE_SIMPLE(RigDefFileData,
    make_field(o.mSkeletonFileName, "skeleton"sv),
    make_field(o.mUpperRootJointName, "upper_root"sv)
);

typedef const ozz::animation::Animation* ConstOzzAnimationPtr;

constexpr ui32 MAX_JOINTS_IN_RIG = 128;

struct JointEditorNode;

struct JointEditorNode {
    std::vector<ui32> childNodes;
    const char* name;
};

class RigDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(RigDef, AssetType::Rig);

    // Render bone heirarchy using tree nodes
    void imguiRenderSkeletonHierarchy() const;

    std::vector<SoftAssetReference> mAnimationDefs; // populated by AnimationRepository
    ozz::animation::Skeleton mSkeleton;
    ozz::vector<ozz::math::SimdFloat4> mUpperBodyJointWeights;
    ozz::vector<ozz::math::SimdFloat4> mLowerBodyJointWeights;
    ui32 mRigId;

    // Cached editor view data used by imguiRenderSkeletonHierarchy
private:
    mutable std::unique_ptr<JointEditorNode[]> mJointEditorNodes;
    mutable ui32 mEditorRootNode = 0;
};