#pragma once

#include "rendering/model/Model3D.h"

#include <ozz/animation/runtime/skeleton.h>

struct RigDef;

struct ModelDef {
    const RigDef* mRig;
    nString mModelName;
    nString mRigName;
    Model3D mModel;
    ui32 mModelId;
};
KEG_TYPE_DECL(ModelDef);

