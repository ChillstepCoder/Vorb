#pragma once

enum class VertexType {
    INVALID,
    STANDARD,
    TERRAIN,
    WATER,
    STATIC_MODEL,
    SKINNED_MODEL,
    COUNT
};
const char* const VertexTypeNames[e_cast(VertexType::COUNT)] = {
    "Invalid", // INVALID
    "Standard", // STANDARD
    "Terrain", // TERRAIN
    "Water", // WATER
    "Static Model", // STATIC_MODEL
    "Skinned Model", // SKINNED_MODEL
};
inline const char* getVertexTypeName(VertexType type) {
    return VertexTypeNames[e_cast(type)];
}
static_assert(e_cast(VertexType::COUNT) == 6, "Update display strings");