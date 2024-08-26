#pragma once

#include "rendering/mesh/Mesh.h"
#include "rendering/model/ModelConst.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "rendering/post_process/ShadowDetail.h"

#include "physics/CollisionShapes.h"
#include "physics/ModelColliderShape.h"

#include <ozz/animation/runtime/skeleton.h>

// Smaller than a 16 byte span
struct ModelBatchSubmeshDrawDataSpanKey {
    SubmeshID startIndex;
    int count;
};

constexpr ui8 MAX_DRAW_COMMANDS_PER_MODEL = 12;

class RigDef;
class AnimMachineDef;
struct ModelBatchSubmeshDrawData;

struct ModelCollider {
    std::vector<ModelColliderShape> mSubShapes;
    // Used for determining entity rotation
    glm::quat mBaseOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat mInverseBaseOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    f32v3 mBaseOffset = f32v3(0.0f);
    bool mHasBaseOrientation = false;
    bool mHasBaseOffset = false;
};
SERIALIZABLE_SIMPLE(ModelCollider,
    make_field(o.mSubShapes, "shapes"sv)
);

// Modeldef contains all information about a 3D model
class ModelDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(ModelDef, AssetType::Model);

    VORB_NON_COPYABLE_BUT_MOVABLE(ModelDef);

    bool isSkeletalModel() const { return mRig != nullptr; }
    ui32 getNumMeshes() const { return mSubmeshData.size(); }

    // TODO: AssetHandle
    const RigDef* mRig = nullptr;
    const AnimMachineDef* mAnimMachine = nullptr;
    std::vector<ModelSubmeshData> mSubmeshData;
    std::vector<MeshSkeletonData> mSubmeshSkeletonData;
    std::vector<MeshCpuData> mSubmeshCpuData;
    ui32 mTotalSubmeshJointTransformsNeeded = 0;
    StrToken mModelFileName;
    RigAssetRef mRigRef;
    AnimMachineAssetRef mMachineRef;
    f32 mScale = 1.0f;
    f32 mLodDistance0 = 45.f;
    f32 mLodDistance1 = 90.f;
    f32 mLodDistance2 = 180.f;
    f32 mLodDistance3 = 360.f;
    f32 mBoundingSphereRadius = 10.0f;
    ShadowModelDetail mShadowDetail = ShadowModelDetail::High;
    f32AABB3 mAABB; // Calculated from mesh data
    bool mForceNormalsUp = false;
    std::vector<f32v3> mDamageZoneSpline = { f32v3(0.0f), f32v3(0.0f, 0.0f, 8.0f) }; // Damage zones align to this spline
    ModelCollider mColliderData;
    CollisionShapeID mCollisionShapeID = INVALID_COLLISION_SHAPE_ID;
    f32 mBaseOptimizeErrorThresold = 0.0003f;
    MaterialID mBillboardMaterialID = INVALID_MATERIAL_ID;

    // Variants
    std::vector<ModelVariantData> mVariants;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelDef,
    make_field(o.mModelFileName, "model"sv),
    make_field(o.mRigRef, "rig"sv),
    make_field(o.mMachineRef, "machine"sv),
    make_field(o.mSubmeshData, "submesh_data"sv),
    make_field(o.mScale, "scale"sv),
    make_field(o.mLodDistance0, "lod_dst_0"sv),
    make_field(o.mLodDistance1, "lod_dst_1"sv),
    make_field(o.mLodDistance2, "lod_dst_2"sv),
    make_field(o.mLodDistance3, "lod_dst_3"sv),
    make_field(o.mBoundingSphereRadius, "bound_sphere"sv),
    make_field(o.mShadowDetail, "shadow_detail"sv),
    make_field(o.mForceNormalsUp, "force_normals_up"sv),
    make_field(o.mDamageZoneSpline, "dmg_spline"sv),
    make_field(o.mVariants, "variants"sv),
    make_field(o.mColliderData, "collider"sv),
    make_field(o.mBaseOptimizeErrorThresold, "opt_thresh"sv)
);

struct ModelDefRef {
    AssetHandlePtr<ModelDef> handle;
    int refCount = 0;
};