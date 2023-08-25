#pragma once

enum class MaterialRenderPassType : ui8 {
    Default, // Standard rendering
    Smudge, // For bushes and things
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(MaterialRenderPassType,
    pair{ MaterialRenderPassType::Default, "default"sv },
    pair{ MaterialRenderPassType::Smudge, "smudge"sv }
)
static_assert(e_count(MaterialRenderPassType) == 2, "Update yml definition");
KEG_ENUM_DECL(MaterialRenderPassType);