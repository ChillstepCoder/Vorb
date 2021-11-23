#pragma once

enum class ShadowState : ui8 {
    NONE,
    THIN, // Thin objects like trees
    LEFT, // Top left vertex on thick objects
    RIGHT //  Top right vertex on thick objects
};

struct TileVertex {
public:
    TileVertex() {};
    TileVertex(const f32v3& pos, const f32v2& uvs, const color4& color, ui16 atlasPage) :
        pos(pos), uvs(uvs), color(color), atlasPage(atlasPage) {
    }

    f32v3 pos;
    f32v2 uvs;
    color4 color;
    ui16 atlasPage;
    i8v3 normal;
    i8v2 tangent; // for normal mapping
    ui8 windInfluence = 0;
    // TODO: Roughness :/
};

// Need power of 2 alignment
static_assert(sizeof(TileVertex) == 32, "Power of 2 byte alignment needed");

// https://gamedev.net/forums/topic/663329-particles-batching-vs-instancing/5196688/
constexpr float BILLBOARD_VERTEX_XZOFFSET_COMPRESSION_RATIO = 100.0f;
struct BillboardVertex {
public:
    BillboardVertex() {};
    BillboardVertex(const f32v3& pos, const i16v2& xzOffset, const f32v2& uvs, const color4& color, ui16 atlasPage) :
        rootPos(pos), xzOffset(xzOffset), uvs(uvs), color(color), atlasPage(atlasPage) {
    }

    f32v3 rootPos;
    i16v2 xzOffset;
    f32v2 uvs; //TODO: ui16v2?
    color4 color;
    ui16 atlasPage;
    ui8 windInfluence = 0;
    ui8 roughness;
};
// Need power of 2 alignment
static_assert(sizeof(BillboardVertex) == 32, "Power of 2 byte alignment needed");

struct TriangleVertex {
public:
    TriangleVertex() {};
    TriangleVertex(const f32v3& pos, const f32v3& normal, const f32v2& uvs, const color4& color, ui16 atlasPage) :
        pos(pos), uvs(uvs), normal(normal), color(color), atlasPage(atlasPage) {
    }

    f32v3 pos;
    f32v3 normal;
    f32v2 uvs;
    f32v4 uvTiling;
    color4 color;
    ui16 atlasPage;
    ui8 roughness;
    ui8 padding[7];
};
// Need power of 2 alignment
static_assert(sizeof(TriangleVertex) == 64, "Power of 2 byte alignment needed");