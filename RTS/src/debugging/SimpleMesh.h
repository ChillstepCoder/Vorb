#pragma once

enum class DebugMeshType {
    LINES,
    QUADS
};

struct SimpleMesh {
    VGVertexArray vao = 0;
    VGBuffer vbo = 0;
    DebugMeshType type;
    int lifetime = 0;
    int id = 0;
    GLsizei numVerts = 0;
};