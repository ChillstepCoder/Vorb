#include "stdafx.h"
#include "MaterialRenderPassType.h"

SERIALIZABLE_ENUM_SAME_NAME(MaterialRenderPassType,
    pair{ MaterialRenderPassType::Default, "default"sv },
    pair{ MaterialRenderPassType::Smudge, "smudge"sv },
    pair{ MaterialRenderPassType::Water, "water"sv }
)
static_assert(e_count(MaterialRenderPassType) == 3, "Update yml definition");
