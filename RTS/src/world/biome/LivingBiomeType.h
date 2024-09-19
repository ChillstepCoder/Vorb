#pragma once

#include "tile/MutationDef.h"

enum class LivingBiomeType : ui8 {
    Banshira,
    Chernobog,
    COUNT
};

inline constexpr std::array<MutationType, e_count(LivingBiomeType)> LIVING_BIOME_MUTATION_TYPES = {
    MutationType::BCorrupt,
    MutationType::CCorrupt,
};