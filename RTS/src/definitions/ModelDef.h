#pragma once

#include "rendering/model/Model3D.h"

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
    Model3D mModel;
    ui32 mModelId;
};

