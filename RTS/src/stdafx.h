#pragma once

#ifndef stdafx_h__RTS
#define stdafx_h__RTS

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
#include <sstream>
/************************************************************************/
/* STL Containers                                                       */
/************************************************************************/
#include <map>
#include <queue>
#include <set>
#include <vector>
/************************************************************************/
/* Other                                                                */
/************************************************************************/
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <assert.h>
#include <filesystem>

namespace fs = std::filesystem;

/************************************************************************/
/* Boost Containers                                                     */
/************************************************************************/
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/circular_buffer.hpp>

template <class Key, class Value, class Compare = std::less<Key>>
using FlatMap = boost::container::flat_map<Key, Value, Compare>;

template <class Key, class Compare = std::less<Key>>
using FlatSet = boost::container::flat_set<Key, Compare>;

template <class Key, class Value, class Hasher = boost::hash<Key>, class KeyEqual = std::equal_to<Key>, class Allocator = std::allocator<std::pair<const Key, Value>>>
using UnorderedFlatMap = boost::unordered_flat_map<Key, Value, Hasher, KeyEqual, Allocator>;

template <class Key, class Hasher = boost::hash<Key>, class KeyEqual = std::equal_to<Key>, class Allocator = std::allocator<Key>>
using UnorderedFlatSet = boost::unordered_flat_set<Key, Hasher, KeyEqual, Allocator>;

// TODO: Distribute OpenGL from this location
#include <GL/glew.h>

#include <Vorb/graphics/gtypes.h>
#include <Vorb/VorbPreDecl.inl>

#include <Vorb/Constants.h>
#include <Vorb/types.h>
#include <Vorb/Timing.h>
#include <Vorb/io/Path.h>
#include <Vorb/decorators.h>
#include <Vorb/colors.h>
#include <Vorb/Event.hpp>
#include <Vorb/utils.h>

// Specializations

// TODO: FILE
#ifndef M_PI
#define M_PI   3.14159265358979323846264338327950288   /* pi */
#endif
#ifndef M_PIF
#define M_PIF   3.14159265358979323846264338327950288f   /* pi */
#endif
#ifndef M_2_PI
#define M_2_PI 6.283185307179586476925286766559 /* 2 * pi */
#endif
#ifndef M_2_PIF
#define M_2_PIF 6.283185307179586476925286766559F /* 2 * pi */
#endif
#ifndef M_3_PI_2F
#define M_3_PI_2F 4.7123889803846898576939650749193   /* 3 * (pi / 2) */
#endif
#ifndef M_4_PI
#define M_4_PI 12.566370614359172953850573533118 /* 4 * pi */
#endif
#ifndef M_4_PIF
#define M_4_PIF 12.566370614359172953850573533118f /* 4 * pi */
#endif
#ifndef M_PI_2
#define M_PI_2 1.5707963267948966192313216916398   /* pi / 2 */
#endif
#ifndef M_PI_2F
#define M_PI_2F 1.5707963267948966192313216916398f   /* pi / 2 */
#endif
#ifndef M_PI_4
#define M_PI_4 0.78539816339744830961566084581988   /* pi / 4 */
#endif
#ifndef M_PI_4F
#define M_PI_4F 0.78539816339744830961566084581988f   /* pi / 4 */
#endif
#ifndef M_G
#define M_G 0.0000000000667384
#endif

// Types
#include "types/IdTypes.h"
#include "util/TypeHash.h"
#include "util/GridIdUtil.h"

#include <entt/entt.hpp>
constexpr entt::entity INVALID_ENTITY = (entt::null);
typedef std::vector<entt::entity> EntityVector;

typedef f64 TimeStampSec;
typedef f64 TimeSpanSec;

// Services
#include "services/Services.h"

extern bool IS_SHUTTING_DOWN;

// ================================ Binary Serialization ================================
//https://github.com/fraillt/bitsery
#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/vector.h>
using BBuffer = std::vector<uint8_t>;
using BOutputAdapter = bitsery::OutputBufferAdapter<BBuffer>;
using BInputAdapter = bitsery::InputBufferAdapter<uint8_t*>;
using BOutputSerializer = bitsery::Serializer<BOutputAdapter>;
using BInputDeserializer = bitsery::Deserializer<BInputAdapter>;


// Usage: s.value2b(myValue) ect...
// See bitsery documentation
// Put this at the BOTTOM of the class definition
#define BINARY_SERIALIZE() \
private: \
  friend bitsery::Access; \
  template <typename S>  \
  void serialize(S& s)

// Must have BINARY_SERIALIZE(); defined first.
#define BINARY_SERIALIZE_INPUT() \
  template <> \
  void serialize(BInputDeserializer& s)

// Must have BINARY_SERIALIZE(); defined first.
#define BINARY_SERIALIZE_OUTPUT() \
  template <> \
  void serialize(BOutputSerializer& s)

// ================================ Networking ================================
#define NET_SERIALIZE_DECL() \
template <typename Stream> bool netSerialize(Stream& stream);


// ================================ LOGGING ================================
#include <Vorb/logging/Logger.h>
#include "logging/ErrorLogging.h"
#include "util/panic.h"

// Tiles
#include "tile/TileConst.h"

// Utils
#include "util/UniqueId64.h"
#include "util/CommonUtil.h"
#include "util/MathDefines.h"
#include "util/MathUtil.hpp"
#include "util/AABB.hpp"
#include "util/BitFlags.h"
#include "util/Cartesian.h"
#include "util/CubeFacing.h"
#include "util/ThreadIDs.h"
#include "util/StrToken.h"
#include "util/StringUtils.h"

// Random
#include "math/Random.h"

// Instrumentation
#include "instrumentation/instrumentor.h"

// Text
#include "text/LocText.h"

// Const
#include "physics/PhysicsConst.h"
#include "world/CoordinateTypes.h"
#include "world/ChunkConst.h"

// Corner winding
#include "util/Winding.h"

// Items
#include "item/ItemConst.h"

// Asset
#include "resources/IAsset.h"
#include "resources/asset/AssetHandle.h"
#include "resources/asset/VariantAssetRef.h"
#include "resources/asset/LiteAssetRef.h"

// ================================ Constexpr vectors ================================
#include "math/ConstVectors.h"

// ================================ FPS ================================
extern float sFps;

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
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

inline void setThreadPriorityToAboveNormal() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

// Literals
using namespace std::literals::string_view_literals;

// ================================ NEW GRAPHICS API ================================
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
#include "serialization/YmlSerializer.h"
#include "serialization/VorbSerializableDefs.h"
#include "serialization/CommonSerializable.h"
// Runs automatically at program startup

// Serializable stuff
#include "tile/TileType.h"

// Assets
#include "resources/IAssetRepository.h"

#endif // stdafx_h__RTS