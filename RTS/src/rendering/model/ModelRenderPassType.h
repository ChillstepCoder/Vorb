#pragma once

enum class ModelRenderPassType : ui8 {
    Default, // Standard rendering
    Smudge, // For bushes and things
    COUNT
};
KEG_ENUM_DECL(ModelRenderPassType);