#pragma once

constexpr int MAX_BONES_PER_VERTEX = 4;

// For packing signed normals in to a single 32 byte value
// https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
inline uint32_t Pack_INT_2_10_10_10_REV(float x, float y, float z, float w)
{
    const uint32_t xs = x < 0;
    const uint32_t ys = y < 0;
    const uint32_t zs = z < 0;
    const uint32_t ws = w < 0;
    uint32_t vi =
        ws << 31 | ((uint32_t)(w + (ws << 1)) & 1) << 30 |
        zs << 29 | ((uint32_t)(z * 511 + (zs << 9)) & 511) << 20 |
        ys << 19 | ((uint32_t)(y * 511 + (ys << 9)) & 511) << 10 |
        xs << 9 | ((uint32_t)(x * 511 + (xs << 9)) & 511);
    return vi;
}


// https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
struct alignas(32) StaticModelVertex {
public:
    StaticModelVertex() {};

    f32v3 pos;
    ui32 normalPacked;
    ui32 tangentPacked;
    ui16v2 uvsPacked;
    color4 color;
    ui8 textureIndex;
};
static_assert(sizeof(StaticModelVertex) == 32, "32 byte alignment needed");

// TODO: Reduce to 64 https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
struct alignas(32) SkinnedModelVertex {
public:
    SkinnedModelVertex() {};

    f32v3 pos;
    f32v3 normal;
    f32v3 tangent; // TODO: This can be i8v3 which will compress to 64 bits per vertex
    f32v2 uvs;
    color4 color;
    // TODO: ui16 weights?
    f32 boneWeights[MAX_BONES_PER_VERTEX] = {}; // 0 Weight default 
    ui8 boneIDs[MAX_BONES_PER_VERTEX] = {}; //
};
static_assert(sizeof(SkinnedModelVertex) == 96, "32 byte alignment needed");