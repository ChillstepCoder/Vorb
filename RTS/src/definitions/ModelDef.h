#pragma once

#include "rendering/model/Model3D.h"

#include <ozz/animation/runtime/skeleton.h>

typedef ui32 ModelID;

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
    SkinnedModel3D mModel;
    ModelID mModelId;
};

