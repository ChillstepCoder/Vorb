#pragma once

// Cartesian
enum class Cartesian : ui8 {
    SOUTH = 0, //-y 
    WEST = 1, //-x
    EAST = 2, //+x
    NORTH = 3,  //+y
    NONE = 100,
    INVALID = 101
};

enum class Cartesian8 : ui8 {
    SOUTH_WEST = 0,
    SOUTH = 1,
    SOUTH_EAST = 2,
    WEST = 3,
    EAST = 4,
    NORTH_WEST = 5,
    NORTH = 6,
    NORTH_EAST = 7,
    NONE = 100,
    INVALID = 101
};

constexpr Cartesian CARTESIAN8_TO_CARTESIAN[8] = {
    Cartesian::NONE, //SOUTH_WEST
    Cartesian::SOUTH, //SOUTH 
    Cartesian::NONE, //SOUTH_EAST
    Cartesian::WEST, //WEST
    Cartesian::EAST, //EAST
    Cartesian::NONE, //NORTH_WEST
    Cartesian::NORTH, //NORTH
    Cartesian::NONE, //NORTH_EAST
};

constexpr Cartesian8 CARTESIAN_TO_CARTESIAN8[4] = {
    Cartesian8::SOUTH,
    Cartesian8::WEST,
    Cartesian8::EAST,
    Cartesian8::NORTH,
};

const i32v2 CARTESIAN8_DIR_OFFSETS[8] = {
    {-1, -1}, //SOUTH_WEST
    { 0, -1}, //SOUTH 
    { 1, -1}, //SOUTH_EAST
    {-1,  0}, //WEST
    { 1,  0}, //EAST
    {-1,  1}, //NORTH_WEST
    { 0,  1}, //NORTH
    { 1,  1}, //NORTH_EAST
};

constexpr Cartesian8 CARTESIAN8_OPPOSITES[8] = {
    Cartesian8::NORTH_EAST, //SOUTH_WEST
    Cartesian8::NORTH,      //SOUTH 
    Cartesian8::NORTH_WEST, //SOUTH_EAST
    Cartesian8::EAST,       //WEST
    Cartesian8::WEST,       //EAST
    Cartesian8::SOUTH_EAST, //NORTH_WEST
    Cartesian8::SOUTH,      //NORTH
    Cartesian8::SOUTH_WEST, //NORTH_EAST
};

enum AXIS_2D {
    AXIS_HORIZONTAL = 0,
    AXIS_VERTICAL = 1
};

enum AXIS_3D {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2
};

constexpr int CARTESIAN_COUNT = 4;
constexpr Cartesian CARTESIAN_NEIGHBORS[CARTESIAN_COUNT][2] = {
    { Cartesian::WEST, Cartesian::EAST }, // SOUTH
    { Cartesian::NORTH, Cartesian::SOUTH }, // WEST
    { Cartesian::SOUTH, Cartesian::NORTH }, // EAST
    { Cartesian::EAST, Cartesian::WEST }, // NORTH
};
constexpr Cartesian CARTESIAN_OPPOSITES[CARTESIAN_COUNT] = {
    Cartesian::NORTH,
    Cartesian::EAST,
    Cartesian::WEST,
    Cartesian::SOUTH,
};
const i32v2 CARTESIAN_NORMALS[CARTESIAN_COUNT] = {
    i32v2(0, -1), // SOUTH
    i32v2(-1, 0), // WEST
    i32v2(1,  0), // EAST
    i32v2(0,  1), // NORTH
};
const i32v3 CARTESIAN_NORMALS_3D[CARTESIAN_COUNT] = {
    i32v3(0, -1, 0), // SOUTH
    i32v3(-1, 0, 0), // WEST
    i32v3(1,  0, 0), // EAST
    i32v3(0,  1, 0), // NORTH
};
const i32v2 CARTESIAN_EDGE_DIRS_ABS[CARTESIAN_COUNT] = {
    i32v2(1, 0), // SOUTH
    i32v2(0, 1), // WEST
    i32v2(0, 1), // EAST
    i32v2(1, 0), // NORTH
};
const i32v3 CARTESIAN_EDGE_DIRS_ABS_3D[CARTESIAN_COUNT] = {
    i32v3(1, 0, 0), // SOUTH
    i32v3(0, 1, 0), // WEST
    i32v3(0, 1, 0), // EAST
    i32v3(1, 0, 0), // NORTH
};
const i32v2 CARTESIAN_EDGE_DIRS_COUNTER_CLOCKWISE[CARTESIAN_COUNT] = {
    i32v2(1, 0), // SOUTH
    i32v2(0, -1), // WEST
    i32v2(0, 1), // EAST
    i32v2(-1, 0), // NORTH
};
const i32v2 CARTESIAN_EDGE_INDEX_OFFSET_MULTS[CARTESIAN_COUNT] = {
    i32v2(0, 0), // SOUTH
    i32v2(0, 0), // WEST
    i32v2(1, 0), // EAST
    i32v2(0, 1), // NORTH
};
const int CARTESIAN_EDGEWALK_AXIS[CARTESIAN_COUNT] = {
    AXIS_X, // SOUTH
    AXIS_Y, // WEST
    AXIS_Y, // EAST
    AXIS_X, // NORTH
};
const color4 CARTESIAN_COLORS[CARTESIAN_COUNT] = {
    color4(0, 128, 128, 255), // SOUTH
    color4(128, 0, 128, 255), // WEST
    color4(255, 0, 0, 255), // EAST
    color4(0, 255, 0, 255), // NORTH
};

const AXIS_2D CARTESIAN_TO_AXIS_2D[CARTESIAN_COUNT] = {
    AXIS_VERTICAL,  // DOWN
    AXIS_HORIZONTAL,// LEFT
    AXIS_HORIZONTAL,// RIGHT
    AXIS_VERTICAL   // UP
};