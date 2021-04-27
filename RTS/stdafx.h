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

// Comment out for larger chunks
// #define USE_SMALL_CHUNK_WIDTH

#ifdef USE_SMALL_CHUNK_WIDTH
constexpr int CHUNK_WIDTH = 64;
static_assert(CHUNK_WIDTH == 64, "Adjust bitwise operators below");
#define TILE_INDEX_Y_SHIFT 6
#define TILE_INDEX_X_MASK 0x3f
#else
constexpr int CHUNK_WIDTH = 128;
static_assert(CHUNK_WIDTH == 128, "Adjust bitwise operators below");
#define TILE_INDEX_Y_SHIFT 7
#define TILE_INDEX_X_MASK 0x7f
#endif

constexpr int HALF_CHUNK_WIDTH = CHUNK_WIDTH / 2;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_WIDTH;
constexpr ui16 INVALID_TILE_INDEX = 0xffff;

// Cartesian
enum class Cartesian {
    DOWN = 0, //-y  south
    LEFT = 1, //-x  west
    RIGHT = 2, //+x east
    UP = 3,  //+y    north
    NONE = 100
};
constexpr int CARTESIAN_COUNT = 4; 
constexpr Cartesian CARTESIAN_OPPOSITES[CARTESIAN_COUNT] = {
    Cartesian::UP,
    Cartesian::RIGHT,
    Cartesian::LEFT,
    Cartesian::DOWN,
};
const i32v2 CARTESIAN_OFFSETS[CARTESIAN_COUNT] = {
    i32v2(0, -1), // DOWN
    i32v2(-1, 0), // LEFT
    i32v2(1,  0), // RIGHT
    i32v2(0,  1), // UP
};
// Corner winding
constexpr int CORNER_COUNT = 4;
enum class CornerWinding {
    BOTTOM_LEFT  = 0,
    BOTTOM_RIGHT = 1,
    TOP_LEFT     = 2,
    TOP_RIGHT    = 3
};
const ui32v2 CORNER_WINDING_OFFSETS[CORNER_COUNT] = {
    ui32v2(0,  0), // BOTTOM_LEFT
    ui32v2(1,  0), // BOTTOM_RIGHT
    ui32v2(0,  1), // TOP_LEFT
    ui32v2(1,  1), // TOP_RIGHT
};


enum AXIS {
    AXIS_HORIZONTAL = 0,
    AXIS_VERTICAL   = 1
};

// TODO: Remove
// We are in 3/4 perspective
constexpr float Z_TO_XY_RATIO = 0.75f;

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

template<typename E>
constexpr auto enum_cast(E e) -> typename std::underlying_type<E>::type {
	return static_cast<typename std::underlying_type<E>::type>(e);
}

// **************** BEBUG *****************
extern bool s_debugToggle;
extern bool s_wasTogglePressed;
extern float sFps;

struct DebugOptions {
	f64 mTimeOffset = 0.0f;
    bool mWireframe = false;
    bool mChunkBoundaries = false;
	bool mCities = false;
};

extern DebugOptions sDebugOptions;

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
template <size_t S> class Sizer { };
#define SIZER(type) Sizer<sizeof(type)> 


#define IS_ENABLED(d) d == 1

#endif // stdafx_h__RTS