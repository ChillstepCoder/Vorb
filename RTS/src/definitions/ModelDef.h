#pragma once

#include "rendering/model/Model3D.h"
#include "rendering/model/ModelConst.h"

#include <ozz/animation/runtime/skeleton.h>

struct RigDef;
struct AnimMachineDef;

struct ModelDefFileData {
    nString mModelName;
    nString mRigName;
    nString mMachineName;
};
KEG_TYPE_DECL(ModelDefFileData);

struct ModelDef {
    const RigDef* mRig = nullptr;
    const AnimMachineDef* mAnimMachine = nullptr;

    SkinnedModel3D& getSkinnedModel() { assert(mModelType == Model3DType::SKINNED); return mSkinnedModel; }
    const SkinnedModel3D& getSkinnedModel() const { assert(mModelType == Model3DType::SKINNED); return mSkinnedModel; }
    StaticModel3D& getStaticModel() { assert(mModelType == Model3DType::STATIC); return mStaticModel; }
    const StaticModel3D& getStaticModel() const { assert(mModelType == Model3DType::STATIC); return mStaticModel; }
private:
    // TODO: Fix union, rightnow the skinned model internally holds unique_ptr, that needs to be managed here in the modeldef
    //union {
        SkinnedModel3D mSkinnedModel;
        StaticModel3D mStaticModel;
    //};
public:
    Model3DType mModelType;
    ModelID mModelId;
};
