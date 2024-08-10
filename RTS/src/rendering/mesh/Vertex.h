#pragma once

#include "rendering/mesh/VertexType.h"

// For packing signed normals in to a single 32 byte value
// https://www.khronos.org/opengl/wiki/Normalized_Integer#Alternate_mapping
// https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
inline constexpr uint32_t Pack_INT_2_10_10_10_REV(float x, float y, float z, float w)
{
    const uint32_t xs = x < 0;
    const uint32_t ys = y < 0;
    const uint32_t zs = z < 0;
    const uint32_t ws = w < 0;
    uint32_t vi =
        ws << 31 | ((uint32_t)(w + (ws << 1)) & 1) << 30 |
        zs << 29 | ((uint32_t)(z * 0x1ff + (zs << 9)) & 0x1ff) << 20 |
        ys << 19 | ((uint32_t)(y * 0x1ff + (ys << 9)) & 0x1ff) << 10 |
        xs << 9 | ((uint32_t)(x * 0x1ff + (xs << 9)) & 0x1ff);
    return vi;
}
inline constexpr uint32_t Pack_INT_2_10_10_10_REV(const f32v3& vec3) {
    return Pack_INT_2_10_10_10_REV(vec3.x, vec3.y, vec3.z, 0.0f);
}

constexpr f32 UV_MAX_RANGE = 8.0f; //[-8, 8]
inline i16v2 PackUVs(const f32v2& uvs) {
    return i16v2(std::nearbyint((uvs.x / UV_MAX_RANGE) * INT16_MAX), std::nearbyint((uvs.y / UV_MAX_RANGE) * INT16_MAX));
}

// https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
struct alignas(16) StandardModelVertex {
    f32v3 pos; // TODO: Can we get away with 16 bit local positions?
    ui32 normalPacked;
    ui32 tangentPacked;
    i16v2 uvsPacked;
    color4 color;
    union {
        ui16 materialSlot; // Used by models
        ui16 materialId; // Used by procedural mesh builder
    };
    ui8 windInfluence;
    ui8 damageZone; // 0-64

    // TODO: Pre-packed normals/tangent
    void build(const f32v3& pos, const f32v3& normal, const f32v3& tangent, const f32v2& uvs, const color4& color, ui16 materialIdOrSlot, ui8 windInfluence, ui8 damageZone = 0);

    static VertexType bindVertexAttribs(VGBuffer vao);
    static VertexType vertexType() { return VertexType::STANDARD_MODEL; }
};
static_assert(sizeof(StandardModelVertex) == 32, "16 byte alignment needed");

struct alignas(16) TerrainVertex {
    f32v3 pos; // TODO: We can make this just Z by storing a separate global f32v2 buffer of XY positions since every terrain patch is the same in XY
    ui32 normalPacked;

    static VertexType bindVertexAttribs(VGBuffer vao);
    static VertexType vertexType() { return VertexType::TERRAIN; }
};
static_assert(sizeof(TerrainVertex) == 16, "16 byte alignment needed");

// TODO: This could probably be only depth
struct alignas(16) WaterVertex {
    f32v3 pos; // TODO: We can make this just Z (or possibly JUST depth) by storing a separate global f32v2 buffer of XY positions since every terrain patch is the same in XY
    f32 depth;

    static VertexType bindVertexAttribs(VGBuffer vao);
    static VertexType vertexType() { return VertexType::WATER; }
};
static_assert(sizeof(WaterVertex) == 16, "16 byte alignment needed");

// Vertex variant
struct alignas(16) Vertex32 {
    Vertex32() {};

    union { 
        TerrainVertex mTerrain; // VertexType::TERRAIN
        StandardModelVertex mStaticModel; // VertexType::STATIC_MODEL
    };
};
static_assert(sizeof(Vertex32) == 32, "16 byte alignment needed");

// https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
struct SkinnedModelVertex {
public:
    SkinnedModelVertex() {};

    f32v3 pos; // TODO: Try compress to ui16?
    ui32 normalPacked;
    ui32 tangentPacked;
    i16v2 uvsPacked;
    color4 color;
    // TODO: ui16 or ui8 weights
    f32 boneWeights[MAX_BONES_PER_VERTEX] = {}; // 0 Weight default 
    ui8 boneIDs[MAX_BONES_PER_VERTEX] = {}; //
    ui16 materialSlot; // Used by models


    static VertexType bindVertexAttribs(VGBuffer vao);
    static VertexType vertexType() { return VertexType::SKINNED_MODEL; }
};
static_assert(sizeof(SkinnedModelVertex) == 52, "keep small");

extern constexpr size_t getVertexSize(VertexType type);