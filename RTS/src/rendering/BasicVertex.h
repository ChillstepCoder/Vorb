#pragma once

struct BasicVertex {
public:
    BasicVertex() {};
    BasicVertex(const f32v3& pos, const f32v2& uvs, const color4& color, ui16 atlasPage) :
        pos(pos), uvs(uvs), color(color), atlasPage(atlasPage) {
    }

    f32v3 pos; // TOOD: ui16v3?
    f32v2 uvs; //TODO: ui16v2?
    color4 color;
    ui16 atlasPage;
    ui8 padding[6];
};

// Need power of 2 alignment
static_assert(sizeof(BasicVertex) == 32, "Power of 2 byte alignment needed");