#pragma once

struct ChunkVertex {
public:
    ChunkVertex() {};
    ChunkVertex(const f32v3& pos, const f32v2& uvs, const color4& color) : pos(pos), uvs(uvs), color(color) {}

    f32v3 pos; // TOOD: ui16v3?
    f32v2 uvs; //TODO: ui16v2?
    color4 color;
    ui8 atlasPage;
    ui8 padding[7];
};

// Need power of 2 alignment
static_assert(sizeof(ChunkVertex) == 32, "Power of 2 byte alignment needed");