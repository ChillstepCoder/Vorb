#pragma once

enum class MaterialRenderPassType : ui8 {
    Default, // Standard rendering
    Smudge, // For bushes and things
    Water,
    COUNT
};
SERIALIZABLE_ENUM_DECL(MaterialRenderPassType);