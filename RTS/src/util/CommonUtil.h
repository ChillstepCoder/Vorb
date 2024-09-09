#pragma once

#include <boost/pool/singleton_pool.hpp>
#include <ranges>


// Enum cast
template<typename E>
constexpr auto e_cast(E e) -> typename std::underlying_type<E>::type {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

// Iterate over a range of enum values
// https://stackoverflow.com/questions/69762598/what-are-commonly-used-ways-to-iterate-over-an-enum-class-in-c
constexpr inline auto enum_range = [](auto begin, auto end) {
    return std::views::iota(e_cast(begin), e_cast(end))
        | std::views::transform([](auto e) { return decltype(begin)(e); });
};

#define e_count(e) (e_cast(e::COUNT))

#define KEG_ENUM_STR(e, v) kes_##e##.getValueFromKey(v).c_str()

template<typename E>
constexpr auto keg_enum_str(E e) -> const char * {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

// Unused Parameter
#define UNUSED(x) (void)(x)


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

constexpr f32 MS_PER_SECOND = 1000.0f;
constexpr f32 SECONDS_PER_MS = 0.001f;
constexpr f64 MS_PER_SECOND_D = 1000.0;
constexpr f64 SECONDS_PER_MS_D = 0.001;
constexpr float SECONDS_PER_DAY = 1440.0f;
constexpr float HOURS_PER_DAY = 24.0f;
constexpr float SECONDS_PER_HOUR = SECONDS_PER_DAY / HOURS_PER_DAY;

// Autoinit on startup
#define RUNTIME_INIT_FUNC(name) namespace { struct name { name (); } name##_ins; } name::name()

#define POOLED_ALLOC_DECL() \
    static void* operator new(size_t count); \
    static void operator delete(void* pointer, size_t size);

#define POOLED_ALLOC_DEF_NOT_THREADSAFE(className, initialSize, threadAssert) \
struct className##_pool {}; \
using singleton_##className##_pool = boost::singleton_pool<className##_pool, sizeof(className), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, initialSize>; \
void* className::operator new(size_t count) { \
    threadAssert; \
    UNUSED(count); \
    return singleton_##className##_pool::malloc(); \
} \
void className::operator delete(void* pointer, size_t size) { \
    threadAssert; \
    UNUSED(size); \
    return singleton_##className##_pool::free(pointer); \
}

#define POOLED_ALLOC_DEF_THREADSAFE(className, initialSize) \
struct className##_pool {}; \
using singleton_##className##_pool = boost::singleton_pool<className##_pool, sizeof(className), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, initialSize>; \
void* className::operator new(size_t count) { \
    UNUSED(count); \
    return singleton_##className##_pool::malloc(); \
} \
void className::operator delete(void* pointer, size_t size) { \
    UNUSED(size); \
    return singleton_##className##_pool::free(pointer); \
}

// cast unique ptr to new one
template<typename TO, typename FROM>
inline std::unique_ptr<TO> static_unique_pointer_cast(std::unique_ptr<FROM>&& old) {
    // conversion: unique_ptr<FROM>->FROM*->TO*->unique_ptr<TO>
    return std::unique_ptr<TO>{static_cast<TO*>(old.release())};
}


// Less verbose std::unique_ptr<T[]>
template<typename T>
class UniqueArray {

public:
    UniqueArray() = default;
    explicit UniqueArray(size_t size) : ptr(new T[size]) {}

    VORB_NON_COPYABLE_BUT_MOVABLE(UniqueArray);

    T& operator[](size_t index) {
        return ptr[index];
    }

    const T& operator[](size_t index) const {
        return ptr[index];
    }

    T* get() const {
        return ptr.get();
    }

    T* release() {
        return ptr.release();
    }

    void reset(T* newPtr = nullptr) {
        ptr.reset(newPtr);
    }

    std::unique_ptr<T[]> ptr;
};

/**
 * Literal class type that wraps a constant expression string.
 *
 * Uses implicit conversion to allow templates to *seemingly* accept constant strings.
 */
template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char(&str)[N]) {
        std::copy_n(str, N, value);
    }

    char value[N];
};

#define DECL_BOOL_TEMPLATE(signature, rest) \
template signature<true>rest; \
template signature<false>rest;


// 8 9 10 11
// 4 5 6 7
// 0 1 2 3
// Becomes:
// 2 2 3 3
// 0 0 1 1
// 0 0 1 1
// * IS NOT CORRECT FOR CHUNKS - STRUCTURES ONLY (X,Y,Z)
// * REQUIRED THAT TILE DIMENSIONS ARE A MULTIPLE OF 2
inline DTileIndex structureTileIndexToDTileIndex(TileIndex tileIndex, DTileIndex structureWidthDTiles) {
    // Mathematical simplication doesn't help here because we use integer division, trust me I tried lol
    // This is likely as optimized as it gets without some complex shit so don't waste your time
    const DTileIndex dtileIndex = tileIndex >> 1;
    const DTileIndex rowIndexPlus1 = dtileIndex / structureWidthDTiles + 1;
    return dtileIndex - (rowIndexPlus1 >> 1) * structureWidthDTiles;
}
// * IS NOT CORRECT FOR CHUNKS - STRUCTURES ONLY
// * REQUIRED THAT TILE DIMENSIONS ARE A MULTIPLE OF 2
inline DTileIndex structureTileXYZToDTileIndex(ui32v3 tileXYZ, DTileIndex structureWidthDTiles, DTileIndex floorStrideDTiles) {
    return tileXYZ.z * floorStrideDTiles + (tileXYZ.y << 1) * structureWidthDTiles + (tileXYZ.x << 1);
}
inline DTileIndex structureTileXYToDTileIndex(ui32v2 tileXY, DTileIndex structureWidthDTiles) {
    return (tileXY.y << 1) * structureWidthDTiles + (tileXY.x << 1);
}

// Represents a set of integer coordinate positions sorted by their distance to something
// We dont use a flat_multimap as insert/erase performance is terrible for those
// TODO: Test google btree implementation?
typedef std::multimap<i32 /*distSqInt*/, i32v2> SortedIntCoordDistanceSqMap;

// Returns the element in the map, asserting in debug mode if it isnt there
template<typename Map>
__forceinline auto& assert_at(const Map& m, const typename Map::key_type& key) {
    auto it = m.find(key);
    assert(it != m.end() && "Key not found in map");
    return it->second;
}
