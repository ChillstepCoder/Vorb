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

#endif // stdafx_h__RTS