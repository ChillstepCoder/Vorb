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

// TODO: Allow -1->2?
inline ui16v2 PackUVs(const f32v2& uvs) {
    return ui16v2(uvs.x * UINT16_MAX, uvs.y * UINT16_MAX);
}

// https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
struct alignas(16) StaticModelVertex {
    f32v3 pos;
    ui32 normalPacked;
    ui32 tangentPacked;
    ui16v2 uvsPacked;
    color4 color;
    ui16 materialId;
    ui8 windInfluence;

    // TODO: Pre-packed normals/tangent
    void build(const f32v3& pos, const f32v3& normal, const f32v3& tangent, const f32v2& uvs, const color4& color, MaterialID materialId, ui8 windInfluence);

    static VertexType bindVertexAttribs(VGBuffer vao);
    static VertexType vertexType() { return VertexType::STATIC_MODEL; }
};
static_assert(sizeof(StaticModelVertex) == 32, "16 byte alignment needed");

struct alignas(16) TerrainVertex {
    f32v3 pos;
    f32v3 normal;

    static VertexType bindVertexAttribs(VGBuffer vao);
    static VertexType vertexType() { return VertexType::TERRAIN; }
};
static_assert(sizeof(TerrainVertex) == 32, "16 byte alignment needed");

struct alignas(16) WaterVertex {
    f32v3 pos;
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
        StaticModelVertex mStaticModel; // VertexType::STATIC_MODEL
    };
};
static_assert(sizeof(Vertex32) == 32, "16 byte alignment needed");

// https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
struct alignas(16) SkinnedModelVertex {
public:
    SkinnedModelVertex() {};

    f32v3 pos;
    ui32 normalPacked; // TODO: Test uncompressed since we have lots of padding room
    ui32 tangentPacked;
    ui16v2 uvsPacked;
    color4 color;
    ui16 materialId;
    // TODO: ui16 weights?
    f32 boneWeights[MAX_BONES_PER_VERTEX] = {}; // 0 Weight default 
    ui8 boneIDs[MAX_BONES_PER_VERTEX] = {}; //


    static VertexType bindVertexAttribs(VGBuffer vao);
    static VertexType vertexType() { return VertexType::SKINNED_MODEL; }
};
static_assert(sizeof(SkinnedModelVertex) == 64, "16 byte alignment needed");

extern constexpr size_t getVertexSize(VertexType type);