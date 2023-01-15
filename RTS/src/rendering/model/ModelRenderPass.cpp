#include "stdafx.h"
#include "ModelRenderPass.h"

KEG_ENUM_DEF(ModelRenderPass, ModelRenderPass, kt) {
    kt.addValue("default", ModelRenderPass::Default);
    kt.addValue("smudge", ModelRenderPass::Smudge);
}
static_assert(e_cast(ModelRenderPass::COUNT) == 2, "Update keg definition");