#include "stdafx.h"
#include "MaterialRenderPassType.h"

KEG_ENUM_DEF(MaterialRenderPassType, MaterialRenderPassType, kt) {
    kt.addValue("default", MaterialRenderPassType::Default);
    kt.addValue("smudge", MaterialRenderPassType::Smudge);
}

