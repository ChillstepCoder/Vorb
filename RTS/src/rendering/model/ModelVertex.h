#pragma once

constexpr int MAX_BONES_PER_VERTEX = 4;


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