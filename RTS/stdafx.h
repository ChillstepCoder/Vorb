#pragma once

#ifndef stdafx_h__RTS
#define stdafx_h__RTS

//#include <Vorb/stdafx.h>
/************************************************************************/
/* C Libraries                                                          */
/************************************************************************/
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
#include <mutex>
#include <string>
#include <thread>

// TODO: Distribute OpenGL from this location
#include <GL/glew.h>

#include <Vorb/graphics/gtypes.h>
#include <Vorb/VorbPreDecl.inl>

#define UNUSED(x) (void)(x)
#define ENTITY_ID_NONE (ui32)(~0u)

struct b2Vec2;
#define TO_BVEC2(x) reinterpret_cast<b2Vec2&>(x)
#define TO_VVEC2(x) reinterpret_cast<f32v2&>(x)
#define TO_BVEC2_C(x) reinterpret_cast<const b2Vec2&>(x)
#define TO_VVEC2_C(x) reinterpret_cast<const f32v2&>(x)


#endif // stdafx_h__RTS