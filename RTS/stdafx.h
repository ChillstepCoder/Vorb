#pragma once

#ifndef stdafx_h__RTS
#define stdafx_h__RTS

//#include <Vorb/stdafx.h>
/************************************************************************/
/* C Libraries                                                          */
/************************************************************************/
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
/************************************************************************/
/* Stream libraries                                                     */
/************************************************************************/
#include <fstream>
#include <iostream>
#include <sstream>
/************************************************************************/
/* STL Containers                                                       */
/************************************************************************/
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
/************************************************************************/
/* Other                                                                */
/************************************************************************/
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <assert.h>

// TODO: Distribute OpenGL from this location
#include <GL/glew.h>

#include <Vorb/graphics/gtypes.h>
#include <Vorb/VorbPreDecl.inl>

#include <Vorb/math/VorbMath.hpp>
#include <Vorb/Constants.h>
#include <Vorb/types.h>
#include <Vorb/Timing.h>
#include <Vorb/io/Keg.h>
#include <Vorb/io/Path.h>
#include <vorb/decorators.h>

#include <entt/entt.hpp>

constexpr entt::entity INVALID_ENTITY = (entt::entity)(UINT32_MAX);

#define UNUSED(x) (void)(x)
#define ENTITY_ID_NONE (ui32)(~0u)

struct b2Vec2;
#define TO_BVEC2(x) reinterpret_cast<b2Vec2&>(x)
#define TO_VVEC2(x) reinterpret_cast<f32v2&>(x)
#define TO_BVEC2_C(x) reinterpret_cast<const b2Vec2&>(x)
#define TO_VVEC2_C(x) reinterpret_cast<const f32v2&>(x)

#ifndef _MATH_DEFINES_DEFINED
#define _MATH_DEFINES_DEFINED
// Definitions of useful mathematical constants
//
// Define _USE_MATH_DEFINES before including <math.h> to expose these macro
// definitions for common math constants.  These are placed under an #ifdef
// since these commonly-defined names are not part of the C or C++ standards
#define M_E        2.71828182845904523536f   // e
#define M_LOG2E    1.44269504088896340736f   // log2(e)
#define M_LOG10E   0.434294481903251827651f  // log10(e)
#define M_LN2      0.693147180559945309417f  // ln(2)
#define M_LN10     2.30258509299404568402f   // ln(10)
#define M_PIf       3.14159265358979323846f   // pi
#define M_PI_2f     1.57079632679489661923f   // pi/2
#define M_PI_4f     0.785398163397448309616f  // pi/4
#define M_1_PIf     0.318309886183790671538f  // 1/pi
#define M_2_PIf     0.636619772367581343076f  // 2/pi
#define M_2_SQRTPI 1.12837916709551257390f   // 2/sqrt(pi)
#define M_SQRT2    1.41421356237309504880f   // sqrt(2)
#define M_SQRT1_2  0.707106781186547524401f  // 1/sqrt(2)
#endif

#include "util/MathUtil.hpp"
#include "util/AABB.hpp"

// Comment out for larger chunks
// #define USE_SMALL_CHUNK_WIDTH

#ifdef USE_SMALL_CHUNK_WIDTH
constexpr int CHUNK_WIDTH = 64;
static_assert(CHUNK_WIDTH == 64, "Adjust bitwise operators below");
constexpr float CHUNK_DIAGONAL_RADIUS = 90.51f;
#define TILE_INDEX_Y_SHIFT 6
#define TILE_INDEX_X_MASK 0x3f
#else
constexpr int CHUNK_WIDTH = 128;
static_assert(CHUNK_WIDTH == 128, "Adjust bitwise operators below");
constexpr float CHUNK_DIAGONAL_RADIUS = 181.02f;
#define TILE_INDEX_Y_SHIFT 7
#define TILE_INDEX_X_MASK 0x7f
#endif

constexpr int SUBCHUNK_WIDTH = 16;
constexpr int SUBCHUNK_WIDTH_SQ = SQ(SUBCHUNK_WIDTH);
constexpr int MIN_SUBCHUNKS_PER_CHUNK_ROW = CHUNK_WIDTH / SUBCHUNK_WIDTH;
constexpr int MIN_SUBCHUNKS_PER_CHUNK = SQ(MIN_SUBCHUNKS_PER_CHUNK_ROW);

