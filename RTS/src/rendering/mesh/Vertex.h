#pragma once

#include "rendering/model/ModelVertex.h"

enum class VertexVariantType {
    STANDARD,
    TERRAIN,
    WATER,
    MODEL
};

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
        zs << 29 | ((uint32_t)(z * 0x1ff + (zs << 9)) & 0x1ff) << 20 |
        ys << 19 | ((uint32_t)(y * 0x1ff + (ys << 9)) & 0x1ff) << 10 |
        xs << 9 | ((uint32_t)(x * 0x1ff + (xs << 9)) & 0x1ff);
    return vi;
}


// https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
struct alignas(32) StaticModelVertex {
    f32v3 pos;
    ui32 normalPacked;
    ui32 tangentPacked;
    ui16v2 uvsPacked;
    color4 color;
    ui8 textureIndex;

    static void bindVertexAttribs(VGBuffer vao);
};
static_assert(sizeof(StaticModelVertex) == 32, "32 byte alignment needed");

struct alignas(32) StandardVertex {
    f32v3 pos;
    f32v2 uvs;
    ui8 textureIndex;
    i8v3 normal; // https://stackoverflow.com/questions/5255806/how-to-calculate-tangent-and-binormal
    i8v2 tangent;
    color4 color;
    ui8 roughness;

    static void bindVertexAttribs(VGBuffer vao);
};

struct alignas(32) TerrainVertex {
    f32v3 pos;
    f32v3 normal;

    static void bindVertexAttribs(VGBuffer vao);
};

struct alignas(32) WaterVertex {
    f32v3 pos;
    f32 depth;

    static void bindVertexAttribs(VGBuffer vao);
};

// Vertex variant
struct alignas(32) Vertex32 {
    Vertex32() {};

    union { 
        StandardVertex mStandard; // VertexVariantType::STANDARD
        TerrainVertex mTerrain; // VertexVariantType::TERRAIN
        WaterVertex mWater; // VertexVariantType::WATER
        StaticModelVertex mStaticModel; // VertexVariantType::MODEL
    };
};

static_assert(sizeof(Vertex32) == 32, "32 byte alignment needed");

struct alignas(32) Vertex96 {
    Vertex96() {};

    union {
        SkinnedModelVertex mSkinnedModelVertex;
    };
};

static_assert(sizeof(Vertex96) == 96, "96 byte alignment needed");