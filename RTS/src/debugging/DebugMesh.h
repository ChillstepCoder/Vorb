#pragma once

DECL_VG(class GLProgram);

#include "debugging/SimpleMesh.h"

struct DebugWireTriangle {
    DebugWireTriangle(const f32v3& pos1, const f32v3& pos2, const f32v3& pos3, const color4& colr)
        : position1(pos1)
        , position2(pos2)
        , position3(pos3)
        , color(colr) {
    }
    f32v3 position1;
    f32v3 position2;
    f32v3 position3;
    color4 color;
};

struct DebugLine {
    DebugLine(const f32v2& pos1, const f32v2& pos2, const color4& colr)
        : position1(pos1.x, pos1.y, 0.0f)
        , position2(pos2.x, pos2.y, 0.0f)
        , color(colr) {
    }
    DebugLine(const f32v3& pos1, const f32v3& pos2, const color4& colr)
        : position1(pos1)
        , position2(pos2)
        , color(colr) {
    }
    f32v3 position1;
    f32v3 position2;
    color4 color;
};

struct DebugQuad {
    DebugQuad(const f32v2& position, const f32v2& dims, const color4& colr)
        : position(position.x, position.y, 0.0f)
        , dims(dims)
        , color(colr) {
    }
    DebugQuad(const f32v3& position, const f32v2& dims, const color4& colr)
        : position(position)
        , dims(dims)
        , color(colr) {
    }
    f32v3 position;
    f32v2 dims;
    color4 color;
};

struct DebugQuad3D {
    DebugQuad3D(const f32v3 p1, const f32v3 p2, const f32v3 p3, const f32v3 p4, const color4& colr)
        : p1(p1)
        , p2(p2)
        , p3(p3)
        , p4(p4)
        , color(colr) {
    }
    f32v3 p1;
    f32v3 p2;
    f32v3 p3;
    f32v3 p4;
    color4 color;
};

struct DebugCircle {
    DebugCircle(const f32v3& position, const f32 radius, const color4& colr)
        : position(position)
        , radius(radius)
        , color(colr) {
    }
    f32v3 position;
    f32 radius;
    color4 color;
};

struct SimpleMeshVertex {
    f32v3 position;
    color4 color;
};

struct SimpleMeshCircleVertex {
    f32v3 position;
    f32v2 offset;
    f32 radius;
    color4 color;
};

extern vg::GLProgram sGlobalSimpleProgram;
extern vg::GLProgram sGlobalCircleProgram;

extern void initGlobalSimpleProgram();
extern void initGlobalCircleProgram();