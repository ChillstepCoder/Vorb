#pragma once

#ifndef stdafx_h__RTS
#define stdafx_h__RTS

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
#include <Vorb/decorators.h>
#include <Vorb/colors.h>

#include <entt/entt.hpp>
constexpr entt::entity INVALID_ENTITY = (entt::null);

// Types
#include "types/IdTypes.h"
#include "util/TypeHash.h"

typedef f64 TimeStampSec;
typedef f64 TimeSpanSec;

// Services
#include "services/Services.h"

extern bool IS_SHUTTING_DOWN;

// **************** Networking *****************
#define NET_SERIALIZE_DECL() \
template <typename Stream> bool netSerialize(Stream& stream);


// Utils
#include "util/CommonUtil.h"
#include "util/MathDefines.h"
#include "util/MathUtil.hpp"
#include "util/AABB.hpp"
#include "util/BitFlags.h"
#include "util/Cartesian.h"
#include "util/CubeFacing.h"
#include "util/ThreadIDs.h"
#include "util/StrToken.h"

#include "instrumentation/instrumentor.h"

// Const
#include "physics/PhysicsConst.h"
#include "world/ChunkConst.h"

// Corner winding
#include "util/Winding.h"

// Tiles
#include "tile/TileConst.h"

// Items
#include "item/ItemConst.h"

// Asset (includes serializer)
#include "resources/IAsset.h"

template <IsAssetType T>
class AssetHandle;

template <IsAssetType T>
using AssetHandlePtr = std::shared_ptr<AssetHandle<T>>;

// **************** Constexpr vectors *****************
#include "math/ConstVectors.h"

// **************** FPS *****************
extern float sFps;

// **************** LOGGING *****************
#include <Vorb/logging/Logger.h>
#include "logging/ErrorLogging.h"

// SPDLog definitions
#include <spdlog/fmt/ostr.h>
template<typename OStream>
OStream& operator<<(OStream& os, const glm::vec2& c)
{
    os << fmt::format("<{},{}>", c.x, c.y);
    return os;
}

extern UNIT_SPACE(SECONDS) f64 sTotalTimeSeconds; ///< Total time since the update/draw loop started.
extern UNIT_SPACE(SECONDS) f32 sElapsedSecondsSinceLastFrame; ///< Elapsed time of the previous frame.

#define BIT(i) (1 << (i))
#define BIT_CAST(i) (BIT(e_cast(i)))

// Thread
inline void setThreadPriorityToMax() {
#ifdef VORB_OS_WINDOWS
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
    struct sched_param params;

    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);
#endif
}

#include "util/panic.h"

// Literals
using namespace std::literals::string_view_literals;

// **************** NEW GRAPHICS API *****************
#include "rendering/gl/GLObjects.h"

#ifdef __GNUC__
#define PACKED_STRUCT __attribute__((packed,aligned(1)))
#else
#define PACKED_STRUCT
#endif

// DEBUGGING GRAPHICS
// We get texture warnings if we bind a null texture. TODO: Why? (Used to bind 0 in shadow mapping)
//#define glBindTexture(x, y) assert(y); glBindTexture(x, y)

// TODO: Remove this by making ryml exist in vorb or removing vorb
#include "serialization/VorbSerializableDefs.h"


// Runs automatically at program startup

#endif // stdafx_h__RTS