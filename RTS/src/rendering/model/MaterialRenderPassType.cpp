#include "stdafx.h"
#include "MaterialRenderPassType.h"

#include "serialization/YmlSerializer.h"

KEG_ENUM_DEF(MaterialRenderPassType, MaterialRenderPassType, kt) {
    kt.addValue("default", MaterialRenderPassType::Default);
    kt.addValue("smudge", MaterialRenderPassType::Smudge);
}
SERIALIZABLE_ENUM(MaterialRenderPassType,
    pair{Default, "default"sv},
    pair{Smudge, "smudge"sv}
)
static_assert(e_cast(MaterialRenderPassType::COUNT) == 2, "Update yml definition");
