#pragma once

using LivingBiomeID = ui16;
constexpr LivingBiomeID INVALID_LIVING_BIOME_ID = std::numeric_limits<LivingBiomeID>::max();

// .biome file names should match exactly
// Corrupt biomes should be _B and _C and always come
// directly after the base biome in alphabetical order
enum class BiomeUniqueID : ui8 {
    Ocean = 0,
    Plains = 1,
    Plains_B = 2,
    Plains_C = 3,
    Mountains = 4,
    Mountains_B = 5,
    Mountains_C = 6,
    Forest = 7,
    Forest_B = 8,
    Forest_C = 9,
    Hotsprings = 10,
    Hotsprings_B = 11,
    Hotsprings_C = 12,
    COUNT,
    INVALID = UINT8_MAX
};
SERIALIZABLE_ENUM_DECL(BiomeUniqueID);

constexpr ui8 INVALID_BIOME_ID = e_cast(BiomeUniqueID::INVALID);