#pragma once

#include "rendering/model/ModelVertex.h"

enum class VertexVariantType {
    STANDARD,
    TERRAIN,
    WATER,
    MODEL
};

struct alignas(32) StandardVertex {
    f32v3 pos;
    f32v2 uvs;
    ui8 textureIndex;
    i8v3 normal; // https://stackoverflow.com/questions/5255806/how-to-calculate-tangent-and-binormal
    i8v2 tangent;
    color4 color;
    ui8 roughness;

    static void bindVertexAttribs();
};

struct alignas(32) TerrainVertex {
    f32v3 pos;
    f32v3 normal;

    static void bindVertexAttribs();
};

struct alignas(32) WaterVertex {
    f32v3 pos;
    f32 depth;

    static void bindVertexAttribs();
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