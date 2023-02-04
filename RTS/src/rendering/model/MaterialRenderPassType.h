#pragma once

enum class MaterialRenderPassType : ui8 {
    Default, // Standard rendering
    Smudge, // For bushes and things
    COUNT
};
KEG_ENUM_DECL(MaterialRenderPassType);