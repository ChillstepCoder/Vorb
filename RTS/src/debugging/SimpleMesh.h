#pragma once

enum class DebugMeshType {
    LINES,
    QUADS
};

struct SimpleMesh {
    VGBuffer vbo = 0;
    DebugMeshType type;
    int lifetime = 0;
    int id = 0;
    GLsizei numVerts = 0;
};