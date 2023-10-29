#pragma once

#include "rendering/mesh/Mesh.h"
#include "rendering/model/ModelConst.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "rendering/post_process/ShadowLodDetail.h"

#include <ozz/animation/runtime/skeleton.h>

constexpr int MAX_MODEL_MESH_COUNT = e_count(MaterialRenderPassType);

struct RigDef;
struct AnimMachineDef;


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
    const Mesh& getMesh(ui32 meshIndex) const { return *mMeshes[meshIndex]; }
    void addMesh(std::unique_ptr<Mesh>&& mesh);

    // TODO: AssetHandle
    const RigDef* mRig = nullptr;
    const AnimMachineDef* mAnimMachine = nullptr;
    // TODO: single unique_ptr? <mesh[]>
    std::unique_ptr<Mesh> mMeshes[MAX_MODEL_MESH_COUNT];
    ui32 mNumMeshes = 0;
    StrToken mModelName;
    StrToken mRigName;
    StrToken mMachineName;
    f32 mScale = 1.0f;
    ShadowLodDetail mShadowDetail = ShadowLodDetail::High;
    bool mForceNormalsUp = false;
    //ModelDrawInfo mDrawInfo; // TODO: USE
};
SERIALIZABLE_SIMPLE(ModelDef,
    make_field(o.mModelName, "model"sv),
    make_field(o.mRigName, "rig"sv),
    make_field(o.mMachineName, "machine"sv),
    make_field(o.mScale, "scale"sv),
    make_field(o.mShadowDetail, "shadow_detail"sv),
    make_field(o.mForceNormalsUp, "force_normals_up"sv)
);