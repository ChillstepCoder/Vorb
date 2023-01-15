#pragma once

enum class ModelRenderPass : ui8 {
    Default, // Standard rendering
    Smudge, // For bushes and things
    COUNT
};
KEG_ENUM_DECL(ModelRenderPass);