#pragma once

#include "rendering/mesh/Mesh.h"
#include "rendering/model/ModelConst.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "rendering/post_process/ShadowLodDetail.h"

#include <ozz/animation/runtime/skeleton.h>

constexpr int MAX_MODEL_MESH_COUNT = e_count(MaterialRenderPassType);

struct RigDef;
struct AnimMachineDef;

struct ModelDefFileData {
    nString mModelName;
    nString mRigName;
    nString mMachineName;
    f32 mScale = 1.0f;
    ShadowLodDetail mShadowDetail = ShadowLodDetail::High;
    bool mForceNormalsUp = false;
};
KEG_TYPE_DECL(ModelDefFileData);

// Contains gpu buffers one or more models and their LODs, to improve batching performance
// Currently all models in a batch must share a skeleton (or have no skeleton)
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
class ModelDef {
public:
    ModelDef() = default;
    ~ModelDef() = default;

    VORB_NON_COPYABLE_BUT_MOVABLE(ModelDef);

    bool isSkeletalModel() const { return mRig != nullptr; }
    ui32 getNumMeshes() const { return mNumMeshes; }
    const SkeletalMesh& getSkeletalMesh(ui32 meshIndex) const { assert(isSkeletalModel()); return dynamic_cast<const SkeletalMesh&>(*mMeshes[meshIndex]); }
    const Mesh& getMesh(ui32 meshIndex) const { assert(isSkeletalModel()); return *mMeshes[meshIndex]; }
    void addMesh(std::unique_ptr<Mesh>&& mesh);

    const RigDef* mRig = nullptr;
    const AnimMachineDef* mAnimMachine = nullptr;
    std::unique_ptr<Mesh> mMeshes[MAX_MODEL_MESH_COUNT];
    ui32 mNumMeshes = 0;
    ModelID mModelId;
    ShadowLodDetail mShadowDetail = ShadowLodDetail::High;
    const char* mName = nullptr;
    //ModelDrawInfo mDrawInfo; // TODO: USE
};
