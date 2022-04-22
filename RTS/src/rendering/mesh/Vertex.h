#pragma once

struct alignas(32) CompressedVertex {
    f32v3 pos;
    f32v2 uvs;
    ui16 textureId;
    i8v3 normal; // https://stackoverflow.com/questions/5255806/how-to-calculate-tangent-and-binormal
    i8v2 tangent;
    color4 color;
    ui8 roughness;
};
static_assert(sizeof(CompressedVertex) == 32, "32 byte alignment needed");