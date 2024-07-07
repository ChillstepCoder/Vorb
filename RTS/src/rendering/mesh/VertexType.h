#pragma once

constexpr int MAX_BONES_PER_VERTEX = 4;

enum class VertexType {
    INVALID,
    TERRAIN,
    WATER,
    STANDARD_MODEL,
    SKINNED_MODEL,
    COUNT
};
const char* const VertexTypeNames[e_cast(VertexType::COUNT)] = {
    "Invalid", // INVALID
    "Terrain", // TERRAIN
    "Water", // WATER
    "Standard Model", // STANDARD_MODEL
    "Skinned Model", // SKINNED_MODEL
};
inline const char* getVertexTypeName(VertexType type) {
    return VertexTypeNames[e_cast(type)];
}
static_assert(e_cast(VertexType::COUNT) == 5, "Update display strings");