constexpr int HALF_CHUNK_WIDTH = CHUNK_WIDTH / 2;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_WIDTH;
constexpr ui16 INVALID_TILE_INDEX = 0xffff;

// Enum cast
template<typename E>
constexpr auto enum_cast(E e) -> typename std::underlying_type<E>::type {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

// Cartesian
enum class Cartesian : ui8 {
    DOWN = 0, //-y  south
    LEFT = 1, //-x  west
    RIGHT = 2, //+x east
    UP = 3,  //+y    north
    NONE = 100,
    INVALID = 101
};
constexpr int CARTESIAN_COUNT = 4; 
constexpr Cartesian CARTESIAN_NEIGHBORS[CARTESIAN_COUNT][2] = {
    { Cartesian::LEFT, Cartesian::RIGHT }, // DOWN
    { Cartesian::UP, Cartesian::DOWN }, // LEFT
    { Cartesian::DOWN, Cartesian::UP }, // RIGHT
    { Cartesian::RIGHT, Cartesian::LEFT }, // UP
};
constexpr Cartesian CARTESIAN_OPPOSITES[CARTESIAN_COUNT] = {
    Cartesian::UP,
    Cartesian::RIGHT,
    Cartesian::LEFT,
    Cartesian::DOWN,
};
const i32v2 CARTESIAN_NORMALS[CARTESIAN_COUNT] = {
    i32v2(0, -1), // DOWN
    i32v2(-1, 0), // LEFT
    i32v2(1,  0), // RIGHT
    i32v2(0,  1), // UP
};
const i32v2 CARTESIAN_EDGE_DIRS_ABS[CARTESIAN_COUNT] = {
    i32v2(1, 0), // DOWN
    i32v2(0, 1), // LEFT
    i32v2(0, 1), // RIGHT
    i32v2(1, 0), // UP
};
const i32v2 CARTESIAN_EDGE_DIRS_COUNTER_CLOCKWISE[CARTESIAN_COUNT] = {
    i32v2(1, 0), // DOWN
    i32v2(0, -1), // LEFT
    i32v2(0, 1), // RIGHT
    i32v2(-1, 0), // UP
};
const i32v2 CARTESIAN_EDGE_INDEX_OFFSET_MULTS[CARTESIAN_COUNT] = {
    i32v2(0, 0), // DOWN
    i32v2(0, 0), // LEFT
    i32v2(1, 0), // RIGHT
    i32v2(0, 1), // UP
};
// Corner winding
constexpr int CORNER_COUNT = 4;
enum class CornerWinding {
    BOTTOM_LEFT  = 0,
    BOTTOM_RIGHT = 1,
    TOP_LEFT     = 2,
    TOP_RIGHT    = 3,
    NONE
};
const ui32v2 CORNER_WINDING_OFFSETS[CORNER_COUNT] = {
    ui32v2(0,  0), // BOTTOM_LEFT
    ui32v2(1,  0), // BOTTOM_RIGHT
    ui32v2(0,  1), // TOP_LEFT
    ui32v2(1,  1), // TOP_RIGHT
};

enum AXIS_2D {
    AXIS_HORIZONTAL = 0,
    AXIS_VERTICAL   = 1
};

const AXIS_2D CARTESIAN_TO_AXIS_2D[CARTESIAN_COUNT] = {
    AXIS_VERTICAL,  // DOWN
    AXIS_HORIZONTAL,// LEFT
    AXIS_HORIZONTAL,// RIGHT
    AXIS_VERTICAL   // UP
};

enum AXIS_3D {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2
};

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

const i32v2 CUBE_FACING_AXIS[enum_cast(CubeFacing::COUNT)] = {
    i32v2(AXIS_Y, AXIS_Z), // LEFT
    i32v2(AXIS_X, AXIS_Z),  // FRONT
    i32v2(AXIS_Y, AXIS_Z),  // RIGHT
    i32v2(AXIS_X, AXIS_Z), // BACK
    i32v2(AXIS_X, AXIS_Y),  // TOP
    i32v2(AXIS_X, AXIS_Y)   // BOTTOM
};

const i32v3 CUBE_FACING_NORMALS[enum_cast(CubeFacing::COUNT)] = {
    i32v3(-1, 0, 0), // LEFT
    i32v3(0, -1, 0), // FRONT
    i32v3(1, 0, 0), // RIGHT
    i32v3(0, 1, 0), // BACK
    i32v3(0, 0, 1),  // TOP
    i32v3(0, 0, -1)  // BOTTOM
};

const i32v2 CUBE_FACING_TANGENTS[enum_cast(CubeFacing::COUNT)] = {
    i32v2(1, 0),   // LEFT
    i32v2(-1,  0), // FRONT
    i32v2(-1, 0),  // RIGHT
    i32v2(1, 0),   // BACK
    i32v2(0, 1),   // TOP
    i32v2(0, -1)   // BOTTOM
};

const f32v3 CUBE_FACING_GEOMETRY_OFFSETS[enum_cast(CubeFacing::COUNT)] = {
    f32v3(0, 0, -1.0), // LEFT
    f32v3(0, 0, -1.0), // FRONT
    f32v3(1.0f, 0, -1.0), // RIGHT
    f32v3(0, 1.0f, -1.0), // BACK
    f32v3(0, 0, 0.0f),  // TOP
    f32v3(0, 0, -1.0) // BOTTOM
};

const f32v3 OBJECT_CUBE_FACING_GEOMETRY_OFFSETS[enum_cast(CubeFacing::COUNT)] = {
    f32v3(0, 0, 0.0), // LEFT
    f32v3(0, 0, 0.0), // FRONT
    f32v3(1.0f, 0, 0.0), // RIGHT
    f32v3(0, 1.0f, 0.0), // BACK
    f32v3(0, 0, 1.0f),  // TOP
    f32v3(0, 0, 0.0) // BOTTOM
};


struct TileIndex {
	TileIndex() : index(INVALID_TILE_INDEX) {};
	TileIndex(ui16 index) : index(index) {};
    TileIndex(const TileIndex& index) : index(index.index) {};
	TileIndex(unsigned x, unsigned y) : index((y << TILE_INDEX_Y_SHIFT) + x) {};

	inline ui16 getX() const { return index & TILE_INDEX_X_MASK; }
	inline ui16 getY() const { return index >> TILE_INDEX_Y_SHIFT; }

	operator ui16() const { return index; }

	TileIndex& operator++() {
		++index;
		return *this;
	}
    TileIndex& operator--() {
        --index;
        return *this;
    }

	ui16 index;
};

// Tiles
typedef ui16 TileID;

// Items
typedef ui32 ItemID;
constexpr ui32 INVALID_ITEM_ID = UINT32_MAX;


// **************** FPS *****************
extern float sFps;

// **************** ERRORS *****************
//yes 1, no 0
extern i32 showYesNoBox(const nString& message);
extern i32 showYesNoCancelBox(const nString& message);
extern void showMessage(const nString& message);

extern nString getFullPath(const cString initialDir);
extern void pError(const cString message);
extern void pError(const nString& message);

extern bool checkGlError(const nString& errorLocation);

extern UNIT_SPACE(SECONDS) f64 sTotalTimeSeconds; ///< Total time since the update/draw loop started.
extern UNIT_SPACE(SECONDS) f32 sElapsedSecondsSinceLastFrame; ///< Elapsed time of the previous frame.

// Useful for determining the size of a class pre-compile time
//https://stackoverflow.com/questions/20979565/how-can-i-print-the-result-of-sizeof-at-compile-time-in-c
#define SIZER(type) char (*__kaboom)[sizeof(type)] = 1;

#define IS_ENABLED(d) d == 1

template<int M>
inline bool IsEnabled() {
    return true;
}

template<>
inline bool IsEnabled<0>() {
    return false;
}

const color4 COLOR_WHITE = color4((ui8)255u, (ui8)255u, (ui8)255u, (ui8)255u);


#endif // stdafx_h__RTS