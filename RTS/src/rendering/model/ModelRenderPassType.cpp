#include "stdafx.h"
#include "ModelRenderPassType.h"

KEG_ENUM_DEF(ModelRenderPassType, ModelRenderPassType, kt) {
    kt.addValue("default", ModelRenderPassType::Default);
    kt.addValue("smudge", ModelRenderPassType::Smudge);
}
static_assert(e_cast(ModelRenderPassType::COUNT) == 2, "Update keg definition");