#pragma once

#include "rendering/model/Model3D.h"

struct ModelDef {
    nString mModelName;
    nString mSkeletonName;
    Model3D mModel;
};
KEG_TYPE_DECL(ModelDef);

