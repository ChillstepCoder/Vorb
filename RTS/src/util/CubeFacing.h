#pragma once

// QUAD FACINGS
// TODO: Do we need bottom?
enum class CubeFacing {
    LEFT,
    FRONT,
    RIGHT,
    BACK,
    TOP,
    BOTTOM,
    COUNT
};

const i32v2 CUBE_FACING_AXIS[e_cast(CubeFacing::COUNT)] = {
    i32v2(AXIS_Y, AXIS_Z), // LEFT
    i32v2(AXIS_X, AXIS_Z),  // FRONT
    i32v2(AXIS_Y, AXIS_Z),  // RIGHT
    i32v2(AXIS_X, AXIS_Z), // BACK
    i32v2(AXIS_X, AXIS_Y),  // TOP
    i32v2(AXIS_X, AXIS_Y)   // BOTTOM
};

const i32v3 CUBE_FACING_NORMALS[e_cast(CubeFacing::COUNT)] = {
    i32v3(-1, 0, 0), // LEFT
    i32v3(0, -1, 0), // FRONT
    i32v3(1, 0, 0), // RIGHT
    i32v3(0, 1, 0), // BACK
    i32v3(0, 0, 1),  // TOP
    i32v3(0, 0, -1)  // BOTTOM
};
const f32v3 CUBE_FACING_NORMALSF[e_cast(CubeFacing::COUNT)] = {
    f32v3(-1, 0, 0), // LEFT
    f32v3(0, -1, 0), // FRONT
    f32v3(1, 0, 0), // RIGHT
    f32v3(0, 1, 0), // BACK
    f32v3(0, 0, 1),  // TOP
    f32v3(0, 0, -1)  // BOTTOM
};

const i32v2 CUBE_FACING_TANGENTS[e_cast(CubeFacing::COUNT)] = {
    i32v2( 0, -1),   // LEFT
    i32v2( 1,  0), // FRONT
    i32v2( 0,  1),  // RIGHT
    i32v2(-1,  0),   // BACK
    i32v2( 1,  0),   // TOP
    i32v2( 1,  0)   // BOTTOM
};
const i32v3 CUBE_FACING_TANGENTS_3D[e_cast(CubeFacing::COUNT)] = {
    i32v3(CUBE_FACING_TANGENTS[0].x, CUBE_FACING_TANGENTS[0].y, 0),   // LEFT
    i32v3(CUBE_FACING_TANGENTS[1].x, CUBE_FACING_TANGENTS[1].y, 0), // FRONT
    i32v3(CUBE_FACING_TANGENTS[2].x, CUBE_FACING_TANGENTS[2].y, 0),  // RIGHT
    i32v3(CUBE_FACING_TANGENTS[3].x, CUBE_FACING_TANGENTS[3].y, 0),   // BACK
    i32v3(CUBE_FACING_TANGENTS[4].x, CUBE_FACING_TANGENTS[4].y, 0),   // TOP
    i32v3(CUBE_FACING_TANGENTS[5].x, CUBE_FACING_TANGENTS[5].y, 0)   // BOTTOM
};

const f32v3 CUBE_FACING_TANGENTSF[e_cast(CubeFacing::COUNT)] = {
    f32v3(CUBE_FACING_TANGENTS_3D[0]),   // LEFT
    f32v3(CUBE_FACING_TANGENTS_3D[1]), // FRONT
    f32v3(CUBE_FACING_TANGENTS_3D[2]),  // RIGHT
    f32v3(CUBE_FACING_TANGENTS_3D[3]),   // BACK
    f32v3(CUBE_FACING_TANGENTS_3D[4]),   // TOP
    f32v3(CUBE_FACING_TANGENTS_3D[5])   // BOTTOM
};

const f32v3 CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::COUNT)] = {
    f32v3(0, 0, 0), // LEFT
    f32v3(0, 0, 0), // FRONT
    f32v3(1.0f, 0, 0), // RIGHT
    f32v3(0, 1.0f, 0), // BACK
    f32v3(0, 0, 1.0f),  // TOP
    f32v3(0, 0, 0) // BOTTOM
};

// Radius 1 origin 0
const f32v3 CUBE_POSITIONS[e_cast(CubeFacing::COUNT)][4] = {
    { f32v3(-1, 1, -1), f32v3(-1, -1, -1), f32v3(-1, -1, 1), f32v3(-1, 1, 1) }, // LEFT
    { f32v3(-1, -1, -1), f32v3(1, -1, -1), f32v3(1, -1, 1), f32v3(-1, -1, 1) }, // FRONT
    { f32v3(1, -1, -1), f32v3(1, 1, -1), f32v3(1, 1, 1), f32v3(1, -1, 1) }, // RIGHT
    { f32v3(1, 1, -1), f32v3(-1, 1, -1), f32v3(-1, 1, 1), f32v3(1, 1, 1) }, // BACK
    { f32v3(-1, -1, 1), f32v3(1, -1, 1), f32v3(1, 1, 1), f32v3(-1, 1, 1) }, // TOP
    { f32v3(-1, 1, -1), f32v3(1, 1, -1), f32v3(1, -1, -1), f32v3(-1, -1, -1) }, // BOTTOM
};