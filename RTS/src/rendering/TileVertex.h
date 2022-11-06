#pragma once

enum class ShadowState : ui8 {
    NONE,
    THIN, // Thin objects like trees
    LEFT, // Top left vertex on thick objects
    RIGHT //  Top right vertex on thick objects
};

struct alignas(32) TriangleVertex {
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
    i8v2 tangent;
    ui8 roughness;
};
// Need power of 2 alignment
static_assert(sizeof(TriangleVertex) == 64, "32 byte alignment needed");


