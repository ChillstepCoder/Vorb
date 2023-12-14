#pragma once

#include "rendering/mesh/Mesh.h"
#include "rendering/model/ModelConst.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "rendering/post_process/ShadowDetail.h"

#include <ozz/animation/runtime/skeleton.h>


constexpr int MAX_MODEL_MESH_COUNT = e_count(MaterialRenderPassType);

class RigDef;
class AnimMachineDef;


// Contains gpu buffers one or more models and their LODs, to improve batching performance
// Currently all models in a batch must share a skeleton (or have no skeleton)
// TODO: USE THIS
struct ModelBatch {
    MeshGpuData mMeshData;
    std::unique_ptr<MeshSkeletonData> mSkeletonData;
};

struct ModelDrawInfo {
    bool isValid() const { return mModelBatch != nullptr; }

    ModelBatch* mModelBatch = nullptr;
    GLuint mBaseVertex = 0;
    f32 mBoundingSphereRadius = 10.0f;
};



// Modeldef contains all information about a 3D model including its location
// in a ModelBatch
class ModelDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(ModelDef);

    VORB_NON_COPYABLE_BUT_MOVABLE(ModelDef);

    bool isSkeletalModel() const { return mRig != nullptr; }
    ui32 getNumMeshes() const { return mNumMeshes; }
    const SkeletalMesh& getSkeletalMesh(ui32 meshIndex) const { assert(isSkeletalModel()); return dynamic_cast<const SkeletalMesh&>(*mMeshes[meshIndex]); }
    Mesh& getMesh(ui32 meshIndex) { return *mMeshes[meshIndex]; }
    const Mesh& getMesh(ui32 meshIndex) const { return *mMeshes[meshIndex]; }
    void addMesh(std::unique_ptr<Mesh>&& mesh);

    // TODO: AssetHandle
    const RigDef* mRig = nullptr;
    const AnimMachineDef* mAnimMachine = nullptr;
    // TODO: single unique_ptr? <mesh[]>
    std::unique_ptr<Mesh> mMeshes[MAX_MODEL_MESH_COUNT];
    ui32 mNumMeshes = 0;
    SoftAssetReference mModelName = AssetType::Model;
    SoftAssetReference mRigName = AssetType::Rig;
    SoftAssetReference mMachineName = AssetType::AnimMachine;
    f32 mScale = 1.0f;
    f32 mLodDistance0 = 65.f;
    f32 mLodDistance1 = 125.f;
    f32 mLodDistance2 = 500.f;
    f32 mLodDistance3 = 1000.f;
    f32 mBoundingSphereRadius = 10.0f;
    ShadowModelDetail mShadowDetail = ShadowModelDetail::High;
    bool mForceNormalsUp = false;
    std::vector<ModelSubmeshData> mSubmeshesData;
    //ModelDrawInfo mDrawInfo; // TODO: USE
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelDef,
    make_field(o.mModelName, "model"sv),
    make_field(o.mRigName, "rig"sv),
    make_field(o.mMachineName, "machine"sv),
    make_field(o.mScale, "scale"sv),
    make_field(o.mLodDistance0, "lod_dst_0"sv),
    make_field(o.mLodDistance1, "lod_dst_1"sv),
    make_field(o.mLodDistance2, "lod_dst_2"sv),
    make_field(o.mLodDistance3, "lod_dst_3"sv),
    make_field(o.mBoundingSphereRadius, "bound_sphere"sv),
    make_field(o.mShadowDetail, "shadow_detail"sv),
    make_field(o.mForceNormalsUp, "force_normals_up"sv),
    make_field(o.mSubmeshesData, "submesh_data"sv)
);