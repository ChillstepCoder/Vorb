#pragma once

enum class ShadowState : ui8 {
    NONE,
    STANDARD, // Thin objects like trees
    LEFT, // Top left vertex on thick objects
    RIGHT //  Top right vertex on thick objects
};

struct TileVertex {
public:
    TileVertex() {};
    TileVertex(const f32v3& pos, const f32v2& uvs, const color4& color, ui16 atlasPage) :
        pos(pos), uvs(uvs), color(color), atlasPage(atlasPage) {
    }

    f32v3 pos; // TOOD: ui16v3?
    f32v2 uvs; //TODO: ui16v2?
    color4 color;
    ui16 atlasPage;
    ui8 height; // Defines world height to vertex
    ui8 shadowState;
    ui8 padding[4];
};

// Need power of 2 alignment
static_assert(sizeof(TileVertex) == 32, "Power of 2 byte alignment needed");